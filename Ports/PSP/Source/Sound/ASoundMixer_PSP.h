#pragma once

#include "AWave_PSP.h"

class ASoundMixer_PSP
{
public:
	static constexpr TUINT MAX_VOICES = 4;
	static constexpr TUINT MAX_VOLUME = 4096;
	static constexpr TINT MAX_PAN = 4096;
	static constexpr TINT INVALID_VOICE = -1;

	struct Voice
	{
		const AWave_PSP* pWave;
		TUINT uiSampleFrame;
		TUINT uiLeftVolume;
		TUINT uiRightVolume;
		TBOOL bActive;
		TBOOL bLoop;
	};

public:
	ASoundMixer_PSP();

	TINT Play(
	    const AWave_PSP* a_pWave,
	    TUINT a_uiVolume = MAX_VOLUME,
	    TINT a_iPan = 0,
	    TBOOL a_bLoop = TFALSE
	);
	void Stop( TINT a_iVoice );
	void StopAll();

	TBOOL Mix( TINT16* a_pOutputSamples, TUINT a_uiSampleFrames );
	TUINT GetActiveVoiceCount() const;
	TBOOL IsVoiceActive( TINT a_iVoice ) const;

private:
	Voice m_aVoices[ MAX_VOICES ];
};
