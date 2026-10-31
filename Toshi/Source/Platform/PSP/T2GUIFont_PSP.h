#pragma once

#include "TRenderInterface_PSP.h"

TOSHI_NAMESPACE_START

struct T2GUIFontDataHeader_PSP
{
	static constexpr TUINT32 MAGIC = 0x544e4650;
	static constexpr TUINT32 VERSION = 1;

	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TUINT32 uiGlyphCount;
	TUINT32 uiAtlasWidth;
	TUINT32 uiAtlasHeight;
	TUINT32 uiCellWidth;
	TUINT32 uiCellHeight;
	TUINT32 uiGlyphWidth;
	TUINT32 uiGlyphHeight;
	TUINT32 uiAdvance;
};

static_assert( sizeof( T2GUIFontDataHeader_PSP ) == 40, "Unexpected PSP font header layout" );

struct T2GUIFontGlyphData_PSP
{
	TUINT32 uiCharacter;
	TUINT16 uiSourceX;
	TUINT16 uiSourceY;
};

static_assert( sizeof( T2GUIFontGlyphData_PSP ) == 8, "Unexpected PSP font glyph layout" );

class T2GUIFont_PSP
{
public:
	static constexpr TUINT MAX_GLYPHS = 64;

public:
	T2GUIFont_PSP();
	~T2GUIFont_PSP();

	TBOOL Create(
	    TRenderInterface_PSP* a_pRenderInterface,
	    const T2Texture* a_pTexture,
	    const void* a_pData,
	    TUINT a_uiDataSize
	);
	void Destroy();

	void DrawTextSingleLine( const TWCHAR* a_wszText, TFLOAT a_fX, TFLOAT a_fY, TUINT32 a_uiColour, TFLOAT a_fScale = 1.0f ) const;
	TFLOAT GetTextWidth( const TWCHAR* a_wszText, TFLOAT a_fScale = 1.0f ) const;
	TBOOL IsReady() const;

private:
	const T2GUIFontGlyphData_PSP* FindGlyph( TWCHAR a_wcCharacter ) const;

private:
	TRenderInterface_PSP* m_pRenderInterface;
	const T2Texture*       m_pTexture;
	T2GUIFontDataHeader_PSP m_Header;
	T2GUIFontGlyphData_PSP m_aGlyphs[ MAX_GLYPHS ];
};

TOSHI_NAMESPACE_END
