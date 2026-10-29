#pragma once

#include "TModel_PSP.h"

TOSHI_NAMESPACE_START

class TModelFactory_PSP
{
public:
	static constexpr TSIZE MAX_MODEL_FILE_SIZE = 2 * 1024 * 1024;

public:
	TBOOL CreateModelFromFile( TModel_PSP& a_rModel, const TCHAR* a_pFilePath ) const;
	TBOOL CreateModelFromMemory( TModel_PSP& a_rModel, const void* a_pData, TUINT a_uiDataSize ) const;
};

TOSHI_NAMESPACE_END
