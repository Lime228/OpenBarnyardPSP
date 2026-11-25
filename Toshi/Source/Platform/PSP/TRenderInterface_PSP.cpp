#include "ToshiPCH.h"
#include "TRenderInterface_PSP.h"

#include "TRenderMemory_PSP.h"

#include <Math/TMatrix44.h>
#include <Render/TRenderInterface.h>

#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include <pspgum.h>

TOSHI_NAMESPACE_START

// PSP GU has no desktop device-lost scene probe.  Keeping this platform seam
// as a no-op preserves the original state destruction contract without
// submitting an extra display list between frontend frames.
void TRenderInterface::BeginEndSceneHAL()
{
}

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

static TBOOL TRenderHasQuadArea_PSP( const TRenderQuad_PSP& a_rQuad )
{
	const TVector2& rPosition0 = a_rQuad.aPositions[ 0 ];
	const TVector2& rPosition1 = a_rQuad.aPositions[ 1 ];
	const TVector2& rPosition2 = a_rQuad.aPositions[ 2 ];
	const TVector2& rPosition3 = a_rQuad.aPositions[ 3 ];
	const TFLOAT fArea0 = ( rPosition1.x - rPosition0.x ) * ( rPosition2.y - rPosition0.y ) -
	                      ( rPosition1.y - rPosition0.y ) * ( rPosition2.x - rPosition0.x );
	const TFLOAT fArea1 = ( rPosition3.x - rPosition1.x ) * ( rPosition2.y - rPosition1.y ) -
	                      ( rPosition3.y - rPosition1.y ) * ( rPosition2.x - rPosition1.x );
	return TMath::Abs( fArea0 ) + TMath::Abs( fArea1 ) > 0.001f;
}

static TINT TRenderGetTextureAddress_PSP( TRenderTextureAddress_PSP a_eAddress )
{
	return a_eAddress == TRENDERTEXTUREADDRESS_PSP_CLAMP ? GU_CLAMP : GU_REPEAT;
}

static void TRenderSetTexture_PSP(
	const T2Texture& a_rTexture,
	TINT a_iTextureFunction,
	TRenderTextureAddress_PSP a_eAddressU,
	TRenderTextureAddress_PSP a_eAddressV,
	TINT a_iFilter = GU_NEAREST
)
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
	sceGuTexFilter( a_iFilter, a_iFilter );
	sceGuTexWrap(
		TRenderGetTextureAddress_PSP( a_eAddressU ),
		TRenderGetTextureAddress_PSP( a_eAddressV )
	);
	// GU_TRANSFORM_3D uses normalized UVs. 2D vertices supply texel coordinates.
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

static TUINT32 TRenderModulateColour_PSP( TUINT32 a_uiSource, TUINT32 a_uiModulation )
{
	TUINT32 uiResult = 0;
	for ( TUINT uiShift = 0; uiShift < 32; uiShift += 8 )
	{
		const TUINT32 uiSourceChannel = ( a_uiSource >> uiShift ) & 0xff;
		const TUINT32 uiModulationChannel = ( a_uiModulation >> uiShift ) & 0xff;
		const TUINT32 uiChannel = ( uiSourceChannel * uiModulationChannel + 127 ) / 255;
		uiResult |= uiChannel << uiShift;
	}
	return uiResult;
}

