#include <ToshiPCH.h>
#include "ASoundManager_PSP.h"
#include "ASoundCommandQueue_PSP.h"
#include "AWaveBank_PSP.h"

#include <Toshi/TUtil.h>

#include <pspaudio.h>

struct ASoundCueMapping_PSP
{
	ASoundId_PSP eSoundID;
	const TCHAR* pWaveName;
};

static const ASoundCueMapping_PSP g_aSoundCueMappings_PSP[] = {
	{ ASoundId_PSP_UI_MENUCLICK, "UI_MENUCLICK" },
	{ ASoundId_PSP_UI_MENUBACK, "UI_MENUBACK" },
	{ ASoundId_PSP_UI_MENUOK, "UI_MENUOK" },
};

ASoundOutputThread_PSP::ASoundOutputThread_PSP()
    : m_pSoundManager( TNULL )
{
}

void ASoundOutputThread_PSP::Main()
{
	if ( m_pSoundManager != TNULL )
		m_pSoundManager->RunOutputThread();
}

ASoundManager_PSP::ASoundManager_PSP()
    : m_FreeBuffers()
    , m_ReadyBuffers()
    , m_CompletedBuffers()
    , m_ThreadStopped()
    , m_OutputThread()
    , m_aOutputBuffers{}
    , m_iChannel( -1 )
    , m_bRunning( TFALSE )
    , m_bOutputHealthy( TFALSE )
    , m_bOutputLocked( TFALSE )
    , m_uiWriteBuffer( 0 )
    , m_uiReadBuffer( 0 )
    , m_pWaveBank( TNULL )
    , m_pCommandQueue( TNULL )
    , m_uiRejectedCues( 0 )
{
	m_OutputThread.SetSoundManager( this );
}

ASoundManager_PSP::~ASoundManager_PSP()
{
	Destroy();
}

TBOOL ASoundManager_PSP::Create()
{
	Destroy();
	m_iChannel = sceAudioChReserve(
	    PSP_AUDIO_NEXT_CHANNEL,
	    BUFFER_SAMPLE_FRAMES,
	    PSP_AUDIO_FORMAT_STEREO
	);
	if ( m_iChannel < 0 )
		return TFALSE;

	if ( !m_FreeBuffers.Create( OUTPUT_BUFFER_COUNT, OUTPUT_BUFFER_COUNT ) ||
	     !m_ReadyBuffers.Create( 0, OUTPUT_BUFFER_COUNT + 1 ) ||
	     !m_CompletedBuffers.Create( 0, OUTPUT_BUFFER_COUNT ) ||
	     !m_ThreadStopped.Create( 0, 1 ) )
	{
		Destroy();
		return TFALSE;
	}

	m_bRunning = TTRUE;
	m_bOutputHealthy = TTRUE;
	m_bOutputLocked = TFALSE;
	m_uiWriteBuffer = 0;
	m_uiReadBuffer = 0;
	if ( !m_OutputThread.Create( 0x2000, Toshi::TThread::THREAD_PRIORITY_TIME_CRITICAL, Toshi::TThread::FLAGS_NONE ) )
	{
		Destroy();
		return TFALSE;
	}

	return TTRUE;
}

void ASoundManager_PSP::Destroy()
{
	UnbindCueBank();
	if ( m_bRunning )
	{
		m_bRunning = TFALSE;
		if ( m_OutputThread.m_iThreadID >= 0 )
		{
			m_ReadyBuffers.Signal();
			m_ThreadStopped.Wait();
		}
	}

	if ( m_iChannel >= 0 )
		sceAudioChRelease( m_iChannel );

	m_ThreadStopped.Destroy();
	m_CompletedBuffers.Destroy();
	m_ReadyBuffers.Destroy();
	m_FreeBuffers.Destroy();
	m_iChannel = -1;
	m_bOutputHealthy = TFALSE;
	m_bOutputLocked = TFALSE;
	m_uiWriteBuffer = 0;
	m_uiReadBuffer = 0;
}

TBOOL ASoundManager_PSP::BindCueBank(
    const AWaveBank_PSP* a_pWaveBank,
    ASoundCommandQueue_PSP* a_pCommandQueue
)
{
	UnbindCueBank();
	if ( !IsCreated() || a_pWaveBank == TNULL || a_pCommandQueue == TNULL )
		return TFALSE;

	for ( TSIZE i = 0; i < TARRAYSIZE( g_aSoundCueMappings_PSP ); i++ )
	{
		if ( a_pWaveBank->FindWave( g_aSoundCueMappings_PSP[ i ].pWaveName ) == TNULL )
			return TFALSE;
	}

	m_pWaveBank = a_pWaveBank;
	m_pCommandQueue = a_pCommandQueue;
	return TTRUE;
}

