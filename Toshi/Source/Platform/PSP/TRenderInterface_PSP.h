#pragma once

#include "T2Texture_PSP.h"
#include "TModel_PSP.h"
#include "TRenderBuffer_PSP.h"
#include "Math/TMathInline.h"
#include "Math/TVector2.h"

TOSHI_NAMESPACE_START

struct TRenderColourVertex_PSP
{
	TUINT32 uiColour;
	TFLOAT  fX;
	TFLOAT  fY;
	TFLOAT  fZ;
};

static_assert( sizeof( TRenderColourVertex_PSP ) == 16, "Unexpected PSP colour vertex layout" );

struct TRenderQuad_PSP
{
	TVector2 aPositions[ 4 ];
	TVector2 aUVs[ 4 ];
};

enum TRenderCullMode_PSP : TUINT32
{
	TRENDERCULLMODE_PSP_NONE,
	TRENDERCULLMODE_PSP_BACK,
	TRENDERCULLMODE_PSP_FRONT,
};

enum TRenderTextureFunction_PSP : TUINT32
{
	TRENDERTEXTUREFUNCTION_PSP_REPLACE,
	TRENDERTEXTUREFUNCTION_PSP_MODULATE,
};

enum TRenderTextureAddress_PSP : TUINT32
{
	TRENDERTEXTUREADDRESS_PSP_WRAP,
	TRENDERTEXTUREADDRESS_PSP_CLAMP,
};

enum TRenderAlphaMode_PSP : TUINT32
{
	TRENDERALPHAMODE_PSP_OPAQUE,
	TRENDERALPHAMODE_PSP_MASK,
	TRENDERALPHAMODE_PSP_BLEND,
	TRENDERALPHAMODE_PSP_ADDITIVE,
};

struct TRenderMaterial_PSP
{
	TBOOL                      bTextureEnabled;
	TRenderTextureFunction_PSP eTextureFunction;
	TRenderCullMode_PSP        eCullMode;
	TRenderAlphaMode_PSP       eAlphaMode;
	TRenderTextureAddress_PSP  eAddressU;
	TRenderTextureAddress_PSP  eAddressV;
};

struct TRenderModelTransform_PSP
{
	TFLOAT fScale;
	TFLOAT aCenter[ 3 ];
};

struct TRenderSceneTransform_PSP
{
	TFLOAT aTranslation[ 3 ];
	TFLOAT aRotation[ 3 ];
	TFLOAT fScale;
};

struct TRenderCamera_PSP
{
	TFLOAT aPosition[ 3 ];
	TFLOAT aDirection[ 3 ];
	TFLOAT aUp[ 3 ];
	TFLOAT fFOV;
	TFLOAT fNear = 1.0f;
};

class TRenderInterface_PSP
{
public:
	static constexpr TINT SCREEN_WIDTH  = 480;
	static constexpr TINT SCREEN_HEIGHT = 272;
	static constexpr TINT BUFFER_WIDTH  = 512;
	static constexpr TINT FRAME_BYTES   = BUFFER_WIDTH * SCREEN_HEIGHT * sizeof( TUINT32 );
	static constexpr TINT DEPTH_BYTES   = BUFFER_WIDTH * SCREEN_HEIGHT * sizeof( TUINT16 );
	static constexpr TINT EDRAM_BYTES   = FRAME_BYTES * 2 + DEPTH_BYTES;

public:
	TRenderInterface_PSP();
	~TRenderInterface_PSP();

	TBOOL CreateDisplay();
	void  DestroyDisplay();

	void BeginScene( TUINT32 a_uiClearColour );
	void DrawColourTriangle( TFLOAT a_fXOffset );
	void DrawColourRectangle( TFLOAT a_fX, TFLOAT a_fY, TFLOAT a_fWidth, TFLOAT a_fHeight, TUINT32 a_uiColour );
	void DrawColourQuad( const TRenderQuad_PSP& a_rQuad, TUINT32 a_uiColour );
	void DrawIndexedColourTriangles( const TVertexPoolResource_PSP& a_rVertices, const TIndexPoolResource_PSP& a_rIndices );
	void DrawModel( const TModel_PSP& a_rModel, TFLOAT a_fRotation, const T2Texture* a_pTexture = TNULL );
	void DrawModel( const TModel_PSP& a_rModel, TFLOAT a_fRotation, const TRenderMaterial_PSP& a_rMaterial, const T2Texture* a_pTexture );
	void DrawModel(
	    const TModel_PSP& a_rModel,
	    const TRenderSceneTransform_PSP& a_rTransform,
	    const TRenderMaterial_PSP& a_rMaterial,
	    const T2Texture* a_pTexture
	);
	void DrawModel(
	    const TModel_PSP& a_rModel,
	    const TRenderSceneTransform_PSP& a_rTransform,
	    const TRenderMaterial_PSP& a_rMaterial,
	    const T2Texture* a_pTexture,
	    const TRenderModelTransform_PSP& a_rModelTransform,
	    const TRenderCamera_PSP* a_pCamera = TNULL,
	    TUINT32 a_uiColour = 0xffffffff
	);
	void DrawSkinnedModel(
	    const TModel_PSP& a_rModel,
	    const TVertexPoolResource_PSP& a_rVertices,
	    TFLOAT a_fRotation,
	    const TRenderMaterial_PSP& a_rMaterial,
	    const T2Texture* a_pTexture,
	    const TRenderModelTransform_PSP& a_rTransform
	);
	void DrawTextureSprite( const T2Texture& a_rTexture, TFLOAT a_fX, TFLOAT a_fY, TFLOAT a_fWidth, TFLOAT a_fHeight );
	void DrawTextureRegion(
	    const T2Texture& a_rTexture,
	    TFLOAT a_fSourceX,
	    TFLOAT a_fSourceY,
	    TFLOAT a_fSourceWidth,
	    TFLOAT a_fSourceHeight,
	    TFLOAT a_fX,
	    TFLOAT a_fY,
	    TFLOAT a_fWidth,
	    TFLOAT a_fHeight,
	    TUINT32 a_uiColour
	);
	void DrawTextureQuad( const T2Texture& a_rTexture, const TRenderQuad_PSP& a_rQuad, TUINT32 a_uiColour );
	void DrawMovieFrame( const TUINT32* a_pPixels );
	void EndScene();

	TBOOL IsDisplayCreated() const { return m_bDisplayCreated; }

private:
	alignas( 16 ) TUINT32 m_aDisplayList[ 262144 ];
	void*                 m_pDrawBuffer = TNULL;
	TBOOL                 m_bDisplayCreated;
};

TOSHI_NAMESPACE_END
