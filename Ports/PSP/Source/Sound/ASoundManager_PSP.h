#pragma once

#include <Toshi/Typedefs.h>

class ASoundManager_PSP
{
public:
	static constexpr TUINT SAMPLE_RATE = 44100;
	static constexpr TUINT BUFFER_SAMPLE_FRAMES = 1024;
	static constexpr TUINT OUTPUT_CHANNELS = 2;

public:
	ASoundManager_PSP();
	~ASoundManager_PSP();

	TBOOL Create();
	void Destroy();

	TBOOL SubmitStereo( const TINT16* a_pSamples, TUINT a_uiSampleFrames );
	TBOOL SubmitSilence();

	TBOOL IsCreated() const { return m_iChannel >= 0; }
	TINT GetChannel() const { return m_iChannel; }

private:
	alignas( 64 ) TINT16 m_aOutputBuffers[ 2 ][ BUFFER_SAMPLE_FRAMES * OUTPUT_CHANNELS ];
	TINT m_iChannel;
	TUINT m_uiOutputBuffer;
};
