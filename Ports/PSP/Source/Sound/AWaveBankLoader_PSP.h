#pragma once

#include "AWaveBank_PSP.h"

struct AWaveBankDataHeader_PSP
{
	static constexpr TUINT32 MAGIC = 0x4b4e4250;
	static constexpr TUINT32 VERSION = 2;

	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TUINT32 uiWaveCount;
};

struct AWaveBankDataEntry_PSP
{
	TUINT32 uiType;
	TCHAR szName[ AWaveBank_PSP::MAX_WAVE_NAME ];
	TCHAR szPath[ 64 ];
};

static_assert( sizeof( AWaveBankDataHeader_PSP ) == 12, "Unexpected PSP wave bank header layout" );
static_assert( sizeof( AWaveBankDataEntry_PSP ) == 116, "Unexpected PSP wave bank entry layout" );

class AWaveBankLoader_PSP
{
public:
	TBOOL Load(
	    AWaveBank_PSP& a_rBank,
	    AWave_PSP* a_pWaveStorage,
	    TUINT a_uiWaveCapacity,
	    AStreamWave_PSP* a_pStreamStorage,
	    TUINT a_uiStreamCapacity,
	    const TCHAR* a_pManifestPath,
	    TUINT& a_rLoadedWaves,
	    TUINT& a_rLoadedStreams
	) const;
	void Unload(
	    AWaveBank_PSP& a_rBank,
	    AWave_PSP* a_pWaveStorage,
	    TUINT a_uiLoadedWaves,
	    AStreamWave_PSP* a_pStreamStorage,
	    TUINT a_uiLoadedStreams
	) const;
};