static const void* TRenderCreateModulatedVertices_PSP(
	const TModel_PSP& a_rModel,
	TUINT32 a_uiColour
)
{
	const TVertexPoolResource_PSP& rVertices = a_rModel.GetVertices();
	if ( a_uiColour == 0xffffffff )
		return rVertices.GetData();

	const TSIZE uiBufferSize = TSIZE( rVertices.GetVertexSize() ) * rVertices.GetNumVertices();
	void* pBuffer = sceGuGetMemory( uiBufferSize );
	if ( pBuffer == TNULL )
		return TNULL;

	if ( a_rModel.GetFormat() == TMODELFORMAT_PSP_TEXTURE_COLOUR_XYZ )
	{
		const TModelTextureVertex_PSP* pSource = TSTATICCAST( const TModelTextureVertex_PSP, rVertices.GetData() );
		TModelTextureVertex_PSP* pTarget = TSTATICCAST( TModelTextureVertex_PSP, pBuffer );
		for ( TUINT i = 0; i < rVertices.GetNumVertices(); i++ )
		{
			pTarget[ i ] = pSource[ i ];
			pTarget[ i ].uiColour = TRenderModulateColour_PSP( pSource[ i ].uiColour, a_uiColour );
		}
	}
	else
	{
		const TModelVertex_PSP* pSource = TSTATICCAST( const TModelVertex_PSP, rVertices.GetData() );
		TModelVertex_PSP* pTarget = TSTATICCAST( TModelVertex_PSP, pBuffer );
		for ( TUINT i = 0; i < rVertices.GetNumVertices(); i++ )
		{
			pTarget[ i ] = pSource[ i ];
			pTarget[ i ].uiColour = TRenderModulateColour_PSP( pSource[ i ].uiColour, a_uiColour );
		}
	}

	return pBuffer;
}

static void TRenderSetAlphaMode_PSP( TRenderAlphaMode_PSP a_eAlphaMode )
{
	sceGuDepthMask( GU_FALSE );
	sceGuDisable( GU_ALPHA_TEST );
	sceGuDisable( GU_BLEND );

	switch ( a_eAlphaMode )
	{
		case TRENDERALPHAMODE_PSP_MASK:
			sceGuAlphaFunc( GU_GREATER, 127, 0xff );
			sceGuEnable( GU_ALPHA_TEST );
			break;
		case TRENDERALPHAMODE_PSP_BLEND:
			sceGuAlphaFunc( GU_GREATER, 0, 0xff );
			sceGuEnable( GU_ALPHA_TEST );
			sceGuEnable( GU_BLEND );
			sceGuBlendFunc( GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0 );
			sceGuDepthMask( GU_TRUE );
			break;
		case TRENDERALPHAMODE_PSP_ADDITIVE:
			sceGuAlphaFunc( GU_GREATER, 0, 0xff );
			sceGuEnable( GU_ALPHA_TEST );
			sceGuEnable( GU_BLEND );
			sceGuBlendFunc( GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0x00ffffff );
			sceGuDepthMask( GU_TRUE );
			break;
		default:
			break;
	}
}

static void TRenderResetAlphaMode_PSP()
{
	sceGuDepthMask( GU_FALSE );
	sceGuDisable( GU_ALPHA_TEST );
	sceGuDisable( GU_BLEND );
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
	m_pDrawBuffer = TNULL;
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
	sceGuEnable( GU_BLEND );
	sceGuBlendFunc( GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0 );

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
	sceGuDisable( GU_BLEND );
}

