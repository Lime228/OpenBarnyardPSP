#include "ToshiPCH.h"
#include "T2GUIFont_PSP.h"

#include "Toshi/TUtil.h"

TOSHI_NAMESPACE_START

T2GUIFont_PSP::T2GUIFont_PSP()
    : m_pRenderInterface( TNULL )
    , m_pTexture( TNULL )
    , m_Header{}
    , m_aGlyphs{}
{
}

T2GUIFont_PSP::~T2GUIFont_PSP()
{
	Destroy();
}

TBOOL T2GUIFont_PSP::Create(
    TRenderInterface_PSP* a_pRenderInterface,
    const T2Texture* a_pTexture,
    const void* a_pData,
    TUINT a_uiDataSize
)
{
	Destroy();
	if ( a_pRenderInterface == TNULL || a_pTexture == TNULL || a_pData == TNULL ||
	     a_uiDataSize < sizeof( T2GUIFontDataHeader_PSP ) )
	{
		return TFALSE;
	}

	const auto* pHeader = static_cast<const T2GUIFontDataHeader_PSP*>( a_pData );
	if ( pHeader->uiMagic != T2GUIFontDataHeader_PSP::MAGIC ||
	     pHeader->uiVersion != T2GUIFontDataHeader_PSP::VERSION ||
	     pHeader->uiGlyphCount == 0 || pHeader->uiGlyphCount > MAX_GLYPHS ||
	     pHeader->uiAtlasWidth != a_pTexture->GetWidth() ||
	     pHeader->uiAtlasHeight != a_pTexture->GetHeight() ||
	     pHeader->uiCellWidth == 0 || pHeader->uiCellHeight == 0 ||
	     pHeader->uiGlyphWidth == 0 || pHeader->uiGlyphHeight == 0 ||
	     pHeader->uiGlyphWidth > pHeader->uiCellWidth ||
	     pHeader->uiGlyphHeight > pHeader->uiCellHeight ||
	     pHeader->uiAdvance == 0 )
	{
		return TFALSE;
	}

	const TSIZE uiExpectedSize = sizeof( T2GUIFontDataHeader_PSP ) +
	                             pHeader->uiGlyphCount * sizeof( T2GUIFontGlyphData_PSP );
	if ( a_uiDataSize != uiExpectedSize )
		return TFALSE;

	const auto* pGlyphs = reinterpret_cast<const T2GUIFontGlyphData_PSP*>( pHeader + 1 );
	for ( TUINT i = 0; i < pHeader->uiGlyphCount; i++ )
	{
		const auto& rGlyph = pGlyphs[ i ];
		if ( rGlyph.uiSourceX + pHeader->uiGlyphWidth > pHeader->uiAtlasWidth ||
		     rGlyph.uiSourceY + pHeader->uiGlyphHeight > pHeader->uiAtlasHeight )
		{
			return TFALSE;
		}

		for ( TUINT j = 0; j < i; j++ )
		{
			if ( pGlyphs[ j ].uiCharacter == rGlyph.uiCharacter )
				return TFALSE;
		}
	}

	m_Header = *pHeader;
	TUtil::MemCopy( m_aGlyphs, pGlyphs, pHeader->uiGlyphCount * sizeof( T2GUIFontGlyphData_PSP ) );
	m_pRenderInterface = a_pRenderInterface;
	m_pTexture = a_pTexture;
	return TTRUE;
}

void T2GUIFont_PSP::Destroy()
{
	m_pRenderInterface = TNULL;
	m_pTexture = TNULL;
	m_Header = {};
	TUtil::MemClear( m_aGlyphs, sizeof( m_aGlyphs ) );
}

TBOOL T2GUIFont_PSP::IsReady() const
{
	return m_pRenderInterface != TNULL && m_pTexture != TNULL &&
	       m_pTexture->GetImageData() != TNULL && m_Header.uiGlyphCount != 0;
}

const T2GUIFontGlyphData_PSP* T2GUIFont_PSP::FindGlyph( TWCHAR a_wcCharacter ) const
{
	for ( TUINT i = 0; i < m_Header.uiGlyphCount; i++ )
	{
		if ( m_aGlyphs[ i ].uiCharacter == TUINT32( a_wcCharacter ) )
			return &m_aGlyphs[ i ];
	}

	return TNULL;
}

void T2GUIFont_PSP::DrawTextSingleLine( const TWCHAR* a_wszText, TFLOAT a_fX, TFLOAT a_fY, TUINT32 a_uiColour, TFLOAT a_fScale ) const
{
	if ( !IsReady() || a_wszText == TNULL || a_fScale <= 0.0f )
		return;

	for ( const TWCHAR* pCharacter = a_wszText; *pCharacter != L'\0'; pCharacter++ )
	{
		const auto* pGlyph = FindGlyph( *pCharacter );
		if ( pGlyph != TNULL )
		{
			m_pRenderInterface->DrawTextureRegion(
			    *m_pTexture,
			    TFLOAT( pGlyph->uiSourceX ),
			    TFLOAT( pGlyph->uiSourceY ),
			    TFLOAT( m_Header.uiGlyphWidth ),
			    TFLOAT( m_Header.uiGlyphHeight ),
			    a_fX,
			    a_fY,
			    m_Header.uiGlyphWidth * a_fScale,
			    m_Header.uiGlyphHeight * a_fScale,
			    a_uiColour
			);
		}

		a_fX += m_Header.uiAdvance * a_fScale;
	}
}

TFLOAT T2GUIFont_PSP::GetTextWidth( const TWCHAR* a_wszText, TFLOAT a_fScale ) const
{
	if ( !IsReady() || a_wszText == TNULL || a_fScale <= 0.0f )
		return 0.0f;

	TSIZE uiLength = 0;
	while ( a_wszText[ uiLength ] != L'\0' )
		uiLength++;

	return uiLength == 0 ? 0.0f :
	       ( uiLength * m_Header.uiAdvance - ( m_Header.uiAdvance - m_Header.uiGlyphWidth ) ) * a_fScale;
}

TOSHI_NAMESPACE_END
