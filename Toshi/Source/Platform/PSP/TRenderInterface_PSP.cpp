#include "ToshiPCH.h"
#include "TRenderInterface_PSP.h"

#include "TRenderMemory_PSP.h"

#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>

TOSHI_NAMESPACE_START

struct TRenderTextureVertex_PSP
{
	TFLOAT fU;
	TFLOAT fV;
	TFLOAT fX;
	TFLOAT fY;
	TFLOAT fZ;
};

static_assert( sizeof( TRenderTextureVertex_PSP ) == 20, "Unexpected PSP texture vertex layout" );

struct TRenderTextureColourVertex_PSP
{
	TFLOAT  fU;
	TFLOAT  fV;
	TUINT32 uiColour;
	TFLOAT  fX;
	TFLOAT  fY;
	TFLOAT  fZ;
};

static_assert( sizeof( TRenderTextureColourVertex_PSP ) == 24, "Unexpected PSP texture colour vertex layout" );

static void TRenderSetTexture_PSP( const T2Texture& a_rTexture, TINT a_iTextureFunction )
{
	sceGuEnable( GU_TEXTURE_2D );
	sceGuTexMode( GU_PSM_8888, 0, 0, a_rTexture.IsSwizzled() ? GU_TRUE : GU_FALSE );
	sceGuTexImage(
	    0,
	    a_rTexture.GetWidth(),
	    a_rTexture.GetHeight(),
	    a_rTexture.GetBufferWidth(),
	    a_rTexture.GetImageData()
	);
	sceGuTexFunc( a_iTextureFunction, GU_TCC_RGBA );
	sceGuTexFilter( GU_NEAREST, GU_NEAREST );
	sceGuTexWrap( GU_CLAMP, GU_CLAMP );
	sceGuTexScale( 1.0f, 1.0f );
	sceGuTexOffset( 0.0f, 0.0f );
}

static void TRenderSetCullMode_PSP( TRenderCullMode_PSP a_eCullMode )
{
	switch ( a_eCullMode )
	{
		case TRENDERCULLMODE_PSP_BACK:
			sceGuFrontFace( GU_CCW );
			sceGuEnable( GU_CULL_FACE );
			break;
		case TRENDERCULLMODE_PSP_FRONT:
			sceGuFrontFace( GU_CW );
			sceGuEnable( GU_CULL_FACE );
			break;
		default:
			sceGuDisable( GU_CULL_FACE );
			break;
	}
}

static TINT TRenderGetTextureFunction_PSP( TRenderTextureFunction_PSP a_eTextureFunction )
{
	return a_eTextureFunction == TRENDERTEXTUREFUNCTION_PSP_REPLACE ? GU_TFX_REPLACE : GU_TFX_MODULATE;
}

TRenderInterface_PSP::TRenderInterface_PSP()
    : m_bDisplayCreated( TFALSE )
{
}

TRenderInterface_PSP::~TRenderInterface_PSP()
{
	DestroyDisplay();
}

TBOOL TRenderInterface_PSP::CreateDisplay()
{
	if ( m_bDisplayCreated )
		return TTRUE;
	if ( !TRenderMemory_PSP::SetEDRAMUsage( EDRAM_BYTES ) )
		return TFALSE;

	sceGuInit();
	sceGuStart( GU_DIRECT, m_aDisplayList );
	sceGuDrawBuffer( GU_PSM_8888, reinterpret_cast<void*>( 0 ), BUFFER_WIDTH );
	sceGuDispBuffer( SCREEN_WIDTH, SCREEN_HEIGHT, reinterpret_cast<void*>( FRAME_BYTES ), BUFFER_WIDTH );
	sceGuDepthBuffer( reinterpret_cast<void*>( FRAME_BYTES * 2 ), BUFFER_WIDTH );
	sceGuOffset( 2048 - SCREEN_WIDTH / 2, 2048 - SCREEN_HEIGHT / 2 );
	sceGuViewport( 2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT );
	sceGuDepthRange( 65535, 0 );
	sceGuDepthFunc( GU_GEQUAL );
	sceGuScissor( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT );
	sceGuEnable( GU_SCISSOR_TEST );
	sceGuDisable( GU_DEPTH_TEST );
	sceGuDisable( GU_TEXTURE_2D );
	sceGuShadeModel( GU_SMOOTH );
	sceGuFinish();
	sceGuSync( GU_SYNC_FINISH, GU_SYNC_WHAT_DONE );
	sceDisplayWaitVblankStart();
	sceGuDisplay( GU_TRUE );

	m_bDisplayCreated = TTRUE;
	return TTRUE;
}

void TRenderInterface_PSP::DestroyDisplay()
{
	if ( !m_bDisplayCreated )
		return;

	sceGuDisplay( GU_FALSE );
	sceGuTerm();
	TRenderMemory_PSP::SetEDRAMUsage( 0 );
	m_bDisplayCreated = TFALSE;
}

