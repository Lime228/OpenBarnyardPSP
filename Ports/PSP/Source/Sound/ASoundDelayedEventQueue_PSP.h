#pragma once

#include "ASoundCommandQueue_PSP.h"

class ASoundDelayedEventQueue_PSP
{
public:
	static constexpr TUINT MAX_EVENTS = 8;
	static constexpr TUINT MAX_PENDING_PLAY_RESULTS = 8;

public:
	ASoundDelayedEventQueue_PSP();

	TBOOL Schedule(
	    const AWaveBank_PSP& a_rWaveBank,
	    const ASoundEvent_PSP& a_rEvent,
	    TUINT a_uiDelaySampleFrames
	);
	TBOOL Update( TUINT a_uiElapsedSampleFrames, ASoundCommandQueue_PSP& a_rCommandQueue );
	void Clear();

	TUINT GetEventCount() const { return m_uiEventCount; }
	TUINT GetPendingPlayResultCount() const { return m_uiPendingPlayResultCount; }
	TUINT GetRejectedEventCount() const { return m_uiRejectedEvents; }

private:
	struct Event
	{
		ASoundEventType_PSP eEventType;
		const AWave_PSP* pWave;
		ASoundVoiceHandle_PSP hVoice;
		TUINT uiVolume;
		TINT iPan;
		TBOOL bLoop;
		TUINT uiRemainingSampleFrames;
	};

private:
	TBOOL Dispatch( const Event& a_rEvent, ASoundCommandQueue_PSP& a_rCommandQueue );
	void RemoveEvent( TUINT a_uiIndex );
	void ConsumePlayResults( ASoundCommandQueue_PSP& a_rCommandQueue );

private:
	Event m_aEvents[ MAX_EVENTS ];
	ASoundCommandRequest_PSP m_aPendingPlayResults[ MAX_PENDING_PLAY_RESULTS ];
	TUINT m_uiEventCount;
	TUINT m_uiPendingPlayResultCount;
	TUINT m_uiRejectedEvents;
};