void TRenderInterface_PSP::DrawColourQuad( const TRenderQuad_PSP& a_rQuad, TUINT32 a_uiColour )
{
	if ( !TRenderHasQuadArea_PSP( a_rQuad ) )
		return;

	sceGuDisable( GU_TEXTURE_2D );
	sceGuDisable( GU_DEPTH_TEST );
	sceGuDisable( GU_CULL_FACE );
	sceGuEnable( GU_BLEND );
	sceGuBlendFunc( GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0 );

	auto* pVertices = static_cast<TRenderColourVertex_PSP*>( sceGuGetMemory( 4 * sizeof( TRenderColourVertex_PSP ) ) );
	for ( TINT i = 0; i < 4; i++ )
	{
		pVertices[ i ] = { a_uiColour, a_rQuad.aPositions[ i ].x, a_rQuad.aPositions[ i ].y, 0.0f };
	}

	sceGuDrawArray(
	    GU_TRIANGLE_STRIP,
	    GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
	    4,
	    TNULL,
	    pVertices
	);
	sceGuDisable( GU_BLEND );
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

static void TRenderSetCamera_PSP( const TRenderCamera_PSP* a_pCamera )
{
	sceGumMatrixMode( GU_PROJECTION );
	sceGumLoadIdentity();
	sceGumPerspective(
		a_pCamera != TNULL ? a_pCamera->fFOV : 45.0f,
		TFLOAT( TRenderInterface_PSP::SCREEN_WIDTH ) / TFLOAT( TRenderInterface_PSP::SCREEN_HEIGHT ),
		a_pCamera != TNULL ? a_pCamera->fNear : 0.1f,
		a_pCamera != TNULL ? 280.0f : 100.0f
	);
	sceGumMatrixMode( GU_VIEW );
	sceGumLoadIdentity();
	if ( a_pCamera != TNULL )
	{
		ScePspFVector3 oEye = {
			a_pCamera->aPosition[ 0 ],
			a_pCamera->aPosition[ 1 ],
			a_pCamera->aPosition[ 2 ],
		};
		ScePspFVector3 oCenter = {
			a_pCamera->aPosition[ 0 ] + a_pCamera->aDirection[ 0 ],
			a_pCamera->aPosition[ 1 ] + a_pCamera->aDirection[ 1 ],
			a_pCamera->aPosition[ 2 ] + a_pCamera->aDirection[ 2 ],
		};
		ScePspFVector3 oUp = {
			-a_pCamera->aUp[ 0 ],
			-a_pCamera->aUp[ 1 ],
			-a_pCamera->aUp[ 2 ],
		};
		// Toshi stores the second camera basis as the inverse of visual world-up
		// for the retail D3D view convention. GU look-at expects visual up.
		sceGumLookAt( &oEye, &oCenter, &oUp );
	}
	sceGumMatrixMode( GU_MODEL );
	sceGumLoadIdentity();
}

void TRenderInterface_PSP::DrawModel( const TModel_PSP& a_rModel, TFLOAT a_fRotation, const T2Texture* a_pTexture )
{
	const TRenderMaterial_PSP oMaterial = {
	    a_rModel.GetFormat() == TMODELFORMAT_PSP_TEXTURE_COLOUR_XYZ && a_pTexture != TNULL,
	    TRENDERTEXTUREFUNCTION_PSP_MODULATE,
	    TRENDERCULLMODE_PSP_BACK,
	    TRENDERALPHAMODE_PSP_OPAQUE,
	    TRENDERTEXTUREADDRESS_PSP_WRAP,
	    TRENDERTEXTUREADDRESS_PSP_WRAP,
	};
	DrawModel( a_rModel, a_fRotation, oMaterial, a_pTexture );
}

void TRenderInterface_PSP::DrawModel( const TModel_PSP& a_rModel, TFLOAT a_fRotation, const TRenderMaterial_PSP& a_rMaterial, const T2Texture* a_pTexture )
{
	const TRenderSceneTransform_PSP oTransform = {
		{ 0.0f, 0.0f, -3.25f },
		{ a_fRotation * 0.5f, a_fRotation, 0.0f },
		1.0f,
	};
	DrawModel( a_rModel, oTransform, a_rMaterial, a_pTexture );
}

void TRenderInterface_PSP::DrawModel(
    const TModel_PSP& a_rModel,
    const TRenderSceneTransform_PSP& a_rTransform,
    const TRenderMaterial_PSP& a_rMaterial,
    const T2Texture* a_pTexture
)
{
	const TRenderModelTransform_PSP oModelTransform = {
		1.0f,
		{ 0.0f, 0.0f, 0.0f },
	};
	DrawModel( a_rModel, a_rTransform, a_rMaterial, a_pTexture, oModelTransform );
}

void TRenderInterface_PSP::DrawModel(
    const TModel_PSP& a_rModel,
    const TRenderSceneTransform_PSP& a_rTransform,
    const TRenderMaterial_PSP& a_rMaterial,
    const T2Texture* a_pTexture,
    const TRenderModelTransform_PSP& a_rModelTransform,
    const TRenderCamera_PSP* a_pCamera,
    TUINT32 a_uiColour
)
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
	const void* pVertexData = TRenderCreateModulatedVertices_PSP( a_rModel, a_uiColour );
	if ( pVertexData == TNULL )
		return;

	if ( a_rMaterial.bTextureEnabled )
	{
		TRenderSetTexture_PSP(
			*a_pTexture,
			TRenderGetTextureFunction_PSP( a_rMaterial.eTextureFunction ),
			a_rMaterial.eAddressU,
			a_rMaterial.eAddressV,
			GU_LINEAR
		);
	}
	else
		sceGuDisable( GU_TEXTURE_2D );
	TRenderSetCullMode_PSP( a_rMaterial.eCullMode );
	TRenderSetAlphaMode_PSP( a_rMaterial.eAlphaMode );
	sceGuEnable( GU_DEPTH_TEST );

	TRenderSetCamera_PSP( a_pCamera );

	const ScePspFVector3 oTranslation = {
		a_rTransform.aTranslation[ 0 ],
		a_rTransform.aTranslation[ 1 ],
		a_rTransform.aTranslation[ 2 ],
	};
	const ScePspFVector3 oRotation = {
		a_rTransform.aRotation[ 0 ],
		a_rTransform.aRotation[ 1 ],
		a_rTransform.aRotation[ 2 ],
	};
	const ScePspFVector3 oScale = {
		a_rTransform.fScale * a_rModelTransform.fScale,
		a_rTransform.fScale * a_rModelTransform.fScale,
		a_rTransform.fScale * a_rModelTransform.fScale,
	};
	const ScePspFVector3 oCenter = {
		-a_rModelTransform.aCenter[ 0 ],
		-a_rModelTransform.aCenter[ 1 ],
		-a_rModelTransform.aCenter[ 2 ],
	};
	sceGumTranslate( &oTranslation );
	sceGumRotateXYZ( &oRotation );
	sceGumScale( &oScale );
	sceGumTranslate( &oCenter );
	const TINT iVertexFormat = GU_COLOR_8888 | GU_VERTEX_32BITF | GU_INDEX_16BIT | GU_TRANSFORM_3D |
	                           ( bHasTextureCoordinates ? GU_TEXTURE_32BITF : 0 );
	sceGumDrawArray(
	    GU_TRIANGLES,
	    iVertexFormat,
	    rIndices.GetNumIndices(),
	    rIndices.GetData(),
	    pVertexData
	);

	sceGuDisable( GU_DEPTH_TEST );
	sceGuDisable( GU_CULL_FACE );
	TRenderResetAlphaMode_PSP();
	if ( a_rMaterial.bTextureEnabled )
		sceGuDisable( GU_TEXTURE_2D );
}

