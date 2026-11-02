#include <ToshiPCH.h>
#include <Math/TMathInline.h>
#include <Math/TVector4.h>
#include "ASoundManager_PSP.h"
#include "ASoundCommandQueue_PSP.h"
#include "ASoundMixer_PSP.h"
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

static TINT ConvertCueCoordinate_PSP( TFLOAT a_fCoordinate )
{
	const TFLOAT fScaled = a_fCoordinate * TFLOAT( ASoundManager_PSP::CUE_POSITION_SCALE );
	return TINT( fScaled + ( fScaled >= 0.0f ? 0.5f : -0.5f ) );
}

static TBOOL IsCuePositionValid_PSP( const Toshi::TVector4& a_rcPosition )
{
	const TFLOAT fMaxCoordinate = TFLOAT( ASoundMixer_PSP::MAX_POSITION ) /
	                              TFLOAT( ASoundManager_PSP::CUE_POSITION_SCALE );
	return a_rcPosition.x >= -fMaxCoordinate && a_rcPosition.x <= fMaxCoordinate &&
	       a_rcPosition.y >= -fMaxCoordinate && a_rcPosition.y <= fMaxCoordinate &&
	       a_rcPosition.z >= -fMaxCoordinate && a_rcPosition.z <= fMaxCoordinate;
}

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
    , m_pDelayedEventQueue( TNULL )
    , m_aCues{}
    , m_uiManagedCues( 0 )
    , m_uiPendingCues( 0 )
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
    ASoundCommandQueue_PSP* a_pCommandQueue,
    ASoundDelayedEventQueue_PSP* a_pDelayedEventQueue
)
{
	UnbindCueBank();
	if ( !IsCreated() || a_pWaveBank == TNULL || a_pCommandQueue == TNULL ||
	     a_pDelayedEventQueue == TNULL )
		return TFALSE;

	for ( TSIZE i = 0; i < TARRAYSIZE( g_aSoundCueMappings_PSP ); i++ )
	{
		if ( a_pWaveBank->FindWave( g_aSoundCueMappings_PSP[ i ].pWaveName ) == TNULL )
			return TFALSE;
	}

	m_pWaveBank = a_pWaveBank;
	m_pCommandQueue = a_pCommandQueue;
	m_pDelayedEventQueue = a_pDelayedEventQueue;
	return TTRUE;
}

void ASoundManager_PSP::UnbindCueBank()
{
	ClearCues();
	m_pWaveBank = TNULL;
	m_pCommandQueue = TNULL;
	m_pDelayedEventQueue = TNULL;
	m_uiRejectedCues = 0;
}

TINT ASoundManager_PSP::PlayCue( ASoundId_PSP a_eSoundID )
{
	return PlayCueEx( a_eSoundID, 1.0f, TFALSE, 0.0f, -1 );
}

