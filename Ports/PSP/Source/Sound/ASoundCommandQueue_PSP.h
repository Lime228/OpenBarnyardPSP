#pragma once

#include "ASoundMixer_PSP.h"
#include "ASoundEvent_PSP.h"
#include "AWaveBank_PSP.h"

using ASoundCommandRequest_PSP = TUINT32;

enum ASoundCommandType_PSP
{
	ASoundCommandType_PSP_Play,
	ASoundCommandType_PSP_Stop,
	ASoundCommandType_PSP_SetVolume,
	ASoundCommandType_PSP_SetFrequency,
	ASoundCommandType_PSP_SetPan,
	ASoundCommandType_PSP_SetPosition,
};

struct ASoundCommand_PSP
{
	ASoundCommandType_PSP eType;
	const AWave_PSP* pWave;
	ASoundVoiceHandle_PSP hVoice;
	ASoundCommandRequest_PSP uiRequest;
	TUINT uiVolume;
	TINT iPan;
	TBOOL bLoop;
	TUINT uiFrequency;
	ASoundPosition_PSP oPosition;
};

struct ASoundCommandResult_PSP
{
	ASoundCommandRequest_PSP uiRequest;
	ASoundVoiceHandle_PSP hVoice;
};

class ASoundCommandQueue_PSP
{
public:
	static constexpr TUINT MAX_COMMANDS = 16;
	static constexpr TUINT MAX_RESULTS = 16;
	static constexpr ASoundCommandRequest_PSP INVALID_REQUEST = 0;

public:
	ASoundCommandQueue_PSP();

	ASoundCommandRequest_PSP QueuePlay(
	    const AWave_PSP* a_pWave,
	    TUINT a_uiVolume = ASoundMixer_PSP::MAX_VOLUME,
	    TINT a_iPan = 0,
	    TBOOL a_bLoop = TFALSE,
	    TUINT a_uiFrequency = ASoundMixer_PSP::DEFAULT_FREQUENCY
	);
	ASoundCommandRequest_PSP QueuePlayNamed(
	    const AWaveBank_PSP& a_rWaveBank,
	    const TCHAR* a_pWaveName,
	    TUINT a_uiVolume = ASoundMixer_PSP::MAX_VOLUME,
	    TINT a_iPan = 0,
	    TBOOL a_bLoop = TFALSE,
	    TUINT a_uiFrequency = ASoundMixer_PSP::DEFAULT_FREQUENCY
	);
	TBOOL QueueEvent(
	    const AWaveBank_PSP& a_rWaveBank,
	    const ASoundEvent_PSP& a_rEvent,
	    ASoundCommandRequest_PSP& a_rRequest
	);
	TBOOL QueueStop( ASoundVoiceHandle_PSP a_hVoice );
	TBOOL QueueSetVolume( ASoundVoiceHandle_PSP a_hVoice, TUINT a_uiVolume );
	TBOOL QueueSetFrequency( ASoundVoiceHandle_PSP a_hVoice, TUINT a_uiFrequency );
	TBOOL QueueSetPan( ASoundVoiceHandle_PSP a_hVoice, TINT a_iPan );
	TBOOL QueueSetPosition( ASoundVoiceHandle_PSP a_hVoice, const ASoundPosition_PSP& a_rPosition );

	TBOOL Process( ASoundMixer_PSP& a_rMixer );
	TBOOL GetPlayResult( ASoundCommandRequest_PSP a_uiRequest, ASoundVoiceHandle_PSP& a_rVoice );
	void Clear();

	TUINT GetQueuedCommandCount() const { return m_uiCommandCount; }
	TUINT GetFailedCommandCount() const { return m_uiFailedCommands; }

private:
	TBOOL Push( const ASoundCommand_PSP& a_rCommand );
	TUINT GetResultCount() const;
	ASoundCommandRequest_PSP AllocateRequest();

private:
	ASoundCommand_PSP m_aCommands[ MAX_COMMANDS ];
	ASoundCommandResult_PSP m_aResults[ MAX_RESULTS ];
	TUINT m_uiReadCommand;
	TUINT m_uiWriteCommand;
	TUINT m_uiCommandCount;
	TUINT m_uiPendingPlayCount;
	TUINT m_uiFailedCommands;
	ASoundCommandRequest_PSP m_uiNextRequest;
};
