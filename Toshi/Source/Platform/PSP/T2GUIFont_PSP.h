#pragma once

#include "TRenderInterface_PSP.h"

TOSHI_NAMESPACE_START

class T2GUIFont_PSP
{
public:
	static constexpr TINT GLYPH_WIDTH  = 5;
	static constexpr TINT GLYPH_HEIGHT = 7;
	static constexpr TINT GLYPH_ADVANCE = 6;
	static constexpr TINT CELL_WIDTH = 8;
	static constexpr TINT CELL_HEIGHT = 8;
	static constexpr TINT ATLAS_COLUMNS = 8;
	static constexpr TINT ATLAS_WIDTH = 64;
	static constexpr TINT ATLAS_HEIGHT = 32;

public:
	T2GUIFont_PSP( TRenderInterface_PSP* a_pRenderInterface, const T2Texture* a_pTexture );

	void DrawTextSingleLine( const TWCHAR* a_wszText, TFLOAT a_fX, TFLOAT a_fY, TUINT32 a_uiColour, TFLOAT a_fScale = 1.0f ) const;
	TFLOAT GetTextWidth( const TWCHAR* a_wszText, TFLOAT a_fScale = 1.0f ) const;
	TBOOL IsReady() const;

private:
	TRenderInterface_PSP* m_pRenderInterface;
	const T2Texture*       m_pTexture;
};

TOSHI_NAMESPACE_END