TINT ASoundManager_PSP::PlayCueEx(
    ASoundId_PSP a_eSoundID,
    TFLOAT a_fVolume,
    TBOOL a_bStartPaused,
    TFLOAT a_fDelay,
    TINT a_iTrack
)
{
	const TCHAR* pWaveName = ResolveCueName( a_eSoundID );
	if ( !IsCueBankBound() || pWaveName == TNULL ||
	     !( a_fVolume >= 0.0f && a_fVolume <= 1.0f ) ||
	     !( a_fDelay >= 0.0f && a_fDelay <= MAX_CUE_DELAY_SECONDS ) ||
	     a_iTrack != -1 )
	{
		m_uiRejectedCues++;
		return -1;
	}

	TUINT uiCue = MAX_MANAGED_CUES;
	for ( TUINT i = 0; i < MAX_MANAGED_CUES; i++ )
	{
		if ( m_aCues[ i ].eState == CueState_Free )
		{
			uiCue = i;
			break;
		}
	}
	if ( uiCue == MAX_MANAGED_CUES )
	{
		m_uiRejectedCues++;
		return -1;
	}

	const TUINT uiVolume = TUINT(
	    a_fVolume * TFLOAT( ASoundMixer_PSP::MAX_VOLUME ) + 0.5f
	);
	const TUINT uiDelaySampleFrames = TUINT(
	    a_fDelay * TFLOAT( SAMPLE_RATE ) + 0.5f
	);
	ASoundCommandRequest_PSP uiRequest = ASoundCommandQueue_PSP::INVALID_REQUEST;
	ASoundScheduledEventHandle_PSP hScheduledEvent = ASoundDelayedEventQueue_PSP::INVALID_EVENT;
	if ( uiDelaySampleFrames == 0 )
	{
		uiRequest = m_pCommandQueue->QueuePlayNamed(
		    *m_pWaveBank,
		    pWaveName,
		    uiVolume
		);
	}
	else
	{
		const ASoundEvent_PSP oEvent = {
			ASoundEventType_PSP_PlayAudio,
			pWaveName,
			ASoundMixer_PSP::INVALID_VOICE,
			uiVolume,
			0,
			TFALSE,
			ASoundMixer_PSP::DEFAULT_FREQUENCY,
			{},
		};
		hScheduledEvent = m_pDelayedEventQueue->Schedule(
		    *m_pWaveBank,
		    oEvent,
		    uiDelaySampleFrames
		);
	}
	if ( uiRequest == ASoundCommandQueue_PSP::INVALID_REQUEST &&
	     hScheduledEvent == ASoundDelayedEventQueue_PSP::INVALID_EVENT )
	{
		m_uiRejectedCues++;
		return -1;
	}

	Cue& rCue = m_aCues[ uiCue ];
	rCue.uiGeneration = ( rCue.uiGeneration + 1 ) & CUE_GENERATION_MASK;
	if ( rCue.uiGeneration == 0 )
		rCue.uiGeneration = 1;
	rCue.eState = uiRequest != ASoundCommandQueue_PSP::INVALID_REQUEST ?
	                    CueState_PendingCommand :
	                    CueState_PendingEvent;
	rCue.eSoundID = a_eSoundID;
	rCue.uiRequest = uiRequest;
	rCue.hScheduledEvent = hScheduledEvent;
	rCue.hStopEvent = ASoundDelayedEventQueue_PSP::INVALID_EVENT;
	rCue.hVolumeEvent = ASoundDelayedEventQueue_PSP::INVALID_EVENT;
	rCue.hFrequencyEvent = ASoundDelayedEventQueue_PSP::INVALID_EVENT;
	rCue.hPositionEvent = ASoundDelayedEventQueue_PSP::INVALID_EVENT;
	rCue.hVoice = ASoundMixer_PSP::INVALID_VOICE;
	rCue.uiVolume = uiVolume;
	rCue.uiFrequency = ASoundMixer_PSP::DEFAULT_FREQUENCY;
	rCue.oPosition = { 0, 0, 0, DEFAULT_CUE_MAX_DISTANCE };
	rCue.uiStopDelaySampleFrames = 0;
	rCue.uiVolumeDelaySampleFrames = 0;
	rCue.uiFrequencyDelaySampleFrames = 0;
	rCue.uiPositionDelaySampleFrames = 0;
	rCue.bVolumeDirty = TFALSE;
	rCue.bFrequencyDirty = TFALSE;
	rCue.bPositionDirty = TFALSE;
	rCue.bPaused = a_bStartPaused;
	rCue.bPauseDirty = a_bStartPaused;
	rCue.bFading = TFALSE;
	rCue.bFadeStopWhenDone = TFALSE;
	rCue.bStopPending = TFALSE;
	rCue.uiFadeStartVolume = 0;
	rCue.uiFadeEndVolume = 0;
	rCue.uiFadeElapsedSampleFrames = 0;
	rCue.uiFadeDurationSampleFrames = 0;
	m_uiManagedCues++;
	m_uiPendingCues++;
	return MakeCueHandle( uiCue );
}

TINT ASoundManager_PSP::PlayCueAtPosition(
    ASoundId_PSP a_eSoundID,
    const Toshi::TVector4& a_rcPosition
)
{
	return PlayCueAtPositionEx(
	    a_eSoundID,
	    a_rcPosition,
	    1.0f,
	    TFALSE,
	    0.0f,
	    -1
	);
}

TINT ASoundManager_PSP::PlayCueAtPositionEx(
    ASoundId_PSP a_eSoundID,
    const Toshi::TVector4& a_rcPosition,
    TFLOAT a_fVolume,
    TBOOL a_bStartPaused,
    TFLOAT a_fDelay,
    TINT a_iTrack
)
{
	if ( !IsCuePositionValid_PSP( a_rcPosition ) )
	{
		m_uiRejectedCues++;
		return -1;
	}

	const TINT iCue = PlayCueEx(
	    a_eSoundID,
	    a_fVolume,
	    a_bStartPaused,
	    a_fDelay,
	    a_iTrack
	);
	if ( iCue >= 0 )
		SetCuePosition( iCue, a_rcPosition, a_fDelay );
	return iCue;
}

