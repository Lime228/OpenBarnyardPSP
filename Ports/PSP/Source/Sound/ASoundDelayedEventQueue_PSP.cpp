#include <ToshiPCH.h>
#include "ASoundDelayedEventQueue_PSP.h"

ASoundDelayedEventQueue_PSP::ASoundDelayedEventQueue_PSP()
    : m_aEvents{}
    , m_uiEventCount( 0 )
    , m_uiPendingPlayResultCount( 0 )
    , m_uiReadyPlayResultCount( 0 )
    , m_uiRejectedEvents( 0 )
{
}

ASoundScheduledEventHandle_PSP ASoundDelayedEventQueue_PSP::Schedule(
    const AWaveBank_PSP& a_rWaveBank,
    const ASoundEvent_PSP& a_rEvent,
    TUINT a_uiDelaySampleFrames
)
{
	if ( m_uiEventCount + m_uiPendingPlayResultCount + m_uiReadyPlayResultCount >= MAX_EVENTS ||
	     a_rEvent.eEventType >= ASoundEventType_PSP_Count )
	{
		m_uiRejectedEvents++;
		return INVALID_EVENT;
	}

	const AWave_PSP* pWave = TNULL;
	AStreamWave_PSP* pStream = TNULL;
	if ( a_rEvent.eEventType == ASoundEventType_PSP_PlayAudio )
	{
		pWave = a_rWaveBank.FindWave( a_rEvent.pWaveName );
		if ( pWave == TNULL )
			pStream = a_rWaveBank.FindStream( a_rEvent.pWaveName );
		const TUINT uiFrequency = a_rEvent.uiFrequency == 0 ?
		                              ASoundMixer_PSP::DEFAULT_FREQUENCY :
		                              a_rEvent.uiFrequency;
		if ( ( pWave == TNULL && pStream == TNULL ) ||
		     uiFrequency < ASoundMixer_PSP::MIN_FREQUENCY ||
		     uiFrequency > ASoundMixer_PSP::MAX_FREQUENCY ||
		     ( pStream != TNULL && uiFrequency != ASoundMixer_PSP::DEFAULT_FREQUENCY ) )
		{
			m_uiRejectedEvents++;
			return INVALID_EVENT;
		}
	}

	for ( TUINT i = 0; i < MAX_EVENTS; i++ )
	{
		Event& rEvent = m_aEvents[ i ];
		if ( rEvent.eState != EventState_Free )
			continue;

		rEvent.uiGeneration = ( rEvent.uiGeneration + 1 ) & GENERATION_MASK;
		if ( rEvent.uiGeneration == 0 ) rEvent.uiGeneration = 1;
		rEvent.eState = EventState_Scheduled;
		rEvent.eEventType = a_rEvent.eEventType;
		rEvent.pWave = pWave;
		rEvent.pStream = pStream;
		rEvent.hVoice = a_rEvent.hVoice;
		rEvent.uiRequest = ASoundCommandQueue_PSP::INVALID_REQUEST;
		rEvent.uiVolume = a_rEvent.uiVolume;
		rEvent.iPan = a_rEvent.iPan;
		rEvent.bLoop = a_rEvent.bLoop;
		rEvent.uiFrequency = a_rEvent.eEventType == ASoundEventType_PSP_PlayAudio &&
		                         a_rEvent.uiFrequency == 0 ?
		                         ASoundMixer_PSP::DEFAULT_FREQUENCY : a_rEvent.uiFrequency;
		rEvent.oPosition = a_rEvent.oPosition;
		rEvent.uiRemainingSampleFrames = a_uiDelaySampleFrames;
		m_uiEventCount++;
		return MakeHandle( i );
	}

	m_uiRejectedEvents++;
	return INVALID_EVENT;
}

TBOOL ASoundDelayedEventQueue_PSP::Cancel( ASoundScheduledEventHandle_PSP a_hEvent )
{
	Event* pEvent = Resolve( a_hEvent );
	if ( pEvent == TNULL || pEvent->eState != EventState_Scheduled ) return TFALSE;

	const TUINT uiIndex = TUINT( pEvent - m_aEvents );
	m_uiEventCount--;
	ReleaseEvent( uiIndex );
	return TTRUE;
}

