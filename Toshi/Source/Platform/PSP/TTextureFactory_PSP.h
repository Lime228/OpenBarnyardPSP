#pragma once

#include "T2Texture_PSP.h"

TOSHI_NAMESPACE_START

class TTextureFactory_PSP
{
public:
	static constexpr TSIZE MAX_TEXTURE_FILE_SIZE = 4 * 1024 * 1024;

public:
	TBOOL CreateTextureFromFile( T2Texture& a_rTexture, const TCHAR* a_pFilePath ) const;
	TBOOL CreateTextureFromMemory( T2Texture& a_rTexture, const void* a_pData, TUINT a_uiDataSize ) const;
};

TOSHI_NAMESPACE_END
