#pragma once

#include "AWave_PSP.h"

class ASoundMixer_PSP
{
public:
	static constexpr TUINT MAX_VOICES = 4;
	static constexpr TUINT MAX_VOLUME = 4096;
	static constexpr TINT INVALID_VOICE = -1;

	struct Voice
	{
		const AWave_PSP* pWave;
		TUINT uiSampleFrame;
		TUINT uiVolume;
		TBOOL bActive;
	};

public:
	ASoundMixer_PSP();

	TINT Play( const AWave_PSP* a_pWave, TUINT a_uiVolume = MAX_VOLUME );
	void Stop( TINT a_iVoice );
	void StopAll();

	TBOOL Mix();
	const TINT16* GetOutputSamples() const { return m_aOutputSamples; }
	TUINT GetActiveVoiceCount() const;
	TBOOL IsVoiceActive( TINT a_iVoice ) const;

private:
	Voice m_aVoices[ MAX_VOICES ];
	alignas( 64 ) TINT16 m_aOutputSamples[ AWave_PSP::MAX_SAMPLE_FRAMES * AWave_PSP::OUTPUT_CHANNELS ];
};
