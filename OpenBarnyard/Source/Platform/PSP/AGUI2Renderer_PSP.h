#pragma once

#include <GUI/T2GUIRenderer.h>

class AGUI2Renderer_PSP : public Toshi::T2GUIRenderer
{
public:
	static constexpr TINT MAX_NUM_TRANSFORMS_PSP = 32;

public:
	AGUI2Renderer_PSP();
	virtual ~AGUI2Renderer_PSP() = default;

	virtual Toshi::T2GUIMaterial* CreateMaterial( Toshi::TTexture* a_pTexture ) override;
	virtual Toshi::T2GUIMaterial* CreateMaterial( const TCHAR* a_szTextureName ) override;
	virtual void                   DestroyMaterial( Toshi::T2GUIMaterial* a_pMaterial ) override;
	virtual Toshi::TTexture*       GetTexture( const TCHAR* a_szTextureName ) override;
	virtual TUINT                  GetWidth( Toshi::T2GUIMaterial* a_pMaterial ) override;
	virtual TUINT                  GetHeight( Toshi::T2GUIMaterial* a_pMaterial ) override;
	virtual void                   BeginScene() override;
	virtual void                   EndScene() override;
	virtual void                   ResetRenderer() override;
	virtual void                   PrepareRenderer() override;
	virtual void                   SetMaterial( Toshi::T2GUIMaterial* a_pMaterial ) override;
	virtual void                   PushTransform( const Toshi::T2GUITransform& a_rTransform, const Toshi::TVector2& a_rAnchor, const Toshi::TVector2& a_rPivot ) override;
	virtual void                   PopTransform() override;
	virtual void                   SetTransform( const Toshi::T2GUITransform& a_rTransform ) override;
	virtual void                   SetColour( TUINT32 a_uiColour ) override;
	virtual void                   SetScissor( TFLOAT a_fLeft, TFLOAT a_fTop, TFLOAT a_fRight, TFLOAT a_fBottom ) override;
	virtual void                   ClearScissor() override;
	virtual void                   RenderRectangle( const Toshi::TVector2& a_rPos1, const Toshi::TVector2& a_rPos2, const Toshi::TVector2& a_rUV1, const Toshi::TVector2& a_rUV2 ) override;
	virtual void                   RenderTriStrip( Toshi::TVector2* a_pVertices, Toshi::TVector2* a_pUVs, TUINT32 a_uiNumVertices ) override;
	virtual void                   RenderLine( TFLOAT a_fX1, TFLOAT a_fY1, TFLOAT a_fX2, TFLOAT a_fY2 ) override;
	virtual void                   RenderLine( const Toshi::TVector2& a_rPos1, const Toshi::TVector2& a_rPos2 ) override;
	virtual void                   RenderOutlineRectangle( const Toshi::TVector2& a_rPos1, const Toshi::TVector2& a_rPos2 ) override;
	virtual void                   RenderFilledRectangle( const Toshi::TVector2& a_rPos1, const Toshi::TVector2& a_rPos2 ) override;
	virtual void                   ScaleCoords( TFLOAT& a_rX, TFLOAT& a_rY ) override;
	virtual void                   ResetZCoordinate() override;

private:
	void TransformPoint( Toshi::TVector2& a_rOut, const Toshi::TVector2& a_rPoint ) const;

private:
	Toshi::T2GUITransform m_aTransforms_PSP[ MAX_NUM_TRANSFORMS_PSP ];
	TINT                   m_iTransformCount_PSP;
	TUINT32                m_uiColour_PSP;
	Toshi::T2GUIMaterial* m_pMaterial_PSP;
};