void ASoundManager_PSP::UnbindCueBank()
{
	m_pWaveBank = TNULL;
	m_pCommandQueue = TNULL;
	m_uiRejectedCues = 0;
}

TINT ASoundManager_PSP::PlayCue( ASoundId_PSP a_eSoundID )
{
	const TCHAR* pWaveName = ResolveCueName( a_eSoundID );
	if ( !IsCueBankBound() || pWaveName == TNULL ||
	     !m_pCommandQueue->QueuePlayDetachedNamed( *m_pWaveBank, pWaveName ) )
	{
		m_uiRejectedCues++;
		return -1;
	}

	return 0;
}

const TCHAR* ASoundManager_PSP::ResolveCueName( ASoundId_PSP a_eSoundID ) const
{
	for ( TSIZE i = 0; i < TARRAYSIZE( g_aSoundCueMappings_PSP ); i++ )
	{
		if ( g_aSoundCueMappings_PSP[ i ].eSoundID == a_eSoundID )
			return g_aSoundCueMappings_PSP[ i ].pWaveName;
	}

	return TNULL;
}

TBOOL ASoundManager_PSP::SubmitStereo( const TINT16* a_pSamples, TUINT a_uiSampleFrames )
{
	if ( !IsCreated() || a_pSamples == TNULL || a_uiSampleFrames != BUFFER_SAMPLE_FRAMES )
		return TFALSE;

	TINT16* pOutput = LockOutputBuffer();
	if ( pOutput == TNULL )
		return TFALSE;

	Toshi::TUtil::MemCopy(
	    pOutput,
	    a_pSamples,
	    BUFFER_SAMPLE_FRAMES * OUTPUT_CHANNELS * sizeof( TINT16 )
	);
	return UnlockOutputBuffer();
}

TBOOL ASoundManager_PSP::SubmitSilence()
{
	if ( !IsCreated() )
		return TFALSE;

	TINT16* pOutput = LockOutputBuffer();
	if ( pOutput == TNULL )
		return TFALSE;

	Toshi::TUtil::MemClear(
	    pOutput,
	    BUFFER_SAMPLE_FRAMES * OUTPUT_CHANNELS * sizeof( TINT16 )
	);
	return UnlockOutputBuffer();
}

TBOOL ASoundManager_PSP::WaitForSubmittedBuffer()
{
	return IsCreated() && m_CompletedBuffers.Wait() && m_bOutputHealthy;
}

TINT16* ASoundManager_PSP::LockOutputBuffer()
{
	if ( !IsCreated() || !m_bRunning || m_bOutputLocked || !m_FreeBuffers.Wait() )
		return TNULL;

	m_bOutputLocked = TTRUE;
	return m_aOutputBuffers[ m_uiWriteBuffer ];
}

TINT16* ASoundManager_PSP::TryLockOutputBuffer()
{
	if ( !IsCreated() || !m_bRunning || m_bOutputLocked || !m_FreeBuffers.Poll() )
		return TNULL;

	m_bOutputLocked = TTRUE;
	return m_aOutputBuffers[ m_uiWriteBuffer ];
}

TBOOL ASoundManager_PSP::UnlockOutputBuffer()
{
	if ( !m_bOutputLocked )
		return TFALSE;

	m_bOutputLocked = TFALSE;
	m_uiWriteBuffer = ( m_uiWriteBuffer + 1 ) % OUTPUT_BUFFER_COUNT;
	return m_ReadyBuffers.Signal();
}

void ASoundManager_PSP::RunOutputThread()
{
	while ( m_ReadyBuffers.Wait() )
	{
		if ( !m_bRunning )
			break;

		TINT16* pOutput = m_aOutputBuffers[ m_uiReadBuffer ];
		if ( sceAudioOutputBlocking( m_iChannel, PSP_AUDIO_VOLUME_MAX, pOutput ) < 0 )
			m_bOutputHealthy = TFALSE;

		m_uiReadBuffer = ( m_uiReadBuffer + 1 ) % OUTPUT_BUFFER_COUNT;
		m_FreeBuffers.Signal();
		m_CompletedBuffers.Signal();
	}

	m_ThreadStopped.Signal();
}
