#pragma once

#include "TRenderInterface_PSP.h"

TOSHI_NAMESPACE_START

class T2GUIFont_PSP
{
public:
	static constexpr TINT GLYPH_WIDTH  = 5;
	static constexpr TINT GLYPH_HEIGHT = 7;
	static constexpr TINT GLYPH_ADVANCE = 6;

public:
	explicit T2GUIFont_PSP( TRenderInterface_PSP* a_pRenderInterface );

	void DrawTextSingleLine( const TWCHAR* a_wszText, TFLOAT a_fX, TFLOAT a_fY, TUINT32 a_uiColour, TFLOAT a_fScale = 1.0f ) const;
	TFLOAT GetTextWidth( const TWCHAR* a_wszText, TFLOAT a_fScale = 1.0f ) const;

private:
	TRenderInterface_PSP* m_pRenderInterface;
};

TOSHI_NAMESPACE_END
