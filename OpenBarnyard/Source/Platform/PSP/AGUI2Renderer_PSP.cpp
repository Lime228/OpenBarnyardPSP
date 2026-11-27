#include "pch.h"

#include "AGUI2Renderer_PSP.h"
#include "AGameRuntimeReport_PSP.h"
#include "ARenderer_PSP.h"

#include "GUI/AGUI2.h"

#include <Toshi/TStringManager.h>
#include <pspgu.h>

TOSHI_NAMESPACE_USING

AGUI2Renderer_PSP::AGUI2Renderer_PSP()
    : m_iTransformCount_PSP( 0 )
    , m_uiColour_PSP( 0xffffffff )
    , m_pMaterial_PSP( TNULL )
{
	for ( TINT i = 0; i < TARRAYSIZE( m_aTextures_PSP ); i++ )
	{
		m_aTextures_PSP[ i ].szName[ 0 ]   = '\0';
		m_aTextures_PSP[ i ].uiReferences = 0;
	}
}

T2GUIMaterial* AGUI2Renderer_PSP::CreateMaterial( TTexture* )
{
	return TNULL;
}

T2GUIMaterial* AGUI2Renderer_PSP::CreateMaterial( const TCHAR* a_szTextureName )
{
	Material_PSP* pMaterial = new Material_PSP;
	if ( pMaterial == TNULL )
		return TNULL;

	TextureEntry_PSP* pTextureEntry = LoadTexture_PSP( a_szTextureName );
	if ( pTextureEntry == TNULL )
	{
		delete pMaterial;
		return TNULL;
	}

	pTextureEntry->uiReferences++;
	pMaterial->pTextureEntry = pTextureEntry;
	return pMaterial;
}

void AGUI2Renderer_PSP::DestroyMaterial( T2GUIMaterial* a_pMaterial )
{
	if ( a_pMaterial == TNULL )
		return;

	Material_PSP* pMaterial = TSTATICCAST( Material_PSP, a_pMaterial );
	TextureEntry_PSP* pTextureEntry = pMaterial->pTextureEntry;
	if ( pTextureEntry != TNULL && pTextureEntry->uiReferences > 0 )
	{
		pTextureEntry->uiReferences--;
		if ( pTextureEntry->uiReferences == 0 )
		{
			pTextureEntry->oTexture.Unload();
			pTextureEntry->szName[ 0 ] = '\0';
		}
	}

	delete pMaterial;
}

TTexture* AGUI2Renderer_PSP::GetTexture( const TCHAR* )
{
	return TNULL;
}

TUINT AGUI2Renderer_PSP::GetWidth( T2GUIMaterial* a_pMaterial )
{
	if ( a_pMaterial == TNULL )
		return 0;

	Material_PSP* pMaterial = TSTATICCAST( Material_PSP, a_pMaterial );
	return pMaterial->pTextureEntry != TNULL ? pMaterial->pTextureEntry->oTexture.GetWidth() : 0;
}

TUINT AGUI2Renderer_PSP::GetHeight( T2GUIMaterial* a_pMaterial )
{
	if ( a_pMaterial == TNULL )
		return 0;

	Material_PSP* pMaterial = TSTATICCAST( Material_PSP, a_pMaterial );
	return pMaterial->pTextureEntry != TNULL ? pMaterial->pTextureEntry->oTexture.GetHeight() : 0;
}

AGUI2Renderer_PSP::TextureEntry_PSP* AGUI2Renderer_PSP::FindTexture_PSP( const TCHAR* a_szTextureName )
{
	if ( a_szTextureName == TNULL )
		return TNULL;

	for ( TINT i = 0; i < TARRAYSIZE( m_aTextures_PSP ); i++ )
	{
		TextureEntry_PSP& rEntry = m_aTextures_PSP[ i ];
		if ( rEntry.uiReferences > 0 && TStringManager::String8CompareNoCase( rEntry.szName, a_szTextureName ) == 0 )
			return &rEntry;
	}

	return TNULL;
}

AGUI2Renderer_PSP::TextureEntry_PSP* AGUI2Renderer_PSP::LoadTexture_PSP( const TCHAR* a_szTextureName )
{
	TextureEntry_PSP* pExisting = FindTexture_PSP( a_szTextureName );
	if ( pExisting != TNULL )
		return pExisting;

	TextureEntry_PSP* pFreeEntry = TNULL;
	for ( TINT i = 0; i < TARRAYSIZE( m_aTextures_PSP ); i++ )
	{
		if ( m_aTextures_PSP[ i ].uiReferences == 0 )
		{
			pFreeEntry = &m_aTextures_PSP[ i ];
			break;
		}
	}
	if ( pFreeEntry == TNULL )
		return TNULL;

	const TCHAR* pFileName = a_szTextureName;
	for ( const TCHAR* pCharacter = a_szTextureName; *pCharacter != '\0'; pCharacter++ )
	{
		if ( *pCharacter == '\\' || *pCharacter == '/' )
			pFileName = pCharacter + 1;
	}

	TCHAR szStem[ 64 ];
	TStringManager::String8Copy( szStem, pFileName, TARRAYSIZE( szStem ) );
	for ( TCHAR* pCharacter = szStem; *pCharacter != '\0'; pCharacter++ )
	{
		if ( *pCharacter == '.' )
		{
			*pCharacter = '\0';
			break;
		}
	}

	TCHAR szPath[ 80 ];
	TStringManager::String8Format( szPath, TARRAYSIZE( szPath ), "local:%s.ptex", szStem );
	if ( !m_TextureFactory_PSP.CreateTextureFromFile( pFreeEntry->oTexture, szPath ) )
		return TNULL;

	TStringManager::String8Copy( pFreeEntry->szName, a_szTextureName, TARRAYSIZE( pFreeEntry->szName ) );
	return pFreeEntry;
}