void TRenderInterface_PSP::BeginScene( TUINT32 a_uiClearColour )
{
	TASSERT( m_bDisplayCreated );
	sceGuStart( GU_DIRECT, m_aDisplayList );
	sceGuClearColor( a_uiClearColour );
	sceGuClearDepth( 0 );
	sceGuClear( GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT );
}

void TRenderInterface_PSP::DrawColourTriangle( TFLOAT a_fXOffset )
{
	auto* pVertices = static_cast<TRenderColourVertex_PSP*>( sceGuGetMemory( 3 * sizeof( TRenderColourVertex_PSP ) ) );
	pVertices[ 0 ]  = { 0xff5ac8fa, 240.0f + a_fXOffset, 46.0f, 0.0f };
	pVertices[ 1 ]  = { 0xff65d26e, 116.0f + a_fXOffset, 226.0f, 0.0f };
	pVertices[ 2 ]  = { 0xfff0cf65, 364.0f + a_fXOffset, 226.0f, 0.0f };

	sceGuDrawArray(
	    GU_TRIANGLES,
	    GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
	    3,
	    TNULL,
	    pVertices
	);
}

void TRenderInterface_PSP::DrawColourRectangle( TFLOAT a_fX, TFLOAT a_fY, TFLOAT a_fWidth, TFLOAT a_fHeight, TUINT32 a_uiColour )
{
	if ( a_fWidth <= 0.0f || a_fHeight <= 0.0f )
		return;

	sceGuDisable( GU_TEXTURE_2D );
	sceGuDisable( GU_DEPTH_TEST );
	sceGuDisable( GU_CULL_FACE );

	auto* pVertices = static_cast<TRenderColourVertex_PSP*>( sceGuGetMemory( 2 * sizeof( TRenderColourVertex_PSP ) ) );
	pVertices[ 0 ]  = { a_uiColour, a_fX, a_fY, 0.0f };
	pVertices[ 1 ]  = { a_uiColour, a_fX + a_fWidth, a_fY + a_fHeight, 0.0f };

	sceGuDrawArray(
	    GU_SPRITES,
	    GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
	    2,
	    TNULL,
	    pVertices
	);
}

void TRenderInterface_PSP::DrawIndexedColourTriangles( const TVertexPoolResource_PSP& a_rVertices, const TIndexPoolResource_PSP& a_rIndices )
{
	TASSERT( a_rVertices.GetVertexSize() == sizeof( TRenderColourVertex_PSP ) );
	TASSERT( !a_rVertices.IsLocked() );
	TASSERT( !a_rIndices.IsLocked() );
	if ( a_rVertices.GetData() == TNULL ||
	     a_rIndices.GetData() == TNULL ||
	     a_rVertices.GetVertexSize() != sizeof( TRenderColourVertex_PSP ) ||
	     a_rIndices.GetNumIndices() == 0 )
	{
		return;
	}

	sceGuDrawArray(
	    GU_TRIANGLES,
	    GU_COLOR_8888 | GU_VERTEX_32BITF | GU_INDEX_16BIT | GU_TRANSFORM_2D,
	    a_rIndices.GetNumIndices(),
	    a_rIndices.GetData(),
	    a_rVertices.GetData()
	);
}

void TRenderInterface_PSP::DrawModel( const TModel_PSP& a_rModel, TFLOAT a_fRotation, const T2Texture* a_pTexture )
{
	const TRenderMaterial_PSP oMaterial = {
	    a_rModel.GetFormat() == TMODELFORMAT_PSP_TEXTURE_COLOUR_XYZ && a_pTexture != TNULL,
	    TRENDERTEXTUREFUNCTION_PSP_MODULATE,
	    TRENDERCULLMODE_PSP_BACK,
	};
	DrawModel( a_rModel, a_fRotation, oMaterial, a_pTexture );
}

