#include <ToshiPCH.h>
#include "AStreamWave_PSP.h"

#include "ASoundManager_PSP.h"

#include <File/TFile.h>
#include <Toshi/TUtil.h>

AStreamPrefetchThread_PSP::AStreamPrefetchThread_PSP()
    : m_pStreamWave( TNULL )
{
}

void AStreamPrefetchThread_PSP::Main()
{
	if ( m_pStreamWave != TNULL )
		m_pStreamWave->RunPrefetchThread();
}

AStreamWave_PSP::AStreamWave_PSP()
    : m_oStateMutex()
    , m_oPrefetchRequested()
    , m_oPrefetchStopped()
    , m_oPrefetchThread()
    , m_pFile( TNULL )
    , m_oHeader{}
    , m_eFormat( StreamFormat_None )
    , m_uiDataOffset( 0 )
    , m_uiBlockBytes( 0 )
    , m_aPages{}
    , m_aCompressedBlock{}
    , m_uiCurrentPage( 0 )
    , m_uiPendingStart( INVALID_SAMPLE_FRAME )
    , m_uiLoadingStart( INVALID_SAMPLE_FRAME )
    , m_uiUnderrunCount( 0 )
    , m_bPrefetchRunning( TFALSE )
    , m_bSyncReady( TFALSE )
{
	m_oPrefetchThread.SetStreamWave( this );
}

AStreamWave_PSP::~AStreamWave_PSP()
{
	Destroy();
}

TBOOL AStreamWave_PSP::Create( const TCHAR* a_pFilePath )
{
	Destroy();
	if ( a_pFilePath == TNULL )
		return TFALSE;

	Toshi::TFile* pFile = Toshi::TFile::Create(
	    a_pFilePath,
	    Toshi::TFILEMODE_READ | Toshi::TFILEMODE_NOBUFFER
	);
	if ( pFile == TNULL )
		return TFALSE;

	TUINT32 uiMagic = 0;
	if ( pFile->Read( &uiMagic, sizeof( uiMagic ) ) != sizeof( uiMagic ) ||
	     !pFile->Seek( 0, Toshi::TSEEK_SET ) )
	{
		pFile->Destroy();
		return TFALSE;
	}

	m_pFile = pFile;
	if ( uiMagic == AWaveDataHeader_PSP::MAGIC )
	{
		AWaveDataHeader_PSP oHeader = {};
		const TBOOL bValid = pFile->Read( &oHeader, sizeof( oHeader ) ) == sizeof( oHeader ) &&
		                     oHeader.uiVersion == AWaveDataHeader_PSP::VERSION &&
		                     oHeader.uiSampleRate == ASoundManager_PSP::SAMPLE_RATE &&
		                     oHeader.uiChannels == AWave_PSP::OUTPUT_CHANNELS &&
		                     oHeader.uiSampleFrames > AWave_PSP::MAX_SAMPLE_FRAMES &&
		                     oHeader.uiSampleFrames <= MAX_STREAM_SAMPLE_FRAMES &&
		                     pFile->GetSize() == sizeof( oHeader ) +
		                                             oHeader.uiSampleFrames *
		                                                 AWave_PSP::OUTPUT_CHANNELS * sizeof( TINT16 );
		if ( !bValid )
		{
			Destroy();
			return TFALSE;
		}
		m_oHeader = oHeader;
		m_eFormat = StreamFormat_PCM;
		m_uiDataOffset = sizeof( oHeader );
	}
	else if ( uiMagic == AADPCMStreamHeader_PSP::MAGIC )
	{
		AADPCMStreamHeader_PSP oHeader = {};
		const TBOOL bValid = pFile->Read( &oHeader, sizeof( oHeader ) ) == sizeof( oHeader ) &&
		                     oHeader.uiVersion == AADPCMStreamHeader_PSP::VERSION &&
		                     oHeader.uiSampleRate == ASoundManager_PSP::SAMPLE_RATE &&
		                     oHeader.uiChannels == AWave_PSP::OUTPUT_CHANNELS &&
		                     oHeader.uiSampleFrames > AWave_PSP::MAX_SAMPLE_FRAMES &&
		                     oHeader.uiSampleFrames <= MAX_STREAM_SAMPLE_FRAMES &&
		                     oHeader.uiBlockSampleFrames == AADPCMStreamHeader_PSP::BLOCK_SAMPLE_FRAMES &&
		                     oHeader.uiBlockBytes == AADPCMStreamHeader_PSP::BLOCK_BYTES &&
		                     oHeader.uiBlockCount ==
		                         ( oHeader.uiSampleFrames + CACHE_SAMPLE_FRAMES - 1 ) /
		                             CACHE_SAMPLE_FRAMES &&
		                     pFile->GetSize() == sizeof( oHeader ) +
		                                             oHeader.uiBlockCount * oHeader.uiBlockBytes;
		if ( !bValid )
		{
			Destroy();
			return TFALSE;
		}
		m_oHeader = {
			AWaveDataHeader_PSP::MAGIC,
			AWaveDataHeader_PSP::VERSION,
			oHeader.uiSampleRate,
			oHeader.uiChannels,
			oHeader.uiSampleFrames,
		};
		m_eFormat = StreamFormat_ADPCM;
		m_uiDataOffset = sizeof( oHeader );
		m_uiBlockBytes = oHeader.uiBlockBytes;
	}
	else
	{
		Destroy();
		return TFALSE;
	}

	if ( !ReadPage( 0, 0 ) ||
	     !m_oStateMutex.Create() ||
	     !m_oPrefetchRequested.Create( 0, 1 ) ||
	     !m_oPrefetchStopped.Create( 0, 1 ) )
	{
		Destroy();
		return TFALSE;
	}
	m_aPages[ 0 ].bReady = TTRUE;

	m_bSyncReady = TTRUE;
	m_bPrefetchRunning = TTRUE;
	if ( !m_oPrefetchThread.Create(
	         0x2000,
	         Toshi::TThread::THREAD_PRIORITY_BELOW_NORMAL,
	         Toshi::TThread::FLAGS_NONE
	     ) )
	{
		Destroy();
		return TFALSE;
	}

	RequestPrefetch( CACHE_SAMPLE_FRAMES );
	return TTRUE;
}