void ASoundManager_PSP::StopCue( TINT& a_rCue )
{
	StopCueAsync( a_rCue, 0.0f );
}

void ASoundManager_PSP::StopCueAsync( TINT& a_rCue, TFLOAT a_fDelay )
{
	if ( !( a_fDelay >= 0.0f && a_fDelay <= MAX_CUE_DELAY_SECONDS ) )
		return;

	const TINT iCue = a_rCue;
	a_rCue = -1;
	if ( iCue < 0 )
		return;

	const TUINT uiIndex = TUINT( iCue ) & CUE_INDEX_MASK;
	Cue* pCue = uiIndex < MAX_MANAGED_CUES ? &m_aCues[ uiIndex ] : TNULL;
	if ( pCue == TNULL || ResolveCue( iCue ) == TNULL )
		return;

	if ( pCue->eState == CueState_PendingEvent &&
	     m_pDelayedEventQueue->Cancel( pCue->hScheduledEvent ) )
	{
		m_uiPendingCues--;
		ReleaseCue( uiIndex );
		return;
	}

	pCue->bStopPending = TTRUE;
	pCue->uiStopDelaySampleFrames = TUINT(
	    a_fDelay * TFLOAT( SAMPLE_RATE ) + 0.5f
	);
	QueueCueControls( *pCue );
}

void ASoundManager_PSP::PauseCue( TINT a_iCue, TBOOL a_bPause )
{
	const TUINT uiIndex = a_iCue >= 0 ? TUINT( a_iCue ) & CUE_INDEX_MASK : MAX_MANAGED_CUES;
	Cue* pCue = uiIndex < MAX_MANAGED_CUES ? &m_aCues[ uiIndex ] : TNULL;
	if ( pCue == TNULL || ResolveCue( a_iCue ) == TNULL )
		return;

	pCue->bPaused = a_bPause;
	pCue->bPauseDirty = TTRUE;
	QueueCueControls( *pCue );
}

void ASoundManager_PSP::SetCueVolume( TINT a_iCue, TFLOAT a_fVolume )
{
	SetCueVolume( a_iCue, a_fVolume, 0.0f );
}

void ASoundManager_PSP::SetCueVolume(
    TINT a_iCue,
    TFLOAT a_fVolume,
    TFLOAT a_fDelay
)
{
	if ( a_fVolume != a_fVolume ||
	     !( a_fDelay >= 0.0f && a_fDelay <= MAX_CUE_DELAY_SECONDS ) )
		return;

	const TUINT uiIndex = a_iCue >= 0 ? TUINT( a_iCue ) & CUE_INDEX_MASK : MAX_MANAGED_CUES;
	Cue* pCue = uiIndex < MAX_MANAGED_CUES ? &m_aCues[ uiIndex ] : TNULL;
	if ( pCue == TNULL || ResolveCue( a_iCue ) == TNULL )
		return;

	const TFLOAT fVolume = a_fVolume <= 0.0f ? 0.0f :
	                       a_fVolume >= 1.0f ? 1.0f : a_fVolume;
	pCue->uiVolume = TUINT(
	    fVolume * TFLOAT( ASoundMixer_PSP::MAX_VOLUME ) + 0.5f
	);
	pCue->bFading = TFALSE;
	pCue->uiVolumeDelaySampleFrames = pCue->eState == CueState_Active ?
	    TUINT( a_fDelay * TFLOAT( SAMPLE_RATE ) + 0.5f ) : 0;
	if ( pCue->eState == CueState_Active )
		m_pDelayedEventQueue->Cancel( pCue->hVolumeEvent );
	pCue->bVolumeDirty = !(
	    pCue->eState == CueState_PendingEvent &&
	    m_pDelayedEventQueue->SetScheduledVolume(
	        pCue->hScheduledEvent,
	        pCue->uiVolume
	    )
	);
	QueueCueControls( *pCue );
}

