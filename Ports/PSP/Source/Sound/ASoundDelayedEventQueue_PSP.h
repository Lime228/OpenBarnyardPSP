#pragma once

#include "ASoundCommandQueue_PSP.h"

using ASoundScheduledEventHandle_PSP = TUINT32;

class ASoundDelayedEventQueue_PSP
{
public:
	static constexpr TUINT MAX_EVENTS = 8;
	static constexpr ASoundScheduledEventHandle_PSP INVALID_EVENT = 0;

public:
	ASoundDelayedEventQueue_PSP();

	ASoundScheduledEventHandle_PSP Schedule(
	    const AWaveBank_PSP& a_rWaveBank,
	    const ASoundEvent_PSP& a_rEvent,
	    TUINT a_uiDelaySampleFrames
	);
	TBOOL Cancel( ASoundScheduledEventHandle_PSP a_hEvent );
	TBOOL SetScheduledVolume( ASoundScheduledEventHandle_PSP a_hEvent, TUINT a_uiVolume );
	TBOOL GetPlayResult( ASoundScheduledEventHandle_PSP a_hEvent, ASoundVoiceHandle_PSP& a_rVoice );
	TBOOL IsPending( ASoundScheduledEventHandle_PSP a_hEvent ) const;
	TBOOL Update( TUINT a_uiElapsedSampleFrames, ASoundCommandQueue_PSP& a_rCommandQueue );
	void Clear();

	TUINT GetEventCount() const { return m_uiEventCount; }
	TUINT GetPendingPlayResultCount() const { return m_uiPendingPlayResultCount; }
	TUINT GetReadyPlayResultCount() const { return m_uiReadyPlayResultCount; }
	TUINT GetRejectedEventCount() const { return m_uiRejectedEvents; }

private:
	enum EventState : TUINT
	{
		EventState_Free,
		EventState_Scheduled,
		EventState_PendingPlayResult,
		EventState_ReadyPlayResult,
	};

	struct Event
	{
		TUINT32 uiGeneration;
		EventState eState;
		ASoundEventType_PSP eEventType;
		const AWave_PSP* pWave;
		AStreamWave_PSP* pStream;
		ASoundVoiceHandle_PSP hVoice;
		ASoundCommandRequest_PSP uiRequest;
		TUINT uiVolume;
		TINT iPan;
		TBOOL bLoop;
		TUINT uiFrequency;
		ASoundPosition_PSP oPosition;
		TUINT uiRemainingSampleFrames;
	};

private:
	static constexpr TUINT32 GENERATION_MASK = 0x00ffffff;

	TBOOL Dispatch( TUINT a_uiIndex, ASoundCommandQueue_PSP& a_rCommandQueue );
	void ConsumePlayResults( ASoundCommandQueue_PSP& a_rCommandQueue );
	void ReleaseEvent( TUINT a_uiIndex );
	Event* Resolve( ASoundScheduledEventHandle_PSP a_hEvent );
	const Event* Resolve( ASoundScheduledEventHandle_PSP a_hEvent ) const;
	ASoundScheduledEventHandle_PSP MakeHandle( TUINT a_uiIndex ) const;

private:
	Event m_aEvents[ MAX_EVENTS ];
	TUINT m_uiEventCount;
	TUINT m_uiPendingPlayResultCount;
	TUINT m_uiReadyPlayResultCount;
	TUINT m_uiRejectedEvents;
};
