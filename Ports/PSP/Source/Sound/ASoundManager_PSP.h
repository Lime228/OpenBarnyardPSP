#pragma once

#include <Toshi/Typedefs.h>
#include <Thread/TThread.h>
#include <Thread/TSemaphore.h>
#include <Math/TVector4.h>

#include "ASoundCommandQueue_PSP.h"
#include "ASoundDelayedEventQueue_PSP.h"

class ASoundManager_PSP;

using ASoundId_PSP = TINT32;

static constexpr ASoundId_PSP ASOUNDID_PSP_INVALID = -1;
static constexpr ASoundId_PSP ASoundId_PSP_UI_MENUCLICK = 166;
static constexpr ASoundId_PSP ASoundId_PSP_UI_MENUBACK  = 167;
static constexpr ASoundId_PSP ASoundId_PSP_UI_MENUOK    = 168;

class ASoundOutputThread_PSP : public Toshi::TThread
{
public:
	ASoundOutputThread_PSP();

	virtual void Main() override;
	void SetSoundManager( ASoundManager_PSP* a_pSoundManager ) { m_pSoundManager = a_pSoundManager; }

private:
	ASoundManager_PSP* m_pSoundManager;
};

class ASoundManager_PSP
{
public:
	static constexpr TUINT SAMPLE_RATE = 44100;
	static constexpr TUINT BUFFER_SAMPLE_FRAMES = 1024;
	static constexpr TUINT OUTPUT_CHANNELS = 2;
	static constexpr TFLOAT MAX_CUE_DELAY_SECONDS = 300.0f;
	static constexpr TINT CUE_POSITION_SCALE = 1000;
	static constexpr TUINT DEFAULT_CUE_MAX_DISTANCE = 100000;

public:
	ASoundManager_PSP();
	~ASoundManager_PSP();

	TBOOL Create();
	void Destroy();
	TBOOL BindCueBank(
	    const AWaveBank_PSP* a_pWaveBank,
	    ASoundCommandQueue_PSP* a_pCommandQueue,
	    ASoundDelayedEventQueue_PSP* a_pDelayedEventQueue
	);
	void UnbindCueBank();
	TINT PlayCue( ASoundId_PSP a_eSoundID );
	TINT PlayCueEx(
	    ASoundId_PSP a_eSoundID,
	    TFLOAT a_fVolume,
	    TBOOL a_bStartPaused,
	    TFLOAT a_fDelay,
	    TINT a_iTrack
	);
	void StopCue( TINT& a_rCue );
	void StopCueAsync( TINT& a_rCue, TFLOAT a_fDelay );
	void SetCueVolume( TINT a_iCue, TFLOAT a_fVolume );
	void SetCueVolume( TINT a_iCue, TFLOAT a_fVolume, TFLOAT a_fDelay );
	void SetCueFrequency( TINT a_iCue, TFLOAT a_fFrequency );
	void SetCueFrequency( TINT a_iCue, TFLOAT a_fFrequency, TFLOAT a_fDelay );
	void SetCuePosition( TINT a_iCue, const Toshi::TVector4& a_rcPosition );
	void SetCuePosition( TINT a_iCue, const Toshi::TVector4& a_rcPosition, TFLOAT a_fDelay );
	TBOOL PlayCueDetached( ASoundId_PSP a_eSoundID );
	void UpdateCues( const ASoundMixer_PSP& a_rMixer );
	TBOOL IsCuePlaying( TINT a_iCue ) const;
	TBOOL GetCueVoice( TINT a_iCue, ASoundVoiceHandle_PSP& a_rVoice ) const;

	TBOOL SubmitStereo( const TINT16* a_pSamples, TUINT a_uiSampleFrames );
	TBOOL SubmitSilence();
	TBOOL WaitForSubmittedBuffer();
	TINT16* LockOutputBuffer();
	TINT16* TryLockOutputBuffer();
	TBOOL UnlockOutputBuffer();

	TBOOL IsCreated() const { return m_iChannel >= 0; }
	TBOOL IsCueBankBound() const
	{
		return m_pWaveBank != TNULL && m_pCommandQueue != TNULL && m_pDelayedEventQueue != TNULL;
	}
	TINT GetChannel() const { return m_iChannel; }
	TUINT GetRejectedCueCount() const { return m_uiRejectedCues; }
	TUINT GetManagedCueCount() const { return m_uiManagedCues; }
	TUINT GetPendingCueCount() const { return m_uiPendingCues; }

private:
	enum CueState : TUINT
	{
		CueState_Free,
		CueState_PendingCommand,
		CueState_PendingEvent,
		CueState_Active,
	};

	struct Cue
	{
		TUINT32 uiGeneration;
		CueState eState;
		ASoundCommandRequest_PSP uiRequest;
		ASoundScheduledEventHandle_PSP hScheduledEvent;
		ASoundScheduledEventHandle_PSP hStopEvent;
		ASoundScheduledEventHandle_PSP hVolumeEvent;
		ASoundScheduledEventHandle_PSP hFrequencyEvent;
		ASoundScheduledEventHandle_PSP hPositionEvent;
		ASoundVoiceHandle_PSP hVoice;
		TUINT uiVolume;
		TUINT uiFrequency;
		ASoundPosition_PSP oPosition;
		TUINT uiStopDelaySampleFrames;
		TUINT uiVolumeDelaySampleFrames;
		TUINT uiFrequencyDelaySampleFrames;
		TUINT uiPositionDelaySampleFrames;
		TBOOL bVolumeDirty;
		TBOOL bFrequencyDirty;
		TBOOL bPositionDirty;
		TBOOL bStopPending;
	};

	const TCHAR* ResolveCueName( ASoundId_PSP a_eSoundID ) const;
	const Cue* ResolveCue( TINT a_iCue ) const;
	TINT MakeCueHandle( TUINT a_uiIndex ) const;
	void ReleaseCue( TUINT a_uiIndex );
	void ClearCues();
	void QueueCueControls( Cue& a_rCue );
	void RunOutputThread();
	friend class ASoundOutputThread_PSP;

	static constexpr TUINT OUTPUT_BUFFER_COUNT = 2;
	static constexpr TUINT MAX_MANAGED_CUES = 8;
	static constexpr TUINT32 CUE_INDEX_MASK = 0xff;
	static constexpr TUINT32 CUE_GENERATION_MASK = 0x007fffff;

	Toshi::TSemaphore m_FreeBuffers;
	Toshi::TSemaphore m_ReadyBuffers;
	Toshi::TSemaphore m_CompletedBuffers;
	Toshi::TSemaphore m_ThreadStopped;
	ASoundOutputThread_PSP m_OutputThread;
	alignas( 64 ) TINT16 m_aOutputBuffers[ 2 ][ BUFFER_SAMPLE_FRAMES * OUTPUT_CHANNELS ];
	TINT m_iChannel;
	volatile TBOOL m_bRunning;
	volatile TBOOL m_bOutputHealthy;
	TBOOL m_bOutputLocked;
	TUINT m_uiWriteBuffer;
	TUINT m_uiReadBuffer;
	const AWaveBank_PSP* m_pWaveBank;
	ASoundCommandQueue_PSP* m_pCommandQueue;
	ASoundDelayedEventQueue_PSP* m_pDelayedEventQueue;
	Cue m_aCues[ MAX_MANAGED_CUES ];
	TUINT m_uiManagedCues;
	TUINT m_uiPendingCues;
	TUINT m_uiRejectedCues;
};