void ASoundManager_PSP::FadeCue(
    TINT a_iCue,
    TFLOAT a_fDuration,
    TFLOAT a_fVolume,
    TFLOAT a_fUnused
)
{
	(void)a_fUnused;
	if ( a_fVolume != a_fVolume ||
	     !( a_fDuration >= 0.0f && a_fDuration <= MAX_CUE_DELAY_SECONDS ) )
		return;

	const TUINT uiIndex = a_iCue >= 0 ? TUINT( a_iCue ) & CUE_INDEX_MASK : MAX_MANAGED_CUES;
	Cue* pCue = uiIndex < MAX_MANAGED_CUES ? &m_aCues[ uiIndex ] : TNULL;
	if ( pCue == TNULL || ResolveCue( a_iCue ) == TNULL )
		return;

	const TFLOAT fVolume = a_fVolume <= 0.0f ? 0.0f :
	                       a_fVolume >= 1.0f ? 1.0f : a_fVolume;
	const TUINT uiEndVolume = TUINT(
	    fVolume * TFLOAT( ASoundMixer_PSP::MAX_VOLUME ) + 0.5f
	);
	const TUINT uiDuration = TUINT(
	    a_fDuration * TFLOAT( SAMPLE_RATE ) + 0.5f
	);
	if ( uiDuration == 0 )
	{
		SetCueVolume( a_iCue, fVolume );
		return;
	}

	pCue->bFading = TTRUE;
	pCue->bFadeStopWhenDone = TFALSE;
	pCue->uiFadeStartVolume = pCue->uiVolume;
	pCue->uiFadeEndVolume = uiEndVolume;
	pCue->uiFadeElapsedSampleFrames = 0;
	pCue->uiFadeDurationSampleFrames = uiDuration;
}

void ASoundManager_PSP::FadeOutCue(
    TINT a_iCue,
    TFLOAT a_fDuration,
    TBOOL a_bStop,
    TFLOAT a_fUnused
)
{
	if ( !( a_fDuration >= 0.0f && a_fDuration <= MAX_CUE_DELAY_SECONDS ) )
		return;

	FadeCue( a_iCue, a_fDuration, 0.0f, a_fUnused );
	const TUINT uiIndex = a_iCue >= 0 ? TUINT( a_iCue ) & CUE_INDEX_MASK : MAX_MANAGED_CUES;
	Cue* pCue = uiIndex < MAX_MANAGED_CUES ? &m_aCues[ uiIndex ] : TNULL;
	if ( pCue != TNULL && ResolveCue( a_iCue ) != TNULL )
	{
		if ( pCue->bFading )
			pCue->bFadeStopWhenDone = a_bStop;
		else if ( a_bStop && pCue->uiVolume == 0 )
		{
			pCue->bStopPending = TTRUE;
			QueueCueControls( *pCue );
		}
	}
}

TBOOL ASoundManager_PSP::IsCueFading( TINT a_iCue ) const
{
	const Cue* pCue = ResolveCue( a_iCue );
	return pCue != TNULL && pCue->bFading;
}

void ASoundManager_PSP::UpdateFades( TUINT a_uiElapsedSampleFrames )
{
	for ( TUINT i = 0; i < MAX_MANAGED_CUES; i++ )
	{
		Cue& rCue = m_aCues[ i ];
		if ( !rCue.bFading || rCue.eState != CueState_Active )
			continue;

		const TUINT uiRemaining = rCue.uiFadeDurationSampleFrames -
		                          rCue.uiFadeElapsedSampleFrames;
		const TUINT uiAdvance = a_uiElapsedSampleFrames < uiRemaining ?
		                             a_uiElapsedSampleFrames : uiRemaining;
		rCue.uiFadeElapsedSampleFrames += uiAdvance;
		const TINT64 iRange = TINT64( rCue.uiFadeEndVolume ) -
		                     TINT64( rCue.uiFadeStartVolume );
		rCue.uiVolume = TUINT(
		    TINT64( rCue.uiFadeStartVolume ) +
		    iRange * TINT64( rCue.uiFadeElapsedSampleFrames ) /
		        TINT64( rCue.uiFadeDurationSampleFrames )
		);
		rCue.bVolumeDirty = TTRUE;
		if ( rCue.uiFadeElapsedSampleFrames == rCue.uiFadeDurationSampleFrames )
		{
			rCue.bFading = TFALSE;
			if ( rCue.bFadeStopWhenDone )
				rCue.bStopPending = TTRUE;
		}
		QueueCueControls( rCue );
	}
}

void ASoundManager_PSP::SetCueFrequency( TINT a_iCue, TFLOAT a_fFrequency )
{
	SetCueFrequency( a_iCue, a_fFrequency, 0.0f );
}