void AGUI2Renderer_PSP::BeginScene()
{
	m_iTransformCount_PSP = 0;
	m_uiColour_PSP        = 0xffffffff;
	m_pMaterial_PSP       = TNULL;

	TFLOAT fRootWidth  = 480.0f;
	TFLOAT fRootHeight = 272.0f;
	if ( AGUI2::GetContext() != TNULL && AGUI2::GetContext()->GetRootElement() != TNULL )
		AGUI2::GetContext()->GetRootElement()->GetDimensions( fRootWidth, fRootHeight );

	// D3D applies the original negative GUI Y scale before its viewport inversion.
	// The GU uses top-left screen coordinates directly, so the final PSP scale is positive.
	m_aTransforms_PSP[ 0 ].Set(
		{ 480.0f / fRootWidth, 0.0f },
		{ 0.0f, 272.0f / fRootHeight },
		{ 240.0f, 136.0f }
	);
}

void AGUI2Renderer_PSP::EndScene()
{
}

void AGUI2Renderer_PSP::ResetRenderer()
{
	BeginScene();
}

void AGUI2Renderer_PSP::PrepareRenderer()
{
}

void AGUI2Renderer_PSP::SetMaterial( T2GUIMaterial* a_pMaterial )
{
	m_pMaterial_PSP = a_pMaterial;
}

void AGUI2Renderer_PSP::PushTransform( const T2GUITransform& a_rTransform, const TVector2& a_rAnchor, const TVector2& a_rPivot )
{
	TASSERT( m_iTransformCount_PSP + 1 < MAX_NUM_TRANSFORMS_PSP );
	if ( m_iTransformCount_PSP + 1 >= MAX_NUM_TRANSFORMS_PSP )
		return;

	T2GUITransform oParent = m_aTransforms_PSP[ m_iTransformCount_PSP ];
	T2GUITransform oLocal  = a_rTransform;
	TVector2       oPoint;

	oParent.Transform( oPoint, a_rAnchor );
	oParent.m_vecTranslation = oPoint;
	oLocal.Transform( oPoint, a_rPivot );
	oLocal.m_vecTranslation = oPoint;

	T2GUITransform::Multiply(
		m_aTransforms_PSP[ m_iTransformCount_PSP + 1 ],
		oParent,
		oLocal
	);
	m_iTransformCount_PSP++;
}

void AGUI2Renderer_PSP::PopTransform()
{
	TASSERT( m_iTransformCount_PSP > 0 );
	if ( m_iTransformCount_PSP > 0 )
		m_iTransformCount_PSP--;
}

void AGUI2Renderer_PSP::SetTransform( const T2GUITransform& a_rTransform )
{
	m_aTransforms_PSP[ m_iTransformCount_PSP ] = a_rTransform;
}

void AGUI2Renderer_PSP::SetColour( TUINT32 a_uiColour )
{
	m_uiColour_PSP = a_uiColour;
}

void AGUI2Renderer_PSP::SetScissor( TFLOAT a_fLeft, TFLOAT a_fTop, TFLOAT a_fRight, TFLOAT a_fBottom )
{
	TVector2 oTopLeft;
	TVector2 oBottomRight;
	TransformPoint( oTopLeft, { a_fLeft, a_fTop } );
	TransformPoint( oBottomRight, { a_fRight, a_fBottom } );

	TINT iLeft   = TMath::Max( TMath::FloorToInt( TMath::Min( oTopLeft.x, oBottomRight.x ) ), 0 );
	TINT iTop    = TMath::Max( TMath::FloorToInt( TMath::Min( oTopLeft.y, oBottomRight.y ) ), 0 );
	TINT iRight  = TMath::Min( TMath::CeilToInt( TMath::Max( oTopLeft.x, oBottomRight.x ) ), 480 );
	TINT iBottom = TMath::Min( TMath::CeilToInt( TMath::Max( oTopLeft.y, oBottomRight.y ) ), 272 );
	sceGuScissor( iLeft, iTop, iRight, iBottom );
}

void AGUI2Renderer_PSP::ClearScissor()
{
	sceGuScissor( 0, 0, 480, 272 );
}

