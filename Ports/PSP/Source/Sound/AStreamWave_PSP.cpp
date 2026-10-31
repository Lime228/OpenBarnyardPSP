#include <ToshiPCH.h>
#include "AStreamWave_PSP.h"

#include "ASoundManager_PSP.h"

#include <File/TFile.h>
#include <Toshi/TUtil.h>

AStreamWave_PSP::AStreamWave_PSP()
    : m_pFile( TNULL )
    , m_oHeader{}
    , m_uiBufferStart( 0 )
    , m_uiBufferedFrames( 0 )
    , m_aSamples{}
{
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

	AWaveDataHeader_PSP oHeader = {};
	const TBOOL bValid = pFile->Read( &oHeader, sizeof( oHeader ) ) == sizeof( oHeader ) &&
	                     oHeader.uiMagic == AWaveDataHeader_PSP::MAGIC &&
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
		pFile->Destroy();
		return TFALSE;
	}

	m_pFile = pFile;
	m_oHeader = oHeader;
	return TTRUE;
}

void AStreamWave_PSP::Destroy()
{
	if ( m_pFile != TNULL )
		m_pFile->Destroy();
	m_pFile = TNULL;
	m_oHeader = {};
	m_uiBufferStart = 0;
	m_uiBufferedFrames = 0;
	Toshi::TUtil::MemClear( m_aSamples, sizeof( m_aSamples ) );
}

TBOOL AStreamWave_PSP::Prepare( TUINT a_uiStartSampleFrame )
{
	if ( !IsCreated() || a_uiStartSampleFrame >= m_oHeader.uiSampleFrames )
		return TFALSE;
	if ( a_uiStartSampleFrame >= m_uiBufferStart &&
	     a_uiStartSampleFrame < m_uiBufferStart + m_uiBufferedFrames )
	{
		return TTRUE;
	}

	const TUINT uiRemainingFrames = m_oHeader.uiSampleFrames - a_uiStartSampleFrame;
	const TUINT uiFramesToRead = uiRemainingFrames < CACHE_SAMPLE_FRAMES ?
	                               uiRemainingFrames : CACHE_SAMPLE_FRAMES;
	const TSIZE uiBytesToRead = uiFramesToRead * AWave_PSP::OUTPUT_CHANNELS * sizeof( TINT16 );
	const TINT iFileOffset = TINT(
	    sizeof( AWaveDataHeader_PSP ) +
	    a_uiStartSampleFrame * AWave_PSP::OUTPUT_CHANNELS * sizeof( TINT16 )
	);
	if ( !m_pFile->Seek( iFileOffset, Toshi::TSEEK_SET ) ||
	     m_pFile->Read( m_aSamples, uiBytesToRead ) != uiBytesToRead )
	{
		m_uiBufferedFrames = 0;
		return TFALSE;
	}

	m_uiBufferStart = a_uiStartSampleFrame;
	m_uiBufferedFrames = uiFramesToRead;
	return TTRUE;
}

const TINT16* AStreamWave_PSP::GetFrame( TUINT a_uiSampleFrame ) const
{
	if ( a_uiSampleFrame < m_uiBufferStart ||
	     a_uiSampleFrame >= m_uiBufferStart + m_uiBufferedFrames )
	{
		return TNULL;
	}

	return m_aSamples +
	       ( a_uiSampleFrame - m_uiBufferStart ) * AWave_PSP::OUTPUT_CHANNELS;
}
