#include <ToshiPCH.h>
#include "ASoundManager_PSP.h"

#include <Toshi/TUtil.h>

#include <pspaudio.h>

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
