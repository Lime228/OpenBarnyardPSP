#include "pch.h"

#include "AGUI2Renderer_PSP.h"
#include "ARenderer_PSP.h"

#include "GUI/AGUI2.h"

#include <pspgu.h>

TOSHI_NAMESPACE_USING

AGUI2Renderer_PSP::AGUI2Renderer_PSP()
    : m_iTransformCount_PSP( 0 )
    , m_uiColour_PSP( 0xffffffff )
    , m_pMaterial_PSP( TNULL )
{
}

T2GUIMaterial* AGUI2Renderer_PSP::CreateMaterial( TTexture* )
{
	return TNULL;
}

T2GUIMaterial* AGUI2Renderer_PSP::CreateMaterial( const TCHAR* )
{
	return TNULL;
}

void AGUI2Renderer_PSP::DestroyMaterial( T2GUIMaterial* )
{
}

TTexture* AGUI2Renderer_PSP::GetTexture( const TCHAR* )
{
	return TNULL;
}

TUINT AGUI2Renderer_PSP::GetWidth( T2GUIMaterial* )
{
	return 0;
}

TUINT AGUI2Renderer_PSP::GetHeight( T2GUIMaterial* )
{
	return 0;
}

void AGUI2Renderer_PSP::BeginScene()
{
	m_iTransformCount_PSP = 0;
	m_uiColour_PSP        = 0xffffffff;
	m_pMaterial_PSP       = TNULL;
	m_aTransforms_PSP[ 0 ].Set(
		{ 1.0f, 0.0f },
		{ 0.0f, -1.0f },
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

void AGUI2Renderer_PSP::RenderRectangle( const TVector2& a_rPos1, const TVector2& a_rPos2, const TVector2&, const TVector2& )
{
	TVector2 oPos1;
	TVector2 oPos2;
	TransformPoint( oPos1, a_rPos1 );
	TransformPoint( oPos2, a_rPos2 );

	const TFLOAT fX      = TMath::Min( oPos1.x, oPos2.x );
	const TFLOAT fY      = TMath::Min( oPos1.y, oPos2.y );
	const TFLOAT fWidth  = TMath::Abs( oPos2.x - oPos1.x );
	const TFLOAT fHeight = TMath::Abs( oPos2.y - oPos1.y );

	// The bounded startup slice currently uses untextured GUI rectangles for
	// fades and overlays.  Retail picture tiles are submitted by
	// AGUIMatlibPicture through the same GU display list.
	AGetRenderInterface_PSP().DrawColourRectangle( fX, fY, fWidth, fHeight, m_uiColour_PSP );
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
		m_uiColour_PSP
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
