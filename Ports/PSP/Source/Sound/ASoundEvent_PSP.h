#pragma once

#include "ASoundMixer_PSP.h"

enum ASoundEventType_PSP : TUINT
{
	ASoundEventType_PSP_PlayAudio,
	ASoundEventType_PSP_StopAudio,
	ASoundEventType_PSP_UpdateChannelVolume,
	ASoundEventType_PSP_UpdateChannelFrequency,
	ASoundEventType_PSP_UpdateChannelPan,
	ASoundEventType_PSP_UpdateChannelPosition,
	ASoundEventType_PSP_Count,
};

struct ASoundEvent_PSP
{
	ASoundEventType_PSP eEventType;
	const TCHAR* pWaveName;
	ASoundVoiceHandle_PSP hVoice;
	TUINT uiVolume;
	TINT iPan;
	TBOOL bLoop;
	TUINT uiFrequency;
	ASoundPosition_PSP oPosition;
};
