#include "ToshiPCH.h"
#include "TRenderInterface_PSP.h"

#include <pspdisplay.h>
#include <pspgu.h>

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

	sceGuInit();
	sceGuStart( GU_DIRECT, m_aDisplayList );
	sceGuDrawBuffer( GU_PSM_8888, reinterpret_cast<void*>( 0 ), BUFFER_WIDTH );
	sceGuDispBuffer( SCREEN_WIDTH, SCREEN_HEIGHT, reinterpret_cast<void*>( FRAME_BYTES ), BUFFER_WIDTH );
	sceGuDepthBuffer( reinterpret_cast<void*>( FRAME_BYTES * 2 ), BUFFER_WIDTH );
	sceGuOffset( 2048 - SCREEN_WIDTH / 2, 2048 - SCREEN_HEIGHT / 2 );
	sceGuViewport( 2048, 2048, SCREEN_WIDTH, SCREEN_HEIGHT );
	sceGuDepthRange( 65535, 0 );
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
	m_bDisplayCreated = TFALSE;
}

void TRenderInterface_PSP::BeginScene( TUINT32 a_uiClearColour )
{
	TASSERT( m_bDisplayCreated );
	sceGuStart( GU_DIRECT, m_aDisplayList );
	sceGuClearColor( a_uiClearColour );
	sceGuClear( GU_COLOR_BUFFER_BIT );
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

void TRenderInterface_PSP::DrawTextureSprite( const T2Texture& a_rTexture, TFLOAT a_fX, TFLOAT a_fY, TFLOAT a_fWidth, TFLOAT a_fHeight )
{
	TASSERT( a_rTexture.GetImageData() != TNULL );
	sceGuEnable( GU_TEXTURE_2D );
	sceGuTexMode( GU_PSM_8888, 0, 0, GU_FALSE );
	sceGuTexImage(
	    0,
	    a_rTexture.GetWidth(),
	    a_rTexture.GetHeight(),
	    a_rTexture.GetBufferWidth(),
	    a_rTexture.GetImageData()
	);
	sceGuTexFunc( GU_TFX_REPLACE, GU_TCC_RGBA );
	sceGuTexFilter( GU_NEAREST, GU_NEAREST );
	sceGuTexWrap( GU_CLAMP, GU_CLAMP );

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

void TRenderInterface_PSP::EndScene()
{
	sceGuFinish();
	sceGuSync( GU_SYNC_FINISH, GU_SYNC_WHAT_DONE );
	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
}

TOSHI_NAMESPACE_END