void TRenderInterface_PSP::DrawSkinnedModel(
    const TModel_PSP& a_rModel,
    const TVertexPoolResource_PSP& a_rVertices,
    TFLOAT a_fRotation,
    const TRenderMaterial_PSP& a_rMaterial,
    const T2Texture* a_pTexture,
    const TRenderModelTransform_PSP& a_rTransform
)
{
	DrawSkinnedModel( a_rModel, a_rVertices, TNULL, a_rMaterial, a_pTexture, TNULL, a_fRotation, a_rTransform );
}

TBOOL TRenderInterface_PSP::DrawSkinnedModel(
    const TModel_PSP& a_rModel,
    const TVertexPoolResource_PSP& a_rVertices,
    const TMatrix44& a_rWorld,
    const TRenderMaterial_PSP& a_rMaterial,
    const T2Texture* a_pTexture,
    const TRenderCamera_PSP& a_rCamera
)
{
	const TRenderModelTransform_PSP oTransform = { 1.0f, { 0.0f, 0.0f, 0.0f } };
	return DrawSkinnedModel( a_rModel, a_rVertices, &a_rWorld, a_rMaterial, a_pTexture, &a_rCamera, 0.0f, oTransform );
}

TBOOL TRenderInterface_PSP::DrawSkinnedModel(
    const TModel_PSP& a_rModel,
    const TVertexPoolResource_PSP& a_rVertices,
    const TMatrix44* a_pWorld,
    const TRenderMaterial_PSP& a_rMaterial,
    const T2Texture* a_pTexture,
    const TRenderCamera_PSP* a_pCamera,
    TFLOAT a_fRotation,
    const TRenderModelTransform_PSP& a_rTransform
)
{
	const TIndexPoolResource_PSP& rIndices = a_rModel.GetIndices();
	if ( !a_rModel.IsLoaded() ||
	     a_rModel.GetFormat() != TMODELFORMAT_PSP_SKIN_TEXTURE_COLOUR_XYZ ||
	     a_rVertices.GetVertexSize() != sizeof( TModelTextureVertex_PSP ) ||
	     a_rVertices.GetNumVertices() != a_rModel.GetVertices().GetNumVertices() ||
	     rIndices.GetNumIndices() == 0 ||
	     ( a_rMaterial.bTextureEnabled && ( a_pTexture == TNULL || a_pTexture->GetImageData() == TNULL ) ) )
	{
		return TFALSE;
	}

	if ( a_rMaterial.bTextureEnabled )
	{
		TRenderSetTexture_PSP(
			*a_pTexture,
			TRenderGetTextureFunction_PSP( a_rMaterial.eTextureFunction ),
			a_rMaterial.eAddressU,
			a_rMaterial.eAddressV,
			GU_LINEAR
		);
	}
	else
		sceGuDisable( GU_TEXTURE_2D );
	TRenderSetCullMode_PSP( a_rMaterial.eCullMode );
	TRenderSetAlphaMode_PSP( a_rMaterial.eAlphaMode );
	sceGuEnable( GU_DEPTH_TEST );

	TRenderSetCamera_PSP( a_pCamera );
	if ( a_pWorld != TNULL )
	{
		alignas( 16 ) ScePspFMatrix4 oWorld;
		const TVector4& rRight = a_pWorld->AsBasisVector4( BASISVECTOR_RIGHT );
		const TVector4& rUp = a_pWorld->AsBasisVector4( BASISVECTOR_UP );
		const TVector4& rForward = a_pWorld->AsBasisVector4( BASISVECTOR_FORWARD );
		const TVector4& rPosition = a_pWorld->GetTranslation();
		oWorld.x = { rRight.x, rRight.y, rRight.z, rRight.w };
		oWorld.y = { rUp.x, rUp.y, rUp.z, rUp.w };
		oWorld.z = { rForward.x, rForward.y, rForward.z, rForward.w };
		oWorld.w = { rPosition.x, rPosition.y, rPosition.z, rPosition.w };
		sceGumLoadMatrix( &oWorld );
	}
	else
	{
		const ScePspFVector3 oTranslation = { 0.0f, 0.0f, -3.25f };
		const ScePspFVector3 oRotation = { a_fRotation * 0.5f, a_fRotation, 0.0f };
		const ScePspFVector3 oScale = { a_rTransform.fScale, a_rTransform.fScale, a_rTransform.fScale };
		const ScePspFVector3 oCenter = {
			-a_rTransform.aCenter[ 0 ],
			-a_rTransform.aCenter[ 1 ],
			-a_rTransform.aCenter[ 2 ],
		};
		sceGumTranslate( &oTranslation );
		sceGumRotateXYZ( &oRotation );
		sceGumScale( &oScale );
		sceGumTranslate( &oCenter );
	}
	sceGumDrawArray(
	    GU_TRIANGLES,
	    GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_INDEX_16BIT | GU_TRANSFORM_3D,
	    rIndices.GetNumIndices(),
	    rIndices.GetData(),
	    a_rVertices.GetData()
	);

	sceGuDisable( GU_DEPTH_TEST );
	sceGuDisable( GU_CULL_FACE );
	TRenderResetAlphaMode_PSP();
	if ( a_rMaterial.bTextureEnabled )
		sceGuDisable( GU_TEXTURE_2D );
	return TTRUE;
}