void AGUI2Renderer_PSP::RenderRectangle( const TVector2& a_rPos1, const TVector2& a_rPos2, const TVector2& a_rUV1, const TVector2& a_rUV2 )
{
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.uiGUIColourDraws++;
	rReport.uiGUIQuadDraws++;
	rReport.uiLastGUIColour = m_uiColour_PSP;
	const T2GUITransform& rTransform = m_aTransforms_PSP[ m_iTransformCount_PSP ];
	if ( TMath::Abs( rTransform.m_aMatrixRows[ 0 ].y ) > 0.0001f ||
	     TMath::Abs( rTransform.m_aMatrixRows[ 1 ].x ) > 0.0001f )
	{
		rReport.uiGUIRotatedQuadDraws++;
	}

	TRenderQuad_PSP oQuad;
	TransformPoint( oQuad.aPositions[ 0 ], a_rPos1 );
	TransformPoint( oQuad.aPositions[ 1 ], { a_rPos2.x, a_rPos1.y } );
	TransformPoint( oQuad.aPositions[ 2 ], { a_rPos1.x, a_rPos2.y } );
	TransformPoint( oQuad.aPositions[ 3 ], a_rPos2 );

	const TUINT32 uiGUColour = TRenderInterface_PSP::ConvertARGBToABGR( m_uiColour_PSP );
	if ( m_pMaterial_PSP != TNULL )
	{
		Material_PSP* pMaterial = TSTATICCAST( Material_PSP, m_pMaterial_PSP );
		if ( pMaterial->pTextureEntry != TNULL )
		{
			const T2Texture& rTexture = pMaterial->pTextureEntry->oTexture;
			const TFLOAT fU1 = a_rUV1.x * rTexture.GetWidth();
			const TFLOAT fV1 = a_rUV1.y * rTexture.GetHeight();
			const TFLOAT fU2 = a_rUV2.x * rTexture.GetWidth();
			const TFLOAT fV2 = a_rUV2.y * rTexture.GetHeight();
			oQuad.aUVs[ 0 ] = { fU1, fV1 };
			oQuad.aUVs[ 1 ] = { fU2, fV1 };
			oQuad.aUVs[ 2 ] = { fU1, fV2 };
			oQuad.aUVs[ 3 ] = { fU2, fV2 };
			AGetRenderInterface_PSP().DrawTextureQuad( rTexture, oQuad, uiGUColour );
			return;
		}
	}

	AGetRenderInterface_PSP().DrawColourQuad( oQuad, uiGUColour );
}

void AGUI2Renderer_PSP::RenderTriStrip( TVector2*, TVector2*, TUINT32 )
{
}

void AGUI2Renderer_PSP::RenderLine( TFLOAT a_fX1, TFLOAT a_fY1, TFLOAT a_fX2, TFLOAT a_fY2 )
{
	RenderLine( { a_fX1, a_fY1 }, { a_fX2, a_fY2 } );
}

void AGUI2Renderer_PSP::RenderLine( const TVector2& a_rPos1, const TVector2& a_rPos2 )
{
	TVector2 oPos1;
	TVector2 oPos2;
	TransformPoint( oPos1, a_rPos1 );
	TransformPoint( oPos2, a_rPos2 );
	AGetRenderInterface_PSP().DrawColourRectangle(
		TMath::Min( oPos1.x, oPos2.x ),
		TMath::Min( oPos1.y, oPos2.y ),
		TMath::Max( TMath::Abs( oPos2.x - oPos1.x ), 1.0f ),
		TMath::Max( TMath::Abs( oPos2.y - oPos1.y ), 1.0f ),
		TRenderInterface_PSP::ConvertARGBToABGR( m_uiColour_PSP )
	);
}

void AGUI2Renderer_PSP::RenderOutlineRectangle( const TVector2& a_rPos1, const TVector2& a_rPos2 )
{
	RenderLine( a_rPos1.x, a_rPos1.y, a_rPos2.x, a_rPos1.y );
	RenderLine( a_rPos2.x, a_rPos1.y, a_rPos2.x, a_rPos2.y );
	RenderLine( a_rPos2.x, a_rPos2.y, a_rPos1.x, a_rPos2.y );
	RenderLine( a_rPos1.x, a_rPos2.y, a_rPos1.x, a_rPos1.y );
}

void AGUI2Renderer_PSP::RenderFilledRectangle( const TVector2& a_rPos1, const TVector2& a_rPos2 )
{
	RenderRectangle( a_rPos1, a_rPos2, TVector2::VEC_ZERO, TVector2::VEC_ZERO );
}

void AGUI2Renderer_PSP::ScaleCoords( TFLOAT& a_rX, TFLOAT& a_rY )
{
	TFLOAT fWidth;
	TFLOAT fHeight;
	AGUI2::GetSingleton()->GetDimensions( fWidth, fHeight );
	a_rX *= fWidth / 480.0f;
	a_rY *= fHeight / 272.0f;
}

void AGUI2Renderer_PSP::ResetZCoordinate()
{
}

void AGUI2Renderer_PSP::TransformPoint( TVector2& a_rOut, const TVector2& a_rPoint ) const
{
	m_aTransforms_PSP[ m_iTransformCount_PSP ].Transform( a_rOut, a_rPoint );
}
