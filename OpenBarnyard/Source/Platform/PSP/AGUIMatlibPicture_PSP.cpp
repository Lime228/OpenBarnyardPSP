#include "pch.h"

#include "ARenderer_PSP.h"
#include "AGameRuntimeReport_PSP.h"
#include "GUI/AGUIMatlibPicture.h"

#include <Platform/PSP/TTextureFactory_PSP.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

static T2Texture          s_aPictureTextures_PSP[ 6 ];
static TTextureFactory_PSP s_PictureTextureFactory_PSP;

TDEFINE_CLASS( AGUIMatlibPicture );

AGUIMatlibPicture::AGUIMatlibPicture()
{
	m_pMaterialLibrary = TNULL;
}

AGUIMatlibPicture::~AGUIMatlibPicture()
{
	Flush();
}

void AGUIMatlibPicture::Flush()
{
	for ( TINT i = 0; i < TARRAYSIZE( s_aPictureTextures_PSP ); i++ )
		s_aPictureTextures_PSP[ i ].Unload();

	m_iNumRects1 = 0;
	m_iNumRects2 = 0;
	m_bLoaded    = TFALSE;
}

void AGUIMatlibPicture::Cache()
{
	if ( m_bLoaded || m_Name.IsEmpty() )
		return;

	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	ASetGameRuntimePicture_PSP( m_Name.GetString8().GetString() );
	ASetGameRuntimeFailedPath_PSP( "" );
	rReport.iPictureTiles = 0;
	ASetGameRuntimePhase_PSP( "picture-loading" );

	TBOOL bLoaded = TTRUE;
	for ( TINT i = 0; i < TARRAYSIZE( s_aPictureTextures_PSP ); i++ )
	{
		TString8 strPath;
		strPath.Format( "local:%s_%d.ptex", m_Name.GetString8().GetString(), i + 1 );
		if ( !s_PictureTextureFactory_PSP.CreateTextureFromFile( s_aPictureTextures_PSP[ i ], strPath.GetString() ) )
		{
			ASetGameRuntimeFailedPath_PSP( strPath.GetString() );
			AUpdateGameRuntimeMemory_PSP();
			ASetGameRuntimePhase_PSP( "picture-failed" );
			bLoaded = TFALSE;
			break;
		}

		rReport.iPictureTiles = i + 1;
		AUpdateGameRuntimeMemory_PSP();
		AWriteGameRuntimeReport_PSP();
	}

	if ( !bLoaded )
	{
		Flush();
		return;
	}

	m_iNumRects1 = TARRAYSIZE( s_aPictureTextures_PSP );
	m_iNumRects2 = TARRAYSIZE( s_aPictureTextures_PSP );
	m_bLoaded    = TTRUE;
	ASetGameRuntimePhase_PSP( "picture-ready" );
}

void AGUIMatlibPicture::Create( const TPString8& a_rName )
{
	if ( m_Name != a_rName )
		Flush();
	m_Name = a_rName;
}

void AGUIMatlibPicture::Render()
{
	if ( !m_bIsVisible || !m_bLoaded )
		return;

	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.uiPictureRenderFrames++;
	TRenderInterface_PSP& rRender = AGetRenderInterface_PSP();
	const TFLOAT aColumnWidths_PSP[ 3 ] = { 192.0f, 192.0f, 96.0f };
	const TFLOAT aRowHeights_PSP[ 2 ]   = { 145.06667f, 126.93333f };
	const TFLOAT aSourceWidths_PSP[ 3 ] = { 256.0f, 256.0f, 128.0f };
	const TFLOAT aSourceHeights_PSP[ 2 ] = { 256.0f, 224.0f };
	TFLOAT fY = 0.0f;

	for ( TINT iRow = 0; iRow < 2; iRow++ )
	{
		TFLOAT fX = 0.0f;
		for ( TINT iColumn = 0; iColumn < 3; iColumn++ )
		{
			const TINT iTexture = iRow * 3 + iColumn;
			rRender.DrawTextureRegion(
				s_aPictureTextures_PSP[ iTexture ],
				0.0f,
				0.0f,
				aSourceWidths_PSP[ iColumn ],
				aSourceHeights_PSP[ iRow ],
				fX,
				fY,
				aColumnWidths_PSP[ iColumn ],
				aRowHeights_PSP[ iRow ],
				0xffffffff
			);
			rReport.uiPictureDraws++;
			fX += aColumnWidths_PSP[ iColumn ];
		}
		fY += aRowHeights_PSP[ iRow ];
	}
}
