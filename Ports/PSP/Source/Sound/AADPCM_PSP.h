#pragma once

#include "AWave_PSP.h"

struct AADPCMStreamHeader_PSP
{
	static constexpr TUINT32 MAGIC = 0x50444150;
	static constexpr TUINT32 VERSION = 1;
	static constexpr TUINT BLOCK_SAMPLE_FRAMES = 1024;
	static constexpr TUINT CHANNEL_HEADER_BYTES = 4;
	static constexpr TUINT CHANNEL_DATA_BYTES = 512;
	static constexpr TUINT BLOCK_BYTES =
	    AWave_PSP::OUTPUT_CHANNELS * ( CHANNEL_HEADER_BYTES + CHANNEL_DATA_BYTES );

	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TUINT32 uiSampleRate;
	TUINT32 uiChannels;
	TUINT32 uiSampleFrames;
	TUINT32 uiBlockSampleFrames;
	TUINT32 uiBlockBytes;
	TUINT32 uiBlockCount;
};

static_assert( sizeof( AADPCMStreamHeader_PSP ) == 32, "Unexpected PSP ADPCM header layout" );

class AADPCMDecoder_PSP
{
public:
	static TBOOL DecodeBlock(
	    const TUINT8* a_pBlock,
	    TSIZE a_uiBlockBytes,
	    TINT16* a_pOutputSamples,
	    TUINT a_uiSampleFrames
	);
};