void ASoundManager_PSP::SetCueFrequency(
    TINT a_iCue,
    TFLOAT a_fFrequency,
    TFLOAT a_fDelay
)
{
	const TFLOAT fMinFrequency = TFLOAT( ASoundMixer_PSP::MIN_FREQUENCY ) /
	                              TFLOAT( ASoundMixer_PSP::DEFAULT_FREQUENCY );
	const TFLOAT fMaxFrequency = TFLOAT( ASoundMixer_PSP::MAX_FREQUENCY ) /
	                              TFLOAT( ASoundMixer_PSP::DEFAULT_FREQUENCY );
	if ( !( a_fFrequency >= fMinFrequency && a_fFrequency <= fMaxFrequency ) ||
	     !( a_fDelay >= 0.0f && a_fDelay <= MAX_CUE_DELAY_SECONDS ) )
		return;

	const TUINT uiIndex = a_iCue >= 0 ? TUINT( a_iCue ) & CUE_INDEX_MASK : MAX_MANAGED_CUES;
	Cue* pCue = uiIndex < MAX_MANAGED_CUES ? &m_aCues[ uiIndex ] : TNULL;
	if ( pCue == TNULL || ResolveCue( a_iCue ) == TNULL )
		return;

	pCue->uiFrequency = TUINT(
	    a_fFrequency * TFLOAT( ASoundMixer_PSP::DEFAULT_FREQUENCY ) + 0.5f
	);
	pCue->uiFrequencyDelaySampleFrames = pCue->eState == CueState_Active ?
	    TUINT( a_fDelay * TFLOAT( SAMPLE_RATE ) + 0.5f ) : 0;
	if ( pCue->eState == CueState_Active )
		m_pDelayedEventQueue->Cancel( pCue->hFrequencyEvent );
	pCue->bFrequencyDirty = !(
	    pCue->eState == CueState_PendingEvent &&
	    m_pDelayedEventQueue->SetScheduledFrequency(
	        pCue->hScheduledEvent,
	        pCue->uiFrequency
	    )
	);
	QueueCueControls( *pCue );
}

void ASoundManager_PSP::SetCuePosition(
    TINT a_iCue,
    const Toshi::TVector4& a_rcPosition
)
{
	SetCuePosition( a_iCue, a_rcPosition, 0.0f );
}

void ASoundManager_PSP::SetCuePosition(
    TINT a_iCue,
    const Toshi::TVector4& a_rcPosition,
    TFLOAT a_fDelay
)
{
	if ( !IsCuePositionValid_PSP( a_rcPosition ) ||
	     !( a_fDelay >= 0.0f && a_fDelay <= MAX_CUE_DELAY_SECONDS ) )
	{
		return;
	}

	const TUINT uiIndex = a_iCue >= 0 ? TUINT( a_iCue ) & CUE_INDEX_MASK : MAX_MANAGED_CUES;
	Cue* pCue = uiIndex < MAX_MANAGED_CUES ? &m_aCues[ uiIndex ] : TNULL;
	if ( pCue == TNULL || ResolveCue( a_iCue ) == TNULL )
		return;

	pCue->oPosition = {
		ConvertCueCoordinate_PSP( a_rcPosition.x ),
		ConvertCueCoordinate_PSP( a_rcPosition.y ),
		ConvertCueCoordinate_PSP( a_rcPosition.z ),
		DEFAULT_CUE_MAX_DISTANCE,
	};
	pCue->uiPositionDelaySampleFrames = pCue->eState == CueState_Active ?
	    TUINT( a_fDelay * TFLOAT( SAMPLE_RATE ) + 0.5f ) : 0;
	if ( pCue->eState == CueState_Active )
		m_pDelayedEventQueue->Cancel( pCue->hPositionEvent );
	pCue->bPositionDirty = TTRUE;
	QueueCueControls( *pCue );
}

TBOOL ASoundManager_PSP::PlayCueDetached( ASoundId_PSP a_eSoundID )
{
	const TCHAR* pWaveName = ResolveCueName( a_eSoundID );
	if ( !IsCueBankBound() || pWaveName == TNULL ||
	     !m_pCommandQueue->QueuePlayDetachedNamed( *m_pWaveBank, pWaveName ) )
	{
		m_uiRejectedCues++;
		return TFALSE;
	}

	return TTRUE;
}

