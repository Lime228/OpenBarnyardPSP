#pragma once

#include <Toshi/Typedefs.h>

struct AWaveDataHeader_PSP
{
	static constexpr TUINT32 MAGIC = 0x4d435050;
	static constexpr TUINT32 VERSION = 1;

	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TUINT32 uiSampleRate;
	TUINT32 uiChannels;
	TUINT32 uiSampleFrames;
};

static_assert( sizeof( AWaveDataHeader_PSP ) == 20, "Unexpected PSP wave header layout" );

class AWave_PSP
{
public:
	static constexpr TUINT MAX_SAMPLE_FRAMES = 1024;
	static constexpr TUINT OUTPUT_CHANNELS = 2;

public:
	AWave_PSP();

	TBOOL Create( const void* a_pData, TUINT a_uiDataSize );
	void Destroy();

	const TINT16* GetSamples() const { return m_aSamples; }
	TUINT GetSampleFrames() const { return m_oHeader.uiSampleFrames; }
	TUINT GetSampleRate() const { return m_oHeader.uiSampleRate; }
	TBOOL IsCreated() const { return m_oHeader.uiSampleFrames != 0; }

private:
	AWaveDataHeader_PSP m_oHeader;
	alignas( 64 ) TINT16 m_aSamples[ MAX_SAMPLE_FRAMES * OUTPUT_CHANNELS ];
};
