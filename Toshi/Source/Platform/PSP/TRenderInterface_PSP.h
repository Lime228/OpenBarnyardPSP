#pragma once

#include "T2Texture_PSP.h"

TOSHI_NAMESPACE_START

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
	void DrawTextureSprite( const T2Texture& a_rTexture, TFLOAT a_fX, TFLOAT a_fY, TFLOAT a_fWidth, TFLOAT a_fHeight );
	void EndScene();

	TBOOL IsDisplayCreated() const { return m_bDisplayCreated; }

private:
	alignas( 16 ) TUINT32 m_aDisplayList[ 262144 ];
	TBOOL                 m_bDisplayCreated;
};

TOSHI_NAMESPACE_END
