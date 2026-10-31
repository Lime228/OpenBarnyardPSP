#pragma once

#include "AWave_PSP.h"

using ASoundVoiceHandle_PSP = TUINT32;

struct ASoundPosition_PSP
{
	TINT iX;
	TINT iY;
	TINT iZ;
	TUINT uiMaxDistance;
};

class ASoundMixer_PSP
{
public:
	static constexpr TUINT MAX_VOICES = 4;
	static constexpr TUINT MAX_VOLUME = 4096;
	static constexpr TINT MAX_PAN = 4096;
	static constexpr TUINT DEFAULT_FREQUENCY = 44100;
	static constexpr TUINT MIN_FREQUENCY = 11025;
	static constexpr TUINT MAX_FREQUENCY = 88200;
	static constexpr TINT MAX_POSITION = 1000000;
	static constexpr ASoundVoiceHandle_PSP INVALID_VOICE = 0xffffffff;

	struct Voice
	{
		const AWave_PSP* pWave;
		TUINT32 uiSamplePosition;
		TUINT32 uiSampleStep;
		TUINT uiFrequency;
		TUINT uiVolume;
		TUINT uiAttenuation;
		TINT iPan;
		TUINT uiLeftVolume;
		TUINT uiRightVolume;
		TBOOL bActive;
		TBOOL bLoop;
	};

public:
	ASoundMixer_PSP();

	ASoundVoiceHandle_PSP Play(
	    const AWave_PSP* a_pWave,
	    TUINT a_uiVolume = MAX_VOLUME,
	    TINT a_iPan = 0,
	    TBOOL a_bLoop = TFALSE,
	    TUINT a_uiFrequency = DEFAULT_FREQUENCY
	);
	TBOOL Stop( ASoundVoiceHandle_PSP a_hVoice );
	void StopAll();
	TBOOL SetVolume( ASoundVoiceHandle_PSP a_hVoice, TUINT a_uiVolume );
	TBOOL SetPan( ASoundVoiceHandle_PSP a_hVoice, TINT a_iPan );
	TBOOL SetFrequency( ASoundVoiceHandle_PSP a_hVoice, TUINT a_uiFrequency );
	TBOOL SetPosition( ASoundVoiceHandle_PSP a_hVoice, const ASoundPosition_PSP& a_rPosition );
	TBOOL GetSpatialState(
	    ASoundVoiceHandle_PSP a_hVoice,
	    TINT& a_rPan,
	    TUINT& a_rAttenuation
	) const;

	TBOOL Mix( TINT16* a_pOutputSamples, TUINT a_uiSampleFrames );
	TUINT GetActiveVoiceCount() const;
	TBOOL IsVoiceActive( ASoundVoiceHandle_PSP a_hVoice ) const;

private:
	TINT ResolveVoice( ASoundVoiceHandle_PSP a_hVoice ) const;
	void UpdateVoiceVolumes( Voice& a_rVoice );
	TBOOL UpdateVoiceFrequency( Voice& a_rVoice, TUINT a_uiFrequency );

	Voice m_aVoices[ MAX_VOICES ];
	TUINT32 m_auiVoiceGenerations[ MAX_VOICES ];
};