TBOOL ASoundDelayedEventQueue_PSP::SetScheduledVolume(
    ASoundScheduledEventHandle_PSP a_hEvent,
    TUINT a_uiVolume
)
{
	Event* pEvent = Resolve( a_hEvent );
	if ( pEvent == TNULL || pEvent->eState != EventState_Scheduled ||
	     a_uiVolume > ASoundMixer_PSP::MAX_VOLUME )
	{
		return TFALSE;
	}

	pEvent->uiVolume = a_uiVolume;
	return TTRUE;
}

TBOOL ASoundDelayedEventQueue_PSP::SetScheduledFrequency(
    ASoundScheduledEventHandle_PSP a_hEvent,
    TUINT a_uiFrequency
)
{
	Event* pEvent = Resolve( a_hEvent );
	if ( pEvent == TNULL || pEvent->eState != EventState_Scheduled ||
	     pEvent->eEventType != ASoundEventType_PSP_PlayAudio ||
	     pEvent->pStream != TNULL ||
	     a_uiFrequency < ASoundMixer_PSP::MIN_FREQUENCY ||
	     a_uiFrequency > ASoundMixer_PSP::MAX_FREQUENCY )
		return TFALSE;

	pEvent->uiFrequency = a_uiFrequency;
	return TTRUE;
}

TBOOL ASoundDelayedEventQueue_PSP::GetPlayResult(
    ASoundScheduledEventHandle_PSP a_hEvent,
    ASoundVoiceHandle_PSP& a_rVoice
)
{
	Event* pEvent = Resolve( a_hEvent );
	if ( pEvent == TNULL || pEvent->eState != EventState_ReadyPlayResult ) return TFALSE;

	a_rVoice = pEvent->hVoice;
	const TUINT uiIndex = TUINT( pEvent - m_aEvents );
	m_uiReadyPlayResultCount--;
	ReleaseEvent( uiIndex );
	return TTRUE;
}

TBOOL ASoundDelayedEventQueue_PSP::IsPending( ASoundScheduledEventHandle_PSP a_hEvent ) const
{
	const Event* pEvent = Resolve( a_hEvent );
	return pEvent != TNULL &&
	       ( pEvent->eState == EventState_Scheduled || pEvent->eState == EventState_PendingPlayResult );
}

TBOOL ASoundDelayedEventQueue_PSP::Update(
    TUINT a_uiElapsedSampleFrames,
    ASoundCommandQueue_PSP& a_rCommandQueue
)
{
	ConsumePlayResults( a_rCommandQueue );

	for ( TUINT i = 0; i < MAX_EVENTS; i++ )
	{
		Event& rEvent = m_aEvents[ i ];
		if ( rEvent.eState != EventState_Scheduled ) continue;

		if ( rEvent.uiRemainingSampleFrames > a_uiElapsedSampleFrames )
		{
			rEvent.uiRemainingSampleFrames -= a_uiElapsedSampleFrames;
			continue;
		}

		rEvent.uiRemainingSampleFrames = 0;
		Dispatch( i, a_rCommandQueue );
	}

	return m_uiEventCount == 0;
}

void ASoundDelayedEventQueue_PSP::Clear()
{
	for ( TUINT i = 0; i < MAX_EVENTS; i++ ) ReleaseEvent( i );
	m_uiEventCount = 0;
	m_uiPendingPlayResultCount = 0;
	m_uiReadyPlayResultCount = 0;
	m_uiRejectedEvents = 0;
}