void ASoundManager_PSP::UpdateCues( const ASoundMixer_PSP& a_rMixer )
{
	if ( !IsCueBankBound() )
		return;

	for ( TUINT i = 0; i < MAX_MANAGED_CUES; i++ )
	{
		Cue& rCue = m_aCues[ i ];
		if ( rCue.eState == CueState_PendingCommand )
		{
			ASoundVoiceHandle_PSP hVoice = ASoundMixer_PSP::INVALID_VOICE;
			if ( m_pCommandQueue->GetPlayResult( rCue.uiRequest, hVoice ) )
			{
				m_uiPendingCues--;
				if ( hVoice == ASoundMixer_PSP::INVALID_VOICE )
					ReleaseCue( i );
				else
				{
					rCue.eState = CueState_Active;
					rCue.uiRequest = ASoundCommandQueue_PSP::INVALID_REQUEST;
					rCue.hVoice = hVoice;
					QueueCueControls( rCue );
				}
			}
		}
		else if ( rCue.eState == CueState_PendingEvent )
		{
			ASoundVoiceHandle_PSP hVoice = ASoundMixer_PSP::INVALID_VOICE;
			if ( m_pDelayedEventQueue->GetPlayResult( rCue.hScheduledEvent, hVoice ) )
			{
				m_uiPendingCues--;
				if ( hVoice == ASoundMixer_PSP::INVALID_VOICE )
					ReleaseCue( i );
				else
				{
					rCue.eState = CueState_Active;
					rCue.hScheduledEvent = ASoundDelayedEventQueue_PSP::INVALID_EVENT;
					rCue.hVoice = hVoice;
					QueueCueControls( rCue );
				}
			}
		}
		else if ( rCue.eState == CueState_Active && !a_rMixer.IsVoiceActive( rCue.hVoice ) )
		{
			ReleaseCue( i );
		}
		else if ( rCue.eState == CueState_Active )
		{
			QueueCueControls( rCue );
		}
	}
}

TBOOL ASoundManager_PSP::IsCuePlaying( TINT a_iCue ) const
{
	return ResolveCue( a_iCue ) != TNULL;
}

TBOOL ASoundManager_PSP::IsCueUsed( TINT a_iCue ) const
{
	return ResolveCue( a_iCue ) != TNULL;
}

TFLOAT ASoundManager_PSP::GetCueVolume( TINT a_iCue ) const
{
	const Cue* pCue = ResolveCue( a_iCue );
	return pCue != TNULL ?
	    TFLOAT( pCue->uiVolume ) / TFLOAT( ASoundMixer_PSP::MAX_VOLUME ) : 0.0f;
}

ASoundId_PSP ASoundManager_PSP::GetCueSoundId( TINT a_iCue ) const
{
	const Cue* pCue = ResolveCue( a_iCue );
	return pCue != TNULL ? pCue->eSoundID : ASOUNDID_PSP_INVALID;
}

TINT ASoundManager_PSP::FindPlayingCue( ASoundId_PSP a_eSoundID ) const
{
	for ( TUINT i = 0; i < MAX_MANAGED_CUES; i++ )
	{
		if ( m_aCues[ i ].eState != CueState_Free &&
		     m_aCues[ i ].eSoundID == a_eSoundID )
		{
			return MakeCueHandle( i );
		}
	}

	return -1;
}

