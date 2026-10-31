#pragma once

#include "T2GUIFont_PSP.h"

TOSHI_NAMESPACE_START

class T2GUIFontFactory_PSP
{
public:
	static constexpr TSIZE MAX_FONT_FILE_SIZE = sizeof( T2GUIFontDataHeader_PSP ) +
	                                               T2GUIFont_PSP::MAX_GLYPHS * sizeof( T2GUIFontGlyphData_PSP );

public:
	TBOOL CreateFontFromFile(
	    T2GUIFont_PSP& a_rFont,
	    TRenderInterface_PSP* a_pRenderInterface,
	    const T2Texture* a_pTexture,
	    const TCHAR* a_pFilePath
	) const;
	TBOOL CreateFontFromMemory(
	    T2GUIFont_PSP& a_rFont,
	    TRenderInterface_PSP* a_pRenderInterface,
	    const T2Texture* a_pTexture,
	    const void* a_pData,
	    TUINT a_uiDataSize
	) const;
};

TOSHI_NAMESPACE_END
