#include <ToshiPCH.h>
#include "ASoundCommandQueue_PSP.h"

#include <Toshi/TUtil.h>

ASoundCommandQueue_PSP::ASoundCommandQueue_PSP()
    : m_aCommands{}
    , m_aResults{}
    , m_uiReadCommand( 0 )
    , m_uiWriteCommand( 0 )
    , m_uiCommandCount( 0 )
    , m_uiPendingPlayCount( 0 )
    , m_uiFailedCommands( 0 )
    , m_uiNextRequest( 0 )
{
}

ASoundCommandRequest_PSP ASoundCommandQueue_PSP::QueuePlay(
    const AWave_PSP* a_pWave,
    TUINT a_uiVolume,
    TINT a_iPan,
    TBOOL a_bLoop,
    TUINT a_uiFrequency
)
{
	if ( a_pWave == TNULL || !a_pWave->IsCreated() ||
	     a_uiFrequency < ASoundMixer_PSP::MIN_FREQUENCY ||
	     a_uiFrequency > ASoundMixer_PSP::MAX_FREQUENCY ||
	     m_uiPendingPlayCount + GetResultCount() >= MAX_RESULTS )
	{
		return INVALID_REQUEST;
	}

	const ASoundCommandRequest_PSP uiRequest = AllocateRequest();
	const ASoundCommand_PSP oCommand = {
		ASoundCommandType_PSP_Play,
		a_pWave,
		ASoundMixer_PSP::INVALID_VOICE,
		uiRequest,
		a_uiVolume,
		a_iPan,
		a_bLoop,
		a_uiFrequency,
		{},
		{},
	};
	if ( !Push( oCommand ) )
		return INVALID_REQUEST;

	m_uiPendingPlayCount++;
	return uiRequest;
}

ASoundCommandRequest_PSP ASoundCommandQueue_PSP::QueuePlayNamed(
    const AWaveBank_PSP& a_rWaveBank,
    const TCHAR* a_pWaveName,
    TUINT a_uiVolume,
    TINT a_iPan,
    TBOOL a_bLoop,
    TUINT a_uiFrequency
)
{
	const AWave_PSP* pWave = a_rWaveBank.FindWave( a_pWaveName );
	if ( pWave != TNULL )
		return QueuePlay( pWave, a_uiVolume, a_iPan, a_bLoop, a_uiFrequency );
	return QueuePlayStream(
	    a_rWaveBank.FindStream( a_pWaveName ),
	    a_uiVolume,
	    a_iPan,
	    a_bLoop,
	    a_uiFrequency
	);
}

TBOOL ASoundCommandQueue_PSP::QueuePlayDetachedNamed(
    const AWaveBank_PSP& a_rWaveBank,
    const TCHAR* a_pWaveName,
    TUINT a_uiVolume,
    TINT a_iPan,
    TBOOL a_bLoop,
    TUINT a_uiFrequency
)
{
	const AWave_PSP* pWave = a_rWaveBank.FindWave( a_pWaveName );
	if ( pWave == TNULL || !pWave->IsCreated() ||
	     a_uiFrequency < ASoundMixer_PSP::MIN_FREQUENCY ||
	     a_uiFrequency > ASoundMixer_PSP::MAX_FREQUENCY )
	{
		return TFALSE;
	}

	return Push( {
		ASoundCommandType_PSP_Play,
		pWave,
		ASoundMixer_PSP::INVALID_VOICE,
		INVALID_REQUEST,
		a_uiVolume,
		a_iPan,
		a_bLoop,
		a_uiFrequency,
		{},
		{},
	} );
}

ASoundCommandRequest_PSP ASoundCommandQueue_PSP::QueuePlayStream(
    AStreamWave_PSP* a_pStream,
    TUINT a_uiVolume,
    TINT a_iPan,
    TBOOL a_bLoop,
    TUINT a_uiFrequency
)
{
	if ( a_pStream == TNULL || !a_pStream->IsCreated() ||
	     a_uiFrequency != ASoundMixer_PSP::DEFAULT_FREQUENCY ||
	     m_uiPendingPlayCount + GetResultCount() >= MAX_RESULTS )
	{
		return INVALID_REQUEST;
	}

	const ASoundCommandRequest_PSP uiRequest = AllocateRequest();
	const ASoundCommand_PSP oCommand = {
		ASoundCommandType_PSP_PlayStream,
		TNULL,
		ASoundMixer_PSP::INVALID_VOICE,
		uiRequest,
		a_uiVolume,
		a_iPan,
		a_bLoop,
		a_uiFrequency,
		{},
		{},
		a_pStream,
	};
	if ( !Push( oCommand ) )
		return INVALID_REQUEST;

	m_uiPendingPlayCount++;
	return uiRequest;
}

