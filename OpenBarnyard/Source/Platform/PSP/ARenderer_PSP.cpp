#include "pch.h"

#include "ARenderer_PSP.h"
#include "AFrontEndTerrain_PSP.h"
#include "AGameRuntimeReport_PSP.h"
#include "GUI/AGUI2.h"
#include "Render/ARenderer.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

static TRenderInterface_PSP s_RenderInterface_PSP;
static AFrontEndTerrain_PSP s_FrontEndTerrain_PSP;

TDEFINE_CLASS( ARenderer );

TRenderInterface_PSP& AGetRenderInterface_PSP()
{
	return s_RenderInterface_PSP;
}

ARenderer::ARenderer()
    : m_RenderGUIEmitter( this )
    , m_AnimationUpdateStartEmitter( this )
    , m_AnimationUpdateEndEmitter( this )
    , m_pViewport( TNULL )
    , m_pCameraObject( TNULL )
    , m_pHALViewport1( TNULL )
    , m_pHALViewport2( TNULL )
    , m_fFar( 280.0f )
    , m_fNear( 1.0f )
    , m_eScreenCaptureStatus( SCREENCAPTURE_NONE )
    , m_pScreenCapture( TNULL )
    , m_pScreenCaptureBuffer( TNULL )
    , m_szScreenCaptureName( TNULL )
    , m_uiScreenCaptureWidth( 0 )
    , m_uiScreenCaptureHeight( 0 )
    , m_pSkyDome( TNULL )
    , m_iScreenshotIndex( 0 )
    , m_bRenderGUI( TTRUE )
{
}

ARenderer::~ARenderer() = default;

TBOOL ARenderer::OnCreate()
{
	m_DisplayModes.Initialise();
	if ( !s_RenderInterface_PSP.CreateDisplay() )
		return TFALSE;

	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.bFrontendTerrainLoaded = s_FrontEndTerrain_PSP.Load();
	rReport.uiFrontendTerrainMeshes = s_FrontEndTerrain_PSP.GetMeshCount();
	rReport.fFrontendTerrainScale = s_FrontEndTerrain_PSP.GetModelTransform().fScale;
	if ( !rReport.bFrontendTerrainLoaded )
		ASetGameRuntimeFailedPath_PSP( s_FrontEndTerrain_PSP.GetFailedPath() );
	AUpdateGameRuntimeMemory_PSP();
	return TTRUE;
}

TBOOL ARenderer::OnUpdate( TFLOAT )
{
	if ( !s_RenderInterface_PSP.IsDisplayCreated() )
		return TFALSE;

	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.uiRendererFrames++;
	s_RenderInterface_PSP.BeginScene( 0xff101820 );
	rReport.uiFrontendTerrainDraws += s_FrontEndTerrain_PSP.Render( s_RenderInterface_PSP );
	if ( m_bRenderGUI && AGUI2::GetContext() != TNULL && AGUI2::GetRenderer() != TNULL )
		RenderGUI();
	s_RenderInterface_PSP.EndScene();
	if ( rReport.uiRendererFrames == 1 || rReport.uiRendererFrames % 60 == 0 )
		AWriteGameRuntimeReport_PSP();
	return TTRUE;
}

void ARenderer::OnDestroy()
{
	s_FrontEndTerrain_PSP.Unload();
	s_RenderInterface_PSP.DestroyDisplay();
}

void ARenderer::RenderGUI()
{
	AGUI2::GetRenderer()->BeginScene();
	m_RenderGUIEmitter.Throw( 0 );
	AGUI2::GetRenderer()->EndScene();
	AGUI2::GetContext()->Render();
}

void ARenderer::ForceUpdate30FPS()
{
	OnUpdate( 1.0f / 30.0f );
}
