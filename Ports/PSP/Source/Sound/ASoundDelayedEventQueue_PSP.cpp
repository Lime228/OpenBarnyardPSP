#include <ToshiPCH.h>
#include "ASoundDelayedEventQueue_PSP.h"

#include <Toshi/TUtil.h>

ASoundDelayedEventQueue_PSP::ASoundDelayedEventQueue_PSP()
    : m_aEvents{}
    , m_aPendingPlayResults{}
    , m_uiEventCount( 0 )
    , m_uiPendingPlayResultCount( 0 )
    , m_uiRejectedEvents( 0 )
{
}

TBOOL ASoundDelayedEventQueue_PSP::Schedule(
    const AWaveBank_PSP& a_rWaveBank,
    const ASoundEvent_PSP& a_rEvent,
    TUINT a_uiDelaySampleFrames
)
{
	if ( m_uiEventCount >= MAX_EVENTS || a_rEvent.eEventType >= ASoundEventType_PSP_Count )
	{
		m_uiRejectedEvents++;
		return TFALSE;
	}

	const AWave_PSP* pWave = TNULL;
	if ( a_rEvent.eEventType == ASoundEventType_PSP_PlayAudio )
	{
		pWave = a_rWaveBank.FindWave( a_rEvent.pWaveName );
		if ( pWave == TNULL || a_rEvent.bLoop )
		{
			m_uiRejectedEvents++;
			return TFALSE;
		}
	}

	m_aEvents[ m_uiEventCount++ ] = {
		a_rEvent.eEventType,
		pWave,
		a_rEvent.hVoice,
		a_rEvent.uiVolume,
		a_rEvent.iPan,
		a_rEvent.bLoop,
		a_uiDelaySampleFrames,
	};
	return TTRUE;
}

TBOOL ASoundDelayedEventQueue_PSP::Update(
    TUINT a_uiElapsedSampleFrames,
    ASoundCommandQueue_PSP& a_rCommandQueue
)
{
	ConsumePlayResults( a_rCommandQueue );

	TUINT uiEvent = 0;
	while ( uiEvent < m_uiEventCount )
	{
		Event& rEvent = m_aEvents[ uiEvent ];
		if ( rEvent.uiRemainingSampleFrames > a_uiElapsedSampleFrames )
		{
			rEvent.uiRemainingSampleFrames -= a_uiElapsedSampleFrames;
			uiEvent++;
			continue;
		}

		rEvent.uiRemainingSampleFrames = 0;
		if ( !Dispatch( rEvent, a_rCommandQueue ) )
		{
			uiEvent++;
			continue;
		}

		RemoveEvent( uiEvent );
	}

	return m_uiEventCount == 0;
}

void ASoundDelayedEventQueue_PSP::Clear()
{
	Toshi::TUtil::MemClear( m_aEvents, sizeof( m_aEvents ) );
	Toshi::TUtil::MemClear( m_aPendingPlayResults, sizeof( m_aPendingPlayResults ) );
	m_uiEventCount = 0;
	m_uiPendingPlayResultCount = 0;
	m_uiRejectedEvents = 0;
}

TBOOL ASoundDelayedEventQueue_PSP::Dispatch(
    const Event& a_rEvent,
    ASoundCommandQueue_PSP& a_rCommandQueue
)
{
	switch ( a_rEvent.eEventType )
	{
		case ASoundEventType_PSP_PlayAudio:
		{
			if ( m_uiPendingPlayResultCount >= MAX_PENDING_PLAY_RESULTS )
				return TFALSE;

			const ASoundCommandRequest_PSP uiRequest = a_rCommandQueue.QueuePlay(
			    a_rEvent.pWave,
			    a_rEvent.uiVolume,
			    a_rEvent.iPan,
			    a_rEvent.bLoop
			);
			if ( uiRequest == ASoundCommandQueue_PSP::INVALID_REQUEST )
				return TFALSE;

			m_aPendingPlayResults[ m_uiPendingPlayResultCount++ ] = uiRequest;
			return TTRUE;
		}
		case ASoundEventType_PSP_StopAudio:
			return a_rCommandQueue.QueueStop( a_rEvent.hVoice );
		case ASoundEventType_PSP_UpdateChannelVolume:
			return a_rCommandQueue.QueueSetVolume( a_rEvent.hVoice, a_rEvent.uiVolume );
		case ASoundEventType_PSP_UpdateChannelPan:
			return a_rCommandQueue.QueueSetPan( a_rEvent.hVoice, a_rEvent.iPan );
		default:
			return TFALSE;
	}
}

void ASoundDelayedEventQueue_PSP::RemoveEvent( TUINT a_uiIndex )
{
	m_uiEventCount--;
	for ( TUINT i = a_uiIndex; i < m_uiEventCount; i++ )
		m_aEvents[ i ] = m_aEvents[ i + 1 ];
	m_aEvents[ m_uiEventCount ] = {};
}

void ASoundDelayedEventQueue_PSP::ConsumePlayResults( ASoundCommandQueue_PSP& a_rCommandQueue )
{
	TUINT uiRequest = 0;
	while ( uiRequest < m_uiPendingPlayResultCount )
	{
		ASoundVoiceHandle_PSP hVoice = ASoundMixer_PSP::INVALID_VOICE;
		if ( !a_rCommandQueue.GetPlayResult( m_aPendingPlayResults[ uiRequest ], hVoice ) )
		{
			uiRequest++;
			continue;
		}

		m_uiPendingPlayResultCount--;
		for ( TUINT i = uiRequest; i < m_uiPendingPlayResultCount; i++ )
			m_aPendingPlayResults[ i ] = m_aPendingPlayResults[ i + 1 ];
		m_aPendingPlayResults[ m_uiPendingPlayResultCount ] = ASoundCommandQueue_PSP::INVALID_REQUEST;
	}
}
