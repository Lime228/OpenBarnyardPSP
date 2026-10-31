#pragma once

#include "AWave_PSP.h"

class AWaveFactory_PSP
{
public:
	static constexpr TSIZE MAX_WAVE_FILE_SIZE = sizeof( AWaveDataHeader_PSP ) +
	                                                AWave_PSP::MAX_SAMPLE_FRAMES *
	                                                    AWave_PSP::OUTPUT_CHANNELS * sizeof( TINT16 );

public:
	TBOOL CreateWaveFromFile( AWave_PSP& a_rWave, const TCHAR* a_pFilePath ) const;
	TBOOL CreateWaveFromMemory( AWave_PSP& a_rWave, const void* a_pData, TUINT a_uiDataSize ) const;
};
