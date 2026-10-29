#pragma once

#include "T2Texture_PSP.h"
#include "TModel_PSP.h"
#include "TRenderBuffer_PSP.h"

TOSHI_NAMESPACE_START

struct TRenderColourVertex_PSP
{
	TUINT32 uiColour;
	TFLOAT  fX;
	TFLOAT  fY;
	TFLOAT  fZ;
};

static_assert( sizeof( TRenderColourVertex_PSP ) == 16, "Unexpected PSP colour vertex layout" );

class TRenderInterface_PSP
{
public:
	static constexpr TINT SCREEN_WIDTH  = 480;
	static constexpr TINT SCREEN_HEIGHT = 272;
	static constexpr TINT BUFFER_WIDTH  = 512;
	static constexpr TINT FRAME_BYTES   = BUFFER_WIDTH * SCREEN_HEIGHT * sizeof( TUINT32 );

public:
	TRenderInterface_PSP();
	~TRenderInterface_PSP();

	TBOOL CreateDisplay();
	void  DestroyDisplay();

	void BeginScene( TUINT32 a_uiClearColour );
	void DrawColourTriangle( TFLOAT a_fXOffset );
	void DrawIndexedColourTriangles( const TVertexPoolResource_PSP& a_rVertices, const TIndexPoolResource_PSP& a_rIndices );
	void DrawModel( const TModel_PSP& a_rModel, TFLOAT a_fRotation, const T2Texture* a_pTexture = TNULL );
	void DrawTextureSprite( const T2Texture& a_rTexture, TFLOAT a_fX, TFLOAT a_fY, TFLOAT a_fWidth, TFLOAT a_fHeight );
	void EndScene();

	TBOOL IsDisplayCreated() const { return m_bDisplayCreated; }

private:
	alignas( 16 ) TUINT32 m_aDisplayList[ 262144 ];
	TBOOL                 m_bDisplayCreated;
};

TOSHI_NAMESPACE_END
