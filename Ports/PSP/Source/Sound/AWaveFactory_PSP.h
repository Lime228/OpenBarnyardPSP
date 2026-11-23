#pragma once

#include "AWave_PSP.h"

class AWaveFactory_PSP
{
public:
	TBOOL CreateWaveFromFile( AWave_PSP& a_rWave, const TCHAR* a_pFilePath ) const;
	TBOOL CreateWaveFromMemory( AWave_PSP& a_rWave, const void* a_pData, TUINT a_uiDataSize ) const;
};
