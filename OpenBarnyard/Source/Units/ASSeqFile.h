#pragma once

#include <Toshi/Defines.h>

class ASSeqFile
{
public:
	// Offsets are relative to the end of the header.
	struct Header
	{
		TUINT32 uiStringRelocsOffset;
		TUINT32 uiStringRelocsSize;
		TUINT32 uiStringsOffset;
		TUINT32 uiPointerRelocsOffset;
		TUINT32 uiPointerRelocsSize;
		TUINT32 uiPointersOffset;
	};

	static constexpr TINT END_INSTRUCTION = 13;
	// ASSeq::ms_aInstructionSizes from the retail game. END has no successor.
	static constexpr TINT INSTRUCTION_SIZES[] = { 0x1c, 0x14, 0x1c, 0x14, 0x1c, 0x18, 0x18, 0x14, 0x10, 0x10, 0x0c, 0x0c, 0x1c, 0x00 };

	// Returns the original bytes, still unrelocated. The caller owns them and frees them with TFree.
	static TUINT8* Read( const TCHAR* a_szPath, TINT& a_riTrackCount );
};
