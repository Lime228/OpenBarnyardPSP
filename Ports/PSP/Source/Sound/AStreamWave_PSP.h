#pragma once

#include "AWave_PSP.h"

TOSHI_NAMESPACE_START
class TFile;
TOSHI_NAMESPACE_END

class AStreamWave_PSP
{
public:
	static constexpr TUINT CACHE_SAMPLE_FRAMES = 1024;
	static constexpr TUINT MAX_STREAM_SAMPLE_FRAMES = 44100 * 300;

public:
	AStreamWave_PSP();
	~AStreamWave_PSP();

	TBOOL Create( const TCHAR* a_pFilePath );
	void Destroy();
	TBOOL Prepare( TUINT a_uiStartSampleFrame );
	const TINT16* GetFrame( TUINT a_uiSampleFrame ) const;

	TUINT GetSampleFrames() const { return m_oHeader.uiSampleFrames; }
	TBOOL IsCreated() const { return m_pFile != TNULL; }

private:
	Toshi::TFile* m_pFile;
	AWaveDataHeader_PSP m_oHeader;
	TUINT m_uiBufferStart;
	TUINT m_uiBufferedFrames;
	alignas( 64 ) TINT16 m_aSamples[ CACHE_SAMPLE_FRAMES * AWave_PSP::OUTPUT_CHANNELS ];
};