void AStreamWave_PSP::Destroy()
{
	if ( m_bPrefetchRunning )
	{
		m_bPrefetchRunning = TFALSE;
		if ( m_oPrefetchThread.m_iThreadID >= 0 )
		{
			m_oPrefetchRequested.Signal();
			m_oPrefetchStopped.Wait();
		}
	}

	if ( m_bSyncReady )
	{
		m_oPrefetchStopped.Destroy();
		m_oPrefetchRequested.Destroy();
		m_oStateMutex.Destroy();
	}
	m_bSyncReady = TFALSE;

	if ( m_pFile != TNULL )
		m_pFile->Destroy();
	m_pFile = TNULL;
	m_oHeader = {};
	m_eFormat = StreamFormat_None;
	m_uiDataOffset = 0;
	m_uiBlockBytes = 0;
	m_uiCurrentPage = 0;
	m_uiPendingStart = INVALID_SAMPLE_FRAME;
	m_uiLoadingStart = INVALID_SAMPLE_FRAME;
	m_uiUnderrunCount = 0;
	Toshi::TUtil::MemClear( m_aPages, sizeof( m_aPages ) );
	Toshi::TUtil::MemClear( m_aCompressedBlock, sizeof( m_aCompressedBlock ) );
}

TBOOL AStreamWave_PSP::Prepare( TUINT a_uiStartSampleFrame, TBOOL a_bLoop )
{
	if ( !IsCreated() || a_uiStartSampleFrame >= m_oHeader.uiSampleFrames )
		return TFALSE;

	m_oStateMutex.Lock();
	CachePage& rCurrentPage = m_aPages[ m_uiCurrentPage ];
	if ( rCurrentPage.bReady &&
	     a_uiStartSampleFrame >= rCurrentPage.uiStart &&
	     a_uiStartSampleFrame < rCurrentPage.uiStart + rCurrentPage.uiFrames )
	{
		m_oStateMutex.Unlock();
		return TTRUE;
	}

	const TUINT uiNextPage = ( m_uiCurrentPage + 1 ) % CACHE_PAGE_COUNT;
	CachePage& rNextPage = m_aPages[ uiNextPage ];
	if ( !rNextPage.bReady ||
	     a_uiStartSampleFrame < rNextPage.uiStart ||
	     a_uiStartSampleFrame >= rNextPage.uiStart + rNextPage.uiFrames )
	{
		m_uiUnderrunCount++;
		m_oStateMutex.Unlock();
		return TFALSE;
	}

	m_uiCurrentPage = uiNextPage;
	const TUINT uiPrefetchStart = rNextPage.uiStart + rNextPage.uiFrames;
	m_oStateMutex.Unlock();
	if ( uiPrefetchStart < m_oHeader.uiSampleFrames )
		RequestPrefetch( uiPrefetchStart );
	else if ( a_bLoop )
		RequestPrefetch( 0 );
	return TTRUE;
}