TBOOL ASoundDelayedEventQueue_PSP::Dispatch(
    TUINT a_uiIndex,
    ASoundCommandQueue_PSP& a_rCommandQueue
)
{
	Event& rEvent = m_aEvents[ a_uiIndex ];
	switch ( rEvent.eEventType )
	{
		case ASoundEventType_PSP_PlayAudio:
		{
			const ASoundCommandRequest_PSP uiRequest = rEvent.pStream != TNULL ?
			    a_rCommandQueue.QueuePlayStream(
			        rEvent.pStream,
			        rEvent.uiVolume,
			        rEvent.iPan,
			        rEvent.bLoop,
			        rEvent.uiFrequency
			    ) :
			    a_rCommandQueue.QueuePlay(
			        rEvent.pWave,
			        rEvent.uiVolume,
			        rEvent.iPan,
			        rEvent.bLoop,
			        rEvent.uiFrequency
			    );
			if ( uiRequest == ASoundCommandQueue_PSP::INVALID_REQUEST ) return TFALSE;

			rEvent.uiRequest = uiRequest;
			rEvent.eState = EventState_PendingPlayResult;
			m_uiEventCount--;
			m_uiPendingPlayResultCount++;
			return TTRUE;
		}
		case ASoundEventType_PSP_StopAudio:
			if ( !a_rCommandQueue.QueueStop( rEvent.hVoice ) ) return TFALSE;
			break;
		case ASoundEventType_PSP_UpdateChannelVolume:
			if ( !a_rCommandQueue.QueueSetVolume( rEvent.hVoice, rEvent.uiVolume ) ) return TFALSE;
			break;
		case ASoundEventType_PSP_UpdateChannelFrequency:
			if ( !a_rCommandQueue.QueueSetFrequency( rEvent.hVoice, rEvent.uiFrequency ) ) return TFALSE;
			break;
		case ASoundEventType_PSP_UpdateChannelPan:
			if ( !a_rCommandQueue.QueueSetPan( rEvent.hVoice, rEvent.iPan ) ) return TFALSE;
			break;
		case ASoundEventType_PSP_UpdateChannelPosition:
			if ( !a_rCommandQueue.QueueSetPosition( rEvent.hVoice, rEvent.oPosition ) ) return TFALSE;
			break;
		default:
			return TFALSE;
	}

	m_uiEventCount--;
	ReleaseEvent( a_uiIndex );
	return TTRUE;
}

void ASoundDelayedEventQueue_PSP::ConsumePlayResults( ASoundCommandQueue_PSP& a_rCommandQueue )
{
	for ( TUINT i = 0; i < MAX_EVENTS; i++ )
	{
		Event& rEvent = m_aEvents[ i ];
		if ( rEvent.eState != EventState_PendingPlayResult ) continue;

		ASoundVoiceHandle_PSP hVoice = ASoundMixer_PSP::INVALID_VOICE;
		if ( !a_rCommandQueue.GetPlayResult( rEvent.uiRequest, hVoice ) ) continue;

		rEvent.uiRequest = ASoundCommandQueue_PSP::INVALID_REQUEST;
		rEvent.hVoice = hVoice;
		rEvent.eState = EventState_ReadyPlayResult;
		m_uiPendingPlayResultCount--;
		m_uiReadyPlayResultCount++;
	}
}

void ASoundDelayedEventQueue_PSP::ReleaseEvent( TUINT a_uiIndex )
{
	Event& rEvent = m_aEvents[ a_uiIndex ];
	const TUINT32 uiGeneration = rEvent.uiGeneration;
	rEvent = {};
	rEvent.uiGeneration = uiGeneration;
}

ASoundDelayedEventQueue_PSP::Event* ASoundDelayedEventQueue_PSP::Resolve(
    ASoundScheduledEventHandle_PSP a_hEvent
)
{
	return const_cast<Event*>( static_cast<const ASoundDelayedEventQueue_PSP*>( this )->Resolve( a_hEvent ) );
}

const ASoundDelayedEventQueue_PSP::Event* ASoundDelayedEventQueue_PSP::Resolve(
    ASoundScheduledEventHandle_PSP a_hEvent
) const
{
	if ( a_hEvent == INVALID_EVENT ) return TNULL;

	const TUINT uiSlot = TUINT( a_hEvent & 0xff );
	if ( uiSlot == 0 || uiSlot > MAX_EVENTS ) return TNULL;

	const Event& rEvent = m_aEvents[ uiSlot - 1 ];
	const TUINT32 uiGeneration = ( a_hEvent >> 8 ) & GENERATION_MASK;
	if ( rEvent.eState == EventState_Free || rEvent.uiGeneration != uiGeneration ) return TNULL;
	return &rEvent;
}

ASoundScheduledEventHandle_PSP ASoundDelayedEventQueue_PSP::MakeHandle( TUINT a_uiIndex ) const
{
	return ( m_aEvents[ a_uiIndex ].uiGeneration << 8 ) | TUINT32( a_uiIndex + 1 );
}