void TRenderInterface_PSP::DrawTextureSprite( const T2Texture& a_rTexture, TFLOAT a_fX, TFLOAT a_fY, TFLOAT a_fWidth, TFLOAT a_fHeight )
{
	TASSERT( a_rTexture.GetImageData() != TNULL );
	TRenderSetTexture_PSP(
		a_rTexture,
		GU_TFX_REPLACE,
		TRENDERTEXTUREADDRESS_PSP_CLAMP,
		TRENDERTEXTUREADDRESS_PSP_CLAMP
	);

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

	TRenderSetTexture_PSP(
		a_rTexture,
		GU_TFX_MODULATE,
		TRENDERTEXTUREADDRESS_PSP_CLAMP,
		TRENDERTEXTUREADDRESS_PSP_CLAMP
	);
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

void TRenderInterface_PSP::DrawTextureQuad( const T2Texture& a_rTexture, const TRenderQuad_PSP& a_rQuad, TUINT32 a_uiColour )
{
	if ( a_rTexture.GetImageData() == TNULL || !TRenderHasQuadArea_PSP( a_rQuad ) )
		return;

	TRenderSetTexture_PSP(
		a_rTexture,
		GU_TFX_MODULATE,
		TRENDERTEXTUREADDRESS_PSP_CLAMP,
		TRENDERTEXTUREADDRESS_PSP_CLAMP
	);
	sceGuDisable( GU_DEPTH_TEST );
	sceGuDisable( GU_CULL_FACE );
	sceGuEnable( GU_BLEND );
	sceGuBlendFunc( GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0 );

	auto* pVertices = static_cast<TRenderTextureColourVertex_PSP*>( sceGuGetMemory( 4 * sizeof( TRenderTextureColourVertex_PSP ) ) );
	for ( TINT i = 0; i < 4; i++ )
	{
		pVertices[ i ] = {
			a_rQuad.aUVs[ i ].x,
			a_rQuad.aUVs[ i ].y,
			a_uiColour,
			a_rQuad.aPositions[ i ].x,
			a_rQuad.aPositions[ i ].y,
			0.0f,
		};
	}

	sceGuDrawArray(
	    GU_TRIANGLE_STRIP,
	    GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
	    4,
	    TNULL,
	    pVertices
	);
	sceGuDisable( GU_BLEND );
	sceGuDisable( GU_TEXTURE_2D );
}

void TRenderInterface_PSP::DrawMovieFrame( const TUINT32* a_pPixels )
{
	void* pDrawBuffer = TREINTERPRETCAST( void*, TREINTERPRETCAST( TUINTPTR, sceGeEdramGetAddr() ) +
	                                        TREINTERPRETCAST( TUINTPTR, m_pDrawBuffer ) );
	sceGuCopyImage( GU_PSM_8888, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH,
	                const_cast<TUINT32*>( a_pPixels ), 0, 0, BUFFER_WIDTH, pDrawBuffer );
	sceGuTexSync();
}

void TRenderInterface_PSP::EndScene()
{
	sceGuFinish();
	sceGuSync( GU_SYNC_FINISH, GU_SYNC_WHAT_DONE );
	sceDisplayWaitVblankStart();
	m_pDrawBuffer = sceGuSwapBuffers();
}

TOSHI_NAMESPACE_END