void TRenderInterface_PSP::DrawModel( const TModel_PSP& a_rModel, TFLOAT a_fRotation, const TRenderMaterial_PSP& a_rMaterial, const T2Texture* a_pTexture )
{
	const auto& rVertices = a_rModel.GetVertices();
	const auto& rIndices  = a_rModel.GetIndices();
	const TBOOL bHasTextureCoordinates = a_rModel.GetFormat() == TMODELFORMAT_PSP_TEXTURE_COLOUR_XYZ;
	const TUINT16 uiExpectedVertexSize = TUINT16( bHasTextureCoordinates ? sizeof( TModelTextureVertex_PSP ) : sizeof( TModelVertex_PSP ) );
	if ( !a_rModel.IsLoaded() ||
	     rVertices.GetVertexSize() != uiExpectedVertexSize ||
	     ( a_rMaterial.bTextureEnabled &&
	       ( !bHasTextureCoordinates || a_pTexture == TNULL || a_pTexture->GetImageData() == TNULL ) ) ||
	     rIndices.GetNumIndices() == 0 )
	{
		return;
	}

	if ( a_rMaterial.bTextureEnabled )
	{
		TRenderSetTexture_PSP( *a_pTexture, TRenderGetTextureFunction_PSP( a_rMaterial.eTextureFunction ) );
		sceGuTexScale( TFLOAT( a_pTexture->GetWidth() ), TFLOAT( a_pTexture->GetHeight() ) );
	}
	else
		sceGuDisable( GU_TEXTURE_2D );
	TRenderSetCullMode_PSP( a_rMaterial.eCullMode );
	sceGuEnable( GU_DEPTH_TEST );

	sceGumMatrixMode( GU_PROJECTION );
	sceGumLoadIdentity();
	sceGumPerspective( 45.0f, TFLOAT( SCREEN_WIDTH ) / TFLOAT( SCREEN_HEIGHT ), 0.1f, 100.0f );
	sceGumMatrixMode( GU_VIEW );
	sceGumLoadIdentity();
	sceGumMatrixMode( GU_MODEL );
	sceGumLoadIdentity();

	const ScePspFVector3 oTranslation = { 0.0f, 0.0f, -3.25f };
	const ScePspFVector3 oRotation    = { a_fRotation * 0.5f, a_fRotation, 0.0f };
	sceGumTranslate( &oTranslation );
	sceGumRotateXYZ( &oRotation );
	const TINT iVertexFormat = GU_COLOR_8888 | GU_VERTEX_32BITF | GU_INDEX_16BIT | GU_TRANSFORM_3D |
	                           ( bHasTextureCoordinates ? GU_TEXTURE_32BITF : 0 );
	sceGumDrawArray(
	    GU_TRIANGLES,
	    iVertexFormat,
	    rIndices.GetNumIndices(),
	    rIndices.GetData(),
	    rVertices.GetData()
	);

	sceGuDisable( GU_DEPTH_TEST );
	sceGuDisable( GU_CULL_FACE );
	if ( a_rMaterial.bTextureEnabled )
		sceGuDisable( GU_TEXTURE_2D );
}

void TRenderInterface_PSP::DrawTextureSprite( const T2Texture& a_rTexture, TFLOAT a_fX, TFLOAT a_fY, TFLOAT a_fWidth, TFLOAT a_fHeight )
{
	TASSERT( a_rTexture.GetImageData() != TNULL );
	TRenderSetTexture_PSP( a_rTexture, GU_TFX_REPLACE );

	auto* pVertices = static_cast<TRenderTextureVertex_PSP*>( sceGuGetMemory( 2 * sizeof( TRenderTextureVertex_PSP ) ) );
	pVertices[ 0 ]  = { 0.0f, 0.0f, a_fX, a_fY, 0.0f };
	pVertices[ 1 ]  = { TFLOAT( a_rTexture.GetWidth() ), TFLOAT( a_rTexture.GetHeight() ), a_fX + a_fWidth, a_fY + a_fHeight, 0.0f };

	sceGuDrawArray(
	    GU_SPRITES,
	    GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
	    2,
	    TNULL,
	    pVertices
	);
	sceGuDisable( GU_TEXTURE_2D );
}

void TRenderInterface_PSP::DrawTextureRegion(
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
)
{
	if ( a_rTexture.GetImageData() == TNULL || a_fSourceWidth <= 0.0f || a_fSourceHeight <= 0.0f ||
	     a_fWidth <= 0.0f || a_fHeight <= 0.0f )
	{
		return;
	}

	TRenderSetTexture_PSP( a_rTexture, GU_TFX_MODULATE );
	sceGuEnable( GU_BLEND );
	sceGuBlendFunc( GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0 );

	auto* pVertices = static_cast<TRenderTextureColourVertex_PSP*>( sceGuGetMemory( 2 * sizeof( TRenderTextureColourVertex_PSP ) ) );
	pVertices[ 0 ] = { a_fSourceX, a_fSourceY, a_uiColour, a_fX, a_fY, 0.0f };
	pVertices[ 1 ] = {
		a_fSourceX + a_fSourceWidth,
		a_fSourceY + a_fSourceHeight,
		a_uiColour,
		a_fX + a_fWidth,
		a_fY + a_fHeight,
		0.0f,
	};

	sceGuDrawArray(
	    GU_SPRITES,
	    GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
	    2,
	    TNULL,
	    pVertices
	);
	sceGuDisable( GU_BLEND );
	sceGuDisable( GU_TEXTURE_2D );
}

void TRenderInterface_PSP::EndScene()
{
	sceGuFinish();
	sceGuSync( GU_SYNC_FINISH, GU_SYNC_WHAT_DONE );
	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
}

TOSHI_NAMESPACE_END