TBOOL AStreamWave_PSP::ReadPage( TUINT a_uiStartSampleFrame, TUINT a_uiPage )
{
	if ( m_pFile == TNULL || a_uiPage >= CACHE_PAGE_COUNT ||
	     a_uiStartSampleFrame >= m_oHeader.uiSampleFrames )
	{
		return TFALSE;
	}

	const TUINT uiRemainingFrames = m_oHeader.uiSampleFrames - a_uiStartSampleFrame;
	const TUINT uiFramesToRead = uiRemainingFrames < CACHE_SAMPLE_FRAMES ?
	                               uiRemainingFrames : CACHE_SAMPLE_FRAMES;
	TBOOL bRead = TFALSE;
	if ( m_eFormat == StreamFormat_PCM )
	{
		const TSIZE uiBytesToRead =
		    uiFramesToRead * AWave_PSP::OUTPUT_CHANNELS * sizeof( TINT16 );
		const TINT iFileOffset = TINT(
		    m_uiDataOffset +
		    a_uiStartSampleFrame * AWave_PSP::OUTPUT_CHANNELS * sizeof( TINT16 )
		);
		bRead = m_pFile->Seek( iFileOffset, Toshi::TSEEK_SET ) &&
		        m_pFile->Read( m_aPages[ a_uiPage ].aSamples, uiBytesToRead ) == uiBytesToRead;
	}
	else if ( m_eFormat == StreamFormat_ADPCM &&
	          a_uiStartSampleFrame % CACHE_SAMPLE_FRAMES == 0 )
	{
		const TUINT uiBlock = a_uiStartSampleFrame / CACHE_SAMPLE_FRAMES;
		const TINT iFileOffset = TINT( m_uiDataOffset + uiBlock * m_uiBlockBytes );
		bRead = m_pFile->Seek( iFileOffset, Toshi::TSEEK_SET ) &&
		        m_pFile->Read( m_aCompressedBlock, m_uiBlockBytes ) == m_uiBlockBytes &&
		        AADPCMDecoder_PSP::DecodeBlock(
		            m_aCompressedBlock,
		            m_uiBlockBytes,
		            m_aPages[ a_uiPage ].aSamples,
		            uiFramesToRead
		        );
	}

	if ( !bRead )
	{
		return TFALSE;
	}

	m_aPages[ a_uiPage ].uiStart = a_uiStartSampleFrame;
	m_aPages[ a_uiPage ].uiFrames = uiFramesToRead;
	return TTRUE;
}

const TINT16* AStreamWave_PSP::GetFrame( TUINT a_uiSampleFrame ) const
{
	const CachePage& rCurrentPage = m_aPages[ m_uiCurrentPage ];
	if ( !rCurrentPage.bReady ||
	     a_uiSampleFrame < rCurrentPage.uiStart ||
	     a_uiSampleFrame >= rCurrentPage.uiStart + rCurrentPage.uiFrames )
	{
		return TNULL;
	}

	return rCurrentPage.aSamples +
	       ( a_uiSampleFrame - rCurrentPage.uiStart ) * AWave_PSP::OUTPUT_CHANNELS;
}

void AStreamWave_PSP::RequestPrefetch( TUINT a_uiStartSampleFrame )
{
	if ( !m_bPrefetchRunning || a_uiStartSampleFrame >= m_oHeader.uiSampleFrames )
		return;

	m_oStateMutex.Lock();
	if ( m_uiPendingStart == INVALID_SAMPLE_FRAME &&
	     m_uiLoadingStart != a_uiStartSampleFrame )
	{
		m_uiPendingStart = a_uiStartSampleFrame;
		m_oStateMutex.Unlock();
		m_oPrefetchRequested.Signal();
		return;
	}
	m_oStateMutex.Unlock();
}

void AStreamWave_PSP::RunPrefetchThread()
{
	while ( m_oPrefetchRequested.Wait() )
	{
		if ( !m_bPrefetchRunning )
			break;

		m_oStateMutex.Lock();
		const TUINT uiStart = m_uiPendingStart;
		m_uiPendingStart = INVALID_SAMPLE_FRAME;
		m_uiLoadingStart = uiStart;
		const TUINT uiPage = ( m_uiCurrentPage + 1 ) % CACHE_PAGE_COUNT;
		m_aPages[ uiPage ].bReady = TFALSE;
		m_oStateMutex.Unlock();

		const TBOOL bRead = uiStart != INVALID_SAMPLE_FRAME && ReadPage( uiStart, uiPage );

		m_oStateMutex.Lock();
		m_aPages[ uiPage ].bReady = bRead;
		m_uiLoadingStart = INVALID_SAMPLE_FRAME;
		m_oStateMutex.Unlock();
	}

	m_oPrefetchStopped.Signal();
}