TBOOL ASoundManager_PSP::GetCueVoice( TINT a_iCue, ASoundVoiceHandle_PSP& a_rVoice ) const
{
	const Cue* pCue = ResolveCue( a_iCue );
	if ( pCue == TNULL || pCue->eState != CueState_Active )
		return TFALSE;

	a_rVoice = pCue->hVoice;
	return TTRUE;
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

const ASoundManager_PSP::Cue* ASoundManager_PSP::ResolveCue( TINT a_iCue ) const
{
	if ( a_iCue < 0 )
		return TNULL;

	const TUINT32 uiCue = TUINT32( a_iCue );
	const TUINT uiIndex = uiCue & CUE_INDEX_MASK;
	const TUINT32 uiGeneration = uiCue >> 8;
	if ( uiIndex >= MAX_MANAGED_CUES || uiGeneration == 0 )
		return TNULL;

	const Cue& rCue = m_aCues[ uiIndex ];
	return rCue.eState != CueState_Free && rCue.uiGeneration == uiGeneration
	    ? &rCue
	    : TNULL;
}

TINT ASoundManager_PSP::MakeCueHandle( TUINT a_uiIndex ) const
{
	return TINT( ( m_aCues[ a_uiIndex ].uiGeneration << 8 ) | a_uiIndex );
}

void ASoundManager_PSP::ReleaseCue( TUINT a_uiIndex )
{
	Cue& rCue = m_aCues[ a_uiIndex ];
	if ( rCue.eState == CueState_Free )
		return;

	rCue.eState = CueState_Free;
	rCue.eSoundID = ASOUNDID_PSP_INVALID;
	rCue.uiRequest = ASoundCommandQueue_PSP::INVALID_REQUEST;
	rCue.hScheduledEvent = ASoundDelayedEventQueue_PSP::INVALID_EVENT;
	m_pDelayedEventQueue->Cancel( rCue.hStopEvent );
	m_pDelayedEventQueue->Cancel( rCue.hVolumeEvent );
	m_pDelayedEventQueue->Cancel( rCue.hFrequencyEvent );
	m_pDelayedEventQueue->Cancel( rCue.hPositionEvent );
	rCue.hStopEvent = ASoundDelayedEventQueue_PSP::INVALID_EVENT;
	rCue.hVolumeEvent = ASoundDelayedEventQueue_PSP::INVALID_EVENT;
	rCue.hFrequencyEvent = ASoundDelayedEventQueue_PSP::INVALID_EVENT;
	rCue.hPositionEvent = ASoundDelayedEventQueue_PSP::INVALID_EVENT;
	rCue.hVoice = ASoundMixer_PSP::INVALID_VOICE;
	rCue.uiVolume = 0;
	rCue.uiFrequency = ASoundMixer_PSP::DEFAULT_FREQUENCY;
	rCue.oPosition = { 0, 0, 0, DEFAULT_CUE_MAX_DISTANCE };
	rCue.uiStopDelaySampleFrames = 0;
	rCue.uiVolumeDelaySampleFrames = 0;
	rCue.uiFrequencyDelaySampleFrames = 0;
	rCue.uiPositionDelaySampleFrames = 0;
	rCue.bVolumeDirty = TFALSE;
	rCue.bFrequencyDirty = TFALSE;
	rCue.bPositionDirty = TFALSE;
	rCue.bPaused = TFALSE;
	rCue.bPauseDirty = TFALSE;
	rCue.bFading = TFALSE;
	rCue.bFadeStopWhenDone = TFALSE;
	rCue.bStopPending = TFALSE;
	rCue.uiFadeStartVolume = 0;
	rCue.uiFadeEndVolume = 0;
	rCue.uiFadeElapsedSampleFrames = 0;
	rCue.uiFadeDurationSampleFrames = 0;
	m_uiManagedCues--;
}

void ASoundManager_PSP::ClearCues()
{
	for ( TUINT i = 0; i < MAX_MANAGED_CUES; i++ )
	{
		m_aCues[ i ].eState = CueState_Free;
		m_aCues[ i ].eSoundID = ASOUNDID_PSP_INVALID;
		m_aCues[ i ].uiRequest = ASoundCommandQueue_PSP::INVALID_REQUEST;
		m_aCues[ i ].hScheduledEvent = ASoundDelayedEventQueue_PSP::INVALID_EVENT;
		m_aCues[ i ].hStopEvent = ASoundDelayedEventQueue_PSP::INVALID_EVENT;
		m_aCues[ i ].hVolumeEvent = ASoundDelayedEventQueue_PSP::INVALID_EVENT;
		m_aCues[ i ].hFrequencyEvent = ASoundDelayedEventQueue_PSP::INVALID_EVENT;
		m_aCues[ i ].hPositionEvent = ASoundDelayedEventQueue_PSP::INVALID_EVENT;
		m_aCues[ i ].hVoice = ASoundMixer_PSP::INVALID_VOICE;
		m_aCues[ i ].uiVolume = 0;
		m_aCues[ i ].uiFrequency = ASoundMixer_PSP::DEFAULT_FREQUENCY;
		m_aCues[ i ].oPosition = { 0, 0, 0, DEFAULT_CUE_MAX_DISTANCE };
		m_aCues[ i ].uiStopDelaySampleFrames = 0;
		m_aCues[ i ].uiVolumeDelaySampleFrames = 0;
		m_aCues[ i ].uiFrequencyDelaySampleFrames = 0;
		m_aCues[ i ].uiPositionDelaySampleFrames = 0;
		m_aCues[ i ].bVolumeDirty = TFALSE;
		m_aCues[ i ].bFrequencyDirty = TFALSE;
		m_aCues[ i ].bPositionDirty = TFALSE;
		m_aCues[ i ].bPaused = TFALSE;
		m_aCues[ i ].bPauseDirty = TFALSE;
		m_aCues[ i ].bFading = TFALSE;
		m_aCues[ i ].bFadeStopWhenDone = TFALSE;
		m_aCues[ i ].bStopPending = TFALSE;
		m_aCues[ i ].uiFadeStartVolume = 0;
		m_aCues[ i ].uiFadeEndVolume = 0;
		m_aCues[ i ].uiFadeElapsedSampleFrames = 0;
		m_aCues[ i ].uiFadeDurationSampleFrames = 0;
	}
	m_uiManagedCues = 0;
	m_uiPendingCues = 0;
}

void ASoundManager_PSP::QueueCueControls( Cue& a_rCue )
{
	if ( a_rCue.eState != CueState_Active )
		return;

	if ( a_rCue.bStopPending )
	{
		if ( a_rCue.uiStopDelaySampleFrames == 0 )
		{
			if ( m_pCommandQueue->QueueStop( a_rCue.hVoice ) )
				a_rCue.bStopPending = TFALSE;
		}
		else
		{
			m_pDelayedEventQueue->Cancel( a_rCue.hStopEvent );
			const ASoundEvent_PSP oEvent = {
				ASoundEventType_PSP_StopAudio,
				TNULL,
				a_rCue.hVoice,
				0,
				0,
				TFALSE,
				0,
				{},
			};
			a_rCue.hStopEvent = m_pDelayedEventQueue->Schedule(
			    *m_pWaveBank,
			    oEvent,
			    a_rCue.uiStopDelaySampleFrames
			);
			if ( a_rCue.hStopEvent != ASoundDelayedEventQueue_PSP::INVALID_EVENT )
				a_rCue.bStopPending = TFALSE;
		}
		return;
	}

	if ( a_rCue.bPauseDirty &&
	     m_pCommandQueue->QueueSetPaused( a_rCue.hVoice, a_rCue.bPaused ) )
	{
		a_rCue.bPauseDirty = TFALSE;
	}

	if ( a_rCue.bVolumeDirty && a_rCue.uiVolumeDelaySampleFrames == 0 )
	{
		if ( m_pCommandQueue->QueueSetVolume( a_rCue.hVoice, a_rCue.uiVolume ) )
			a_rCue.bVolumeDirty = TFALSE;
	}
	else if ( a_rCue.bVolumeDirty )
	{
		const ASoundEvent_PSP oEvent = {
			ASoundEventType_PSP_UpdateChannelVolume,
			TNULL,
			a_rCue.hVoice,
			a_rCue.uiVolume,
			0,
			TFALSE,
			0,
			{},
		};
		a_rCue.hVolumeEvent = m_pDelayedEventQueue->Schedule(
		    *m_pWaveBank,
		    oEvent,
		    a_rCue.uiVolumeDelaySampleFrames
		);
		if ( a_rCue.hVolumeEvent != ASoundDelayedEventQueue_PSP::INVALID_EVENT )
			a_rCue.bVolumeDirty = TFALSE;
	}

	if ( a_rCue.bFrequencyDirty && a_rCue.uiFrequencyDelaySampleFrames == 0 )
	{
		if ( m_pCommandQueue->QueueSetFrequency( a_rCue.hVoice, a_rCue.uiFrequency ) )
			a_rCue.bFrequencyDirty = TFALSE;
	}
	else if ( a_rCue.bFrequencyDirty )
	{
		const ASoundEvent_PSP oEvent = {
			ASoundEventType_PSP_UpdateChannelFrequency,
			TNULL,
			a_rCue.hVoice,
			0,
			0,
			TFALSE,
			a_rCue.uiFrequency,
			{},
		};
		a_rCue.hFrequencyEvent = m_pDelayedEventQueue->Schedule(
		    *m_pWaveBank,
		    oEvent,
		    a_rCue.uiFrequencyDelaySampleFrames
		);
		if ( a_rCue.hFrequencyEvent != ASoundDelayedEventQueue_PSP::INVALID_EVENT )
			a_rCue.bFrequencyDirty = TFALSE;
	}

	if ( a_rCue.bPositionDirty && a_rCue.uiPositionDelaySampleFrames == 0 )
	{
		if ( m_pCommandQueue->QueueSetPosition( a_rCue.hVoice, a_rCue.oPosition ) )
			a_rCue.bPositionDirty = TFALSE;
	}
	else if ( a_rCue.bPositionDirty )
	{
		const ASoundEvent_PSP oEvent = {
			ASoundEventType_PSP_UpdateChannelPosition,
			TNULL,
			a_rCue.hVoice,
			0,
			0,
			TFALSE,
			0,
			a_rCue.oPosition,
		};
		a_rCue.hPositionEvent = m_pDelayedEventQueue->Schedule(
		    *m_pWaveBank,
		    oEvent,
		    a_rCue.uiPositionDelaySampleFrames
		);
		if ( a_rCue.hPositionEvent != ASoundDelayedEventQueue_PSP::INVALID_EVENT )
			a_rCue.bPositionDirty = TFALSE;
	}
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
