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

namespace Toshi
{
class TFile;
}

class AWave_PSP
{
public:
	static constexpr TUINT MAX_SAMPLE_FRAMES = 49152;
	static constexpr TUINT OUTPUT_CHANNELS = 2;

public:
	AWave_PSP();
	~AWave_PSP();
	AWave_PSP( const AWave_PSP& ) = delete;
	AWave_PSP& operator=( const AWave_PSP& ) = delete;

	TBOOL Create( const void* a_pData, TUINT a_uiDataSize );
	TBOOL Create( Toshi::TFile& a_rFile );
	void Destroy();

	const TINT16* GetSamples() const { return m_pSamples; }
	TUINT GetSampleFrames() const { return m_oHeader.uiSampleFrames; }
	TUINT GetSampleRate() const { return m_oHeader.uiSampleRate; }
	TBOOL IsCreated() const { return m_oHeader.uiSampleFrames != 0; }

private:
	TBOOL AllocateSamples( const AWaveDataHeader_PSP& a_rcHeader, TSIZE a_uiDataSize );

	AWaveDataHeader_PSP m_oHeader;
	TINT16* m_pSamples;
};