TBOOL ASoundCommandQueue_PSP::QueueEvent(
    const AWaveBank_PSP& a_rWaveBank,
    const ASoundEvent_PSP& a_rEvent,
    ASoundCommandRequest_PSP& a_rRequest
)
{
	a_rRequest = INVALID_REQUEST;
	switch ( a_rEvent.eEventType )
	{
		case ASoundEventType_PSP_PlayAudio:
			a_rRequest = QueuePlayNamed(
			    a_rWaveBank,
			    a_rEvent.pWaveName,
			    a_rEvent.uiVolume,
			    a_rEvent.iPan,
			    a_rEvent.bLoop,
			    a_rEvent.uiFrequency == 0 ? ASoundMixer_PSP::DEFAULT_FREQUENCY : a_rEvent.uiFrequency
			);
			return a_rRequest != INVALID_REQUEST;
		case ASoundEventType_PSP_StopAudio:
			return QueueStop( a_rEvent.hVoice );
		case ASoundEventType_PSP_UpdateChannelVolume:
			return QueueSetVolume( a_rEvent.hVoice, a_rEvent.uiVolume );
		case ASoundEventType_PSP_UpdateChannelFrequency:
			return QueueSetFrequency( a_rEvent.hVoice, a_rEvent.uiFrequency );
		case ASoundEventType_PSP_UpdateChannelPan:
			return QueueSetPan( a_rEvent.hVoice, a_rEvent.iPan );
		case ASoundEventType_PSP_UpdateChannelPosition:
			return QueueSetPosition( a_rEvent.hVoice, a_rEvent.oPosition );
		default:
			return TFALSE;
	}
}

TBOOL ASoundCommandQueue_PSP::QueueStop( ASoundVoiceHandle_PSP a_hVoice )
{
	return Push( { ASoundCommandType_PSP_Stop, TNULL, a_hVoice, INVALID_REQUEST, 0, 0, TFALSE, 0, {}, {} } );
}

TBOOL ASoundCommandQueue_PSP::QueueSetVolume( ASoundVoiceHandle_PSP a_hVoice, TUINT a_uiVolume )
{
	return Push( { ASoundCommandType_PSP_SetVolume, TNULL, a_hVoice, INVALID_REQUEST, a_uiVolume, 0, TFALSE, 0, {}, {} } );
}

TBOOL ASoundCommandQueue_PSP::QueueSetFrequency( ASoundVoiceHandle_PSP a_hVoice, TUINT a_uiFrequency )
{
	return Push( { ASoundCommandType_PSP_SetFrequency, TNULL, a_hVoice, INVALID_REQUEST, 0, 0, TFALSE, a_uiFrequency, {}, {} } );
}

TBOOL ASoundCommandQueue_PSP::QueueSetPan( ASoundVoiceHandle_PSP a_hVoice, TINT a_iPan )
{
	return Push( { ASoundCommandType_PSP_SetPan, TNULL, a_hVoice, INVALID_REQUEST, 0, a_iPan, TFALSE, 0, {}, {} } );
}

TBOOL ASoundCommandQueue_PSP::QueueSetPosition(
    ASoundVoiceHandle_PSP a_hVoice,
    const ASoundPosition_PSP& a_rPosition
)
{
	return Push( { ASoundCommandType_PSP_SetPosition, TNULL, a_hVoice, INVALID_REQUEST, 0, 0, TFALSE, 0, a_rPosition, {} } );
}

TBOOL ASoundCommandQueue_PSP::QueueSetListener( const ASoundListener_PSP& a_rListener )
{
	return Push( {
		ASoundCommandType_PSP_SetListener,
		TNULL,
		ASoundMixer_PSP::INVALID_VOICE,
		INVALID_REQUEST,
		0,
		0,
		TFALSE,
		0,
		{},
		a_rListener,
	} );
}

