#include "ToshiPCH.h"
#include "T2GUIFont_PSP.h"

TOSHI_NAMESPACE_START

static const TWCHAR s_aT2GUIFontCharacters_PSP[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ-.";

static TINT T2GUIFindGlyph_PSP( TWCHAR a_wcCharacter )
{
	for ( TSIZE i = 0; i < TARRAYSIZE( s_aT2GUIFontCharacters_PSP ) - 1; i++ )
	{
		if ( s_aT2GUIFontCharacters_PSP[ i ] == a_wcCharacter )
			return TINT( i );
	}

	return -1;
}

T2GUIFont_PSP::T2GUIFont_PSP( TRenderInterface_PSP* a_pRenderInterface, const T2Texture* a_pTexture )
    : m_pRenderInterface( a_pRenderInterface )
    , m_pTexture( a_pTexture )
{
	TASSERT( m_pRenderInterface != TNULL );
}

TBOOL T2GUIFont_PSP::IsReady() const
{
	return m_pRenderInterface != TNULL && m_pTexture != TNULL &&
	       m_pTexture->GetImageData() != TNULL &&
	       m_pTexture->GetWidth() == TUINT( ATLAS_WIDTH ) &&
	       m_pTexture->GetHeight() == TUINT( ATLAS_HEIGHT );
}

void T2GUIFont_PSP::DrawTextSingleLine( const TWCHAR* a_wszText, TFLOAT a_fX, TFLOAT a_fY, TUINT32 a_uiColour, TFLOAT a_fScale ) const
{
	if ( !IsReady() || a_wszText == TNULL || a_fScale <= 0.0f )
		return;

	for ( const TWCHAR* pCharacter = a_wszText; *pCharacter != L'\0'; pCharacter++ )
	{
		const TINT iGlyph = T2GUIFindGlyph_PSP( *pCharacter );
		if ( iGlyph >= 0 )
		{
			const TFLOAT fSourceX = TFLOAT( iGlyph % ATLAS_COLUMNS * CELL_WIDTH );
			const TFLOAT fSourceY = TFLOAT( iGlyph / ATLAS_COLUMNS * CELL_HEIGHT );
			m_pRenderInterface->DrawTextureRegion(
			    *m_pTexture,
			    fSourceX,
			    fSourceY,
			    TFLOAT( GLYPH_WIDTH ),
			    TFLOAT( GLYPH_HEIGHT ),
			    a_fX,
			    a_fY,
			    GLYPH_WIDTH * a_fScale,
			    GLYPH_HEIGHT * a_fScale,
			    a_uiColour
			);
		}

		a_fX += GLYPH_ADVANCE * a_fScale;
	}
}

TFLOAT T2GUIFont_PSP::GetTextWidth( const TWCHAR* a_wszText, TFLOAT a_fScale ) const
{
	if ( a_wszText == TNULL || a_fScale <= 0.0f )
		return 0.0f;

	TSIZE uiLength = 0;
	while ( a_wszText[ uiLength ] != L'\0' )
		uiLength++;

	return uiLength == 0 ? 0.0f : ( uiLength * GLYPH_ADVANCE - 1 ) * a_fScale;
}

TOSHI_NAMESPACE_END