TBOOL ASoundCommandQueue_PSP::Process( ASoundMixer_PSP& a_rMixer )
{
	TBOOL bSucceeded = TTRUE;
	while ( m_uiCommandCount != 0 )
	{
		const ASoundCommand_PSP oCommand = m_aCommands[ m_uiReadCommand ];
		m_aCommands[ m_uiReadCommand ] = {};
		m_uiReadCommand = ( m_uiReadCommand + 1 ) % MAX_COMMANDS;
		m_uiCommandCount--;

		TBOOL bCommandSucceeded = TFALSE;
		switch ( oCommand.eType )
		{
			case ASoundCommandType_PSP_Play:
			{
				const ASoundVoiceHandle_PSP hVoice = a_rMixer.Play(
				    oCommand.pWave,
				    oCommand.uiVolume,
				    oCommand.iPan,
				    oCommand.bLoop,
				    oCommand.uiFrequency
				);
				for ( TUINT i = 0;
				      oCommand.uiRequest != INVALID_REQUEST && i < MAX_RESULTS;
				      i++ )
				{
					if ( m_aResults[ i ].uiRequest == INVALID_REQUEST )
					{
						m_aResults[ i ] = { oCommand.uiRequest, hVoice };
						break;
					}
				}
				if ( oCommand.uiRequest != INVALID_REQUEST )
					m_uiPendingPlayCount--;
				bCommandSucceeded = hVoice != ASoundMixer_PSP::INVALID_VOICE;
				break;
			}
			case ASoundCommandType_PSP_PlayStream:
			{
				const ASoundVoiceHandle_PSP hVoice = a_rMixer.PlayStream(
				    oCommand.pStream,
				    oCommand.uiVolume,
				    oCommand.iPan,
				    oCommand.bLoop
				);
				for ( TUINT i = 0; i < MAX_RESULTS; i++ )
				{
					if ( m_aResults[ i ].uiRequest == INVALID_REQUEST )
					{
						m_aResults[ i ] = { oCommand.uiRequest, hVoice };
						break;
					}
				}
				m_uiPendingPlayCount--;
				bCommandSucceeded = hVoice != ASoundMixer_PSP::INVALID_VOICE;
				break;
			}
			case ASoundCommandType_PSP_Stop:
				bCommandSucceeded = a_rMixer.Stop( oCommand.hVoice );
				break;
			case ASoundCommandType_PSP_SetVolume:
				bCommandSucceeded = a_rMixer.SetVolume( oCommand.hVoice, oCommand.uiVolume );
				break;
			case ASoundCommandType_PSP_SetFrequency:
				bCommandSucceeded = a_rMixer.SetFrequency( oCommand.hVoice, oCommand.uiFrequency );
				break;
			case ASoundCommandType_PSP_SetPan:
				bCommandSucceeded = a_rMixer.SetPan( oCommand.hVoice, oCommand.iPan );
				break;
			case ASoundCommandType_PSP_SetPosition:
				bCommandSucceeded = a_rMixer.SetWorldPosition( oCommand.hVoice, oCommand.oPosition );
				break;
			case ASoundCommandType_PSP_SetListener:
				bCommandSucceeded = a_rMixer.SetListener( oCommand.oListener );
				break;
		}

		if ( !bCommandSucceeded )
		{
			m_uiFailedCommands++;
			bSucceeded = TFALSE;
		}
	}

	return bSucceeded;
}

TBOOL ASoundCommandQueue_PSP::GetPlayResult(
    ASoundCommandRequest_PSP a_uiRequest,
    ASoundVoiceHandle_PSP& a_rVoice
)
{
	if ( a_uiRequest == INVALID_REQUEST )
		return TFALSE;

	for ( TUINT i = 0; i < MAX_RESULTS; i++ )
	{
		if ( m_aResults[ i ].uiRequest == a_uiRequest )
		{
			a_rVoice = m_aResults[ i ].hVoice;
			m_aResults[ i ] = {};
			return TTRUE;
		}
	}

	return TFALSE;
}

void ASoundCommandQueue_PSP::Clear()
{
	Toshi::TUtil::MemClear( m_aCommands, sizeof( m_aCommands ) );
	Toshi::TUtil::MemClear( m_aResults, sizeof( m_aResults ) );
	m_uiReadCommand = 0;
	m_uiWriteCommand = 0;
	m_uiCommandCount = 0;
	m_uiPendingPlayCount = 0;
	m_uiFailedCommands = 0;
}

TBOOL ASoundCommandQueue_PSP::Push( const ASoundCommand_PSP& a_rCommand )
{
	if ( m_uiCommandCount >= MAX_COMMANDS )
		return TFALSE;

	m_aCommands[ m_uiWriteCommand ] = a_rCommand;
	m_uiWriteCommand = ( m_uiWriteCommand + 1 ) % MAX_COMMANDS;
	m_uiCommandCount++;
	return TTRUE;
}

TUINT ASoundCommandQueue_PSP::GetResultCount() const
{
	TUINT uiCount = 0;
	for ( TUINT i = 0; i < MAX_RESULTS; i++ )
	{
		if ( m_aResults[ i ].uiRequest != INVALID_REQUEST )
			uiCount++;
	}
	return uiCount;
}

ASoundCommandRequest_PSP ASoundCommandQueue_PSP::AllocateRequest()
{
	m_uiNextRequest++;
	if ( m_uiNextRequest == INVALID_REQUEST )
		m_uiNextRequest++;
	return m_uiNextRequest;
}
