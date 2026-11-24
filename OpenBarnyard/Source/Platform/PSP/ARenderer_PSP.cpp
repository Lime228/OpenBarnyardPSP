#include "pch.h"

#include "ARenderer_PSP.h"
#include "AFrontEndBarnSign_PSP.h"
#include "AFrontEndCamera_PSP.h"
#include "AFrontEndInstanceManager_PSP.h"
#include "AFrontEndTerrain_PSP.h"
#include "AFrontEndTerrainPack_PSP.h"
#include "AGameRuntimeReport_PSP.h"
#include "ASSeqTerrain_PSP.h"
#include "GUI/AGUI2.h"
#include "Movie/AMoviePlayer.h"
#include "Render/ARenderer.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

static TRenderInterface_PSP s_RenderInterface_PSP;
static AFrontEndBarnSign_PSP s_FrontEndBarnSign_PSP;
static AFrontEndCamera_PSP s_FrontEndCamera_PSP;
static AFrontEndTerrain_PSP s_FrontEndTerrain_PSP;
static AFrontEndTerrainPack_PSP s_FrontEndTerrainPack_PSP;
static AFrontEndTerrainPack_PSP s_FrontEndGrass_PSP;
static AFrontEndTerrainPack_PSP s_FrontEndGlow_PSP;
static AFrontEndTerrainPack_PSP s_FrontEndBarnnorthBase_PSP;
static AFrontEndTerrainPack_PSP s_FrontEndBarnnorthDetail_PSP;
static AFrontEndTerrainPack_PSP s_FrontEndBarnnorthGrass_PSP;
static AFrontEndTerrainPack_PSP s_FrontEndBarnnorthGlow_PSP;
static AFrontEndTerrainPack_PSP s_FrontEndBGbarneast_PSP;
static AFrontEndTerrainPack_PSP s_FrontEndBGbarnwest_PSP;
static AFrontEndTerrainPack_PSP s_FrontEndHillbase_PSP;
static AFrontEndTerrainPack_PSP s_FrontEndHilltop_PSP;
static AFrontEndTerrainPack_PSP s_FrontEndMeadowsouth_PSP;
static AFrontEndInstanceManager_PSP s_FrontEndInstanceManager_PSP;
static ASSeqTerrain_PSP s_SSeqTerrain_PSP;
static constexpr TFLOAT s_fFrontEndStartHour_PSP = 6.0f;

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

	ALoadFrontEndWorld_PSP();
	return TTRUE;
}

void ALoadFrontEndWorld_PSP()
{
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.fFrontendHour = s_fFrontEndStartHour_PSP;
	rReport.bFrontendTerrainLoaded = s_FrontEndTerrain_PSP.Load();
	rReport.uiFrontendTerrainMeshes = s_FrontEndTerrain_PSP.GetMeshCount();
	rReport.fFrontendTerrainScale = s_FrontEndTerrain_PSP.GetModelTransform().fScale;
	if ( !rReport.bFrontendTerrainLoaded )
		ASetGameRuntimeFailedPath_PSP( s_FrontEndTerrain_PSP.GetFailedPath() );
	rReport.bFrontendTerrainPackLoaded = s_FrontEndTerrainPack_PSP.Load( "frontend_detail" );
	rReport.uiFrontendTerrainPackModels = s_FrontEndTerrainPack_PSP.GetModelCount();
	rReport.uiFrontendTerrainPackTextures = s_FrontEndTerrainPack_PSP.GetTextureCount();
	if ( !rReport.bFrontendTerrainPackLoaded )
		ASetGameRuntimeFailedPath_PSP( s_FrontEndTerrainPack_PSP.GetFailedPath() );
	rReport.bFrontendGrassLoaded = s_FrontEndGrass_PSP.Load( "frontend_grass" );
	rReport.uiFrontendGrassModels = s_FrontEndGrass_PSP.GetModelCount();
	rReport.uiFrontendGrassTextures = s_FrontEndGrass_PSP.GetTextureCount();
	if ( !rReport.bFrontendGrassLoaded )
		ASetGameRuntimeFailedPath_PSP( s_FrontEndGrass_PSP.GetFailedPath() );
	rReport.bFrontendGlowLoaded = s_FrontEndGlow_PSP.Load( "frontend_glow" );
	rReport.uiFrontendGlowModels = s_FrontEndGlow_PSP.GetModelCount();
	rReport.uiFrontendGlowTextures = s_FrontEndGlow_PSP.GetTextureCount();
	if ( !rReport.bFrontendGlowLoaded )
		ASetGameRuntimeFailedPath_PSP( s_FrontEndGlow_PSP.GetFailedPath() );
	const TBOOL bBarnnorthBaseLoaded = s_FrontEndBarnnorthBase_PSP.Load( "frontend_barnnorth_base" );
	if ( !bBarnnorthBaseLoaded )
		ASetGameRuntimeFailedPath_PSP( s_FrontEndBarnnorthBase_PSP.GetFailedPath() );
	const TBOOL bBarnnorthDetailLoaded = s_FrontEndBarnnorthDetail_PSP.Load( "frontend_barnnorth_detail" );
	if ( !bBarnnorthDetailLoaded )
		ASetGameRuntimeFailedPath_PSP( s_FrontEndBarnnorthDetail_PSP.GetFailedPath() );
	const TBOOL bBarnnorthGrassLoaded = s_FrontEndBarnnorthGrass_PSP.Load( "frontend_barnnorth_grass" );
	if ( !bBarnnorthGrassLoaded )
		ASetGameRuntimeFailedPath_PSP( s_FrontEndBarnnorthGrass_PSP.GetFailedPath() );
	const TBOOL bBarnnorthGlowLoaded = s_FrontEndBarnnorthGlow_PSP.Load( "frontend_barnnorth_glow" );
	if ( !bBarnnorthGlowLoaded )
		ASetGameRuntimeFailedPath_PSP( s_FrontEndBarnnorthGlow_PSP.GetFailedPath() );
	rReport.bFrontendBarnnorthLoaded =
		bBarnnorthBaseLoaded && bBarnnorthDetailLoaded && bBarnnorthGrassLoaded && bBarnnorthGlowLoaded;
	rReport.uiFrontendBarnnorthModels =
		s_FrontEndBarnnorthBase_PSP.GetModelCount() +
		s_FrontEndBarnnorthDetail_PSP.GetModelCount() +
		s_FrontEndBarnnorthGrass_PSP.GetModelCount() +
		s_FrontEndBarnnorthGlow_PSP.GetModelCount();
	rReport.uiFrontendBarnnorthTextures =
		s_FrontEndBarnnorthBase_PSP.GetTextureCount() +
		s_FrontEndBarnnorthDetail_PSP.GetTextureCount() +
		s_FrontEndBarnnorthGrass_PSP.GetTextureCount() +
		s_FrontEndBarnnorthGlow_PSP.GetTextureCount();
	rReport.bFrontendBGbarneastLoaded = s_FrontEndBGbarneast_PSP.Load( "frontend_bgbarneast" );
	rReport.uiFrontendBGbarneastModels = s_FrontEndBGbarneast_PSP.GetModelCount();
	rReport.uiFrontendBGbarneastTextures = s_FrontEndBGbarneast_PSP.GetTextureCount();
	if ( !rReport.bFrontendBGbarneastLoaded )
		ASetGameRuntimeFailedPath_PSP( s_FrontEndBGbarneast_PSP.GetFailedPath() );
	rReport.bFrontendBGbarnwestLoaded = s_FrontEndBGbarnwest_PSP.Load( "frontend_bgbarnwest" );
	rReport.uiFrontendBGbarnwestModels = s_FrontEndBGbarnwest_PSP.GetModelCount();
	rReport.uiFrontendBGbarnwestTextures = s_FrontEndBGbarnwest_PSP.GetTextureCount();
	if ( !rReport.bFrontendBGbarnwestLoaded )
		ASetGameRuntimeFailedPath_PSP( s_FrontEndBGbarnwest_PSP.GetFailedPath() );
	rReport.bFrontendHillbaseLoaded = s_FrontEndHillbase_PSP.Load( "frontend_hillbase" );
	rReport.uiFrontendHillbaseModels = s_FrontEndHillbase_PSP.GetModelCount();
	rReport.uiFrontendHillbaseTextures = s_FrontEndHillbase_PSP.GetTextureCount();
	if ( !rReport.bFrontendHillbaseLoaded )
		ASetGameRuntimeFailedPath_PSP( s_FrontEndHillbase_PSP.GetFailedPath() );
	rReport.bFrontendHilltopLoaded = s_FrontEndHilltop_PSP.Load( "frontend_hilltop" );
	rReport.uiFrontendHilltopModels = s_FrontEndHilltop_PSP.GetModelCount();
	rReport.uiFrontendHilltopTextures = s_FrontEndHilltop_PSP.GetTextureCount();
	if ( !rReport.bFrontendHilltopLoaded )
		ASetGameRuntimeFailedPath_PSP( s_FrontEndHilltop_PSP.GetFailedPath() );
	rReport.bFrontendMeadowsouthLoaded = s_FrontEndMeadowsouth_PSP.Load( "frontend_meadowsouth" );
	rReport.uiFrontendMeadowsouthModels = s_FrontEndMeadowsouth_PSP.GetModelCount();
	rReport.uiFrontendMeadowsouthTextures = s_FrontEndMeadowsouth_PSP.GetTextureCount();
	if ( !rReport.bFrontendMeadowsouthLoaded )
		ASetGameRuntimeFailedPath_PSP( s_FrontEndMeadowsouth_PSP.GetFailedPath() );
	rReport.bFrontendInstancesLoaded = s_FrontEndInstanceManager_PSP.Load();
	rReport.uiFrontendInstancePacks = s_FrontEndInstanceManager_PSP.GetPackCount();
	rReport.uiFrontendInstances = s_FrontEndInstanceManager_PSP.GetSourceInstanceCount();
	rReport.uiFrontendInstanceModels = s_FrontEndInstanceManager_PSP.GetModelCount();
	rReport.uiFrontendInstanceTextures = s_FrontEndInstanceManager_PSP.GetTextureCount();
	if ( !rReport.bFrontendInstancesLoaded )
		ASetGameRuntimeFailedPath_PSP( s_FrontEndInstanceManager_PSP.GetFailedPath() );
	rReport.bFrontendCameraLoaded = s_FrontEndCamera_PSP.Load();
	rReport.uiFrontendCameraFrames = s_FrontEndCamera_PSP.GetFrameCount();
	rReport.uiFrontendCameraSampleRate = s_FrontEndCamera_PSP.GetSampleRate();
	if ( !rReport.bFrontendCameraLoaded )
		ASetGameRuntimeFailedPath_PSP( s_FrontEndCamera_PSP.GetFailedPath() );
	rReport.bFrontendBarnSignLoaded = s_FrontEndBarnSign_PSP.Load();
	rReport.uiFrontendBarnSignMeshes = s_FrontEndBarnSign_PSP.GetMeshCount();
	if ( !rReport.bFrontendBarnSignLoaded )
		ASetGameRuntimeFailedPath_PSP( s_FrontEndBarnSign_PSP.GetFailedPath() );
	AUpdateGameRuntimeMemory_PSP();
}

TBOOL ARenderer::OnUpdate( TFLOAT a_fDeltaTime )
{
	if ( !s_RenderInterface_PSP.IsDisplayCreated() )
		return TFALSE;

	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.uiRendererFrames++;
	AMoviePlayer* pMovie = AMoviePlayer::GetSingletonSafe();
	if ( pMovie != TNULL && pMovie->IsMoviePlaying() )
	{
		s_RenderInterface_PSP.BeginScene( 0xff000000 );
		pMovie->Render( a_fDeltaTime );
		if ( m_bRenderGUI && AGUI2::GetContext() != TNULL && AGUI2::GetRenderer() != TNULL )
			RenderGUI();
		s_RenderInterface_PSP.EndScene();
		if ( rReport.uiRendererFrames % 60 == 0 )
			AWriteGameRuntimeReport_PSP();
		return TTRUE;
	}
	s_FrontEndCamera_PSP.Update( a_fDeltaTime );
	const TRenderCamera_PSP* pFrontEndCamera = s_FrontEndCamera_PSP.IsLoaded() ?
		&s_FrontEndCamera_PSP.GetRenderCamera() : TNULL;
	if ( pFrontEndCamera != TNULL )
	{
		rReport.fFrontendCameraX = pFrontEndCamera->aPosition[ 0 ];
		rReport.fFrontendCameraY = pFrontEndCamera->aPosition[ 1 ];
		rReport.fFrontendCameraZ = pFrontEndCamera->aPosition[ 2 ];
		rReport.fFrontendCameraDirectionX = pFrontEndCamera->aDirection[ 0 ];
		rReport.fFrontendCameraDirectionY = pFrontEndCamera->aDirection[ 1 ];
		rReport.fFrontendCameraDirectionZ = pFrontEndCamera->aDirection[ 2 ];
		rReport.fFrontendCameraUpX = pFrontEndCamera->aUp[ 0 ];
		rReport.fFrontendCameraUpY = pFrontEndCamera->aUp[ 1 ];
		rReport.fFrontendCameraUpZ = pFrontEndCamera->aUp[ 2 ];
	}
	s_RenderInterface_PSP.BeginScene( 0xff101820 );
	rReport.uiFrontendTerrainDraws += s_FrontEndTerrain_PSP.Render(
		s_RenderInterface_PSP,
		pFrontEndCamera
	);
	rReport.uiFrontendTerrainPackDraws += s_FrontEndTerrainPack_PSP.Render(
		s_RenderInterface_PSP,
		pFrontEndCamera,
		s_fFrontEndStartHour_PSP
	);
	rReport.uiFrontendGrassDraws += s_FrontEndGrass_PSP.Render(
		s_RenderInterface_PSP,
		pFrontEndCamera,
		s_fFrontEndStartHour_PSP
	);
	rReport.uiFrontendBarnnorthDraws += s_FrontEndBarnnorthBase_PSP.Render(
		s_RenderInterface_PSP,
		pFrontEndCamera,
		s_fFrontEndStartHour_PSP
	);
	rReport.uiFrontendBarnnorthDraws += s_FrontEndBarnnorthDetail_PSP.Render(
		s_RenderInterface_PSP,
		pFrontEndCamera,
		s_fFrontEndStartHour_PSP
	);
	rReport.uiFrontendBarnnorthDraws += s_FrontEndBarnnorthGrass_PSP.Render(
		s_RenderInterface_PSP,
		pFrontEndCamera,
		s_fFrontEndStartHour_PSP
	);
	rReport.uiFrontendBGbarneastDraws += s_FrontEndBGbarneast_PSP.Render(
		s_RenderInterface_PSP,
		pFrontEndCamera,
		s_fFrontEndStartHour_PSP
	);
	rReport.uiFrontendBGbarnwestDraws += s_FrontEndBGbarnwest_PSP.Render(
		s_RenderInterface_PSP,
		pFrontEndCamera,
		s_fFrontEndStartHour_PSP
	);
	rReport.uiFrontendHillbaseDraws += s_FrontEndHillbase_PSP.Render(
		s_RenderInterface_PSP,
		pFrontEndCamera,
		s_fFrontEndStartHour_PSP
	);
	rReport.uiFrontendHilltopDraws += s_FrontEndHilltop_PSP.Render(
		s_RenderInterface_PSP,
		pFrontEndCamera,
		s_fFrontEndStartHour_PSP
	);
	rReport.uiFrontendMeadowsouthDraws += s_FrontEndMeadowsouth_PSP.Render(
		s_RenderInterface_PSP,
		pFrontEndCamera,
		s_fFrontEndStartHour_PSP
	);
	rReport.uiFrontendInstanceDraws += s_FrontEndInstanceManager_PSP.Render(
		s_RenderInterface_PSP,
		pFrontEndCamera,
		s_fFrontEndStartHour_PSP
	);
	rReport.uiFrontendBarnSignDraws += s_FrontEndBarnSign_PSP.Render(
		s_RenderInterface_PSP,
		pFrontEndCamera
	);
	rReport.uiFrontendGlowDraws += s_FrontEndGlow_PSP.Render(
		s_RenderInterface_PSP,
		pFrontEndCamera,
		s_fFrontEndStartHour_PSP
	);
	rReport.uiFrontendBarnnorthDraws += s_FrontEndBarnnorthGlow_PSP.Render(
		s_RenderInterface_PSP,
		pFrontEndCamera,
		s_fFrontEndStartHour_PSP
	);
	if ( m_bRenderGUI && AGUI2::GetContext() != TNULL && AGUI2::GetRenderer() != TNULL )
		RenderGUI();
	s_RenderInterface_PSP.EndScene();
	if ( rReport.uiRendererFrames == 1 || rReport.uiRendererFrames % 60 == 0 )
		AWriteGameRuntimeReport_PSP();
	return TTRUE;
}

void ARenderer::OnDestroy()
{
	AUnloadFrontEndWorld_PSP();
	s_RenderInterface_PSP.DestroyDisplay();
}

// Call between scenes so the GU has finished using the front-end meshes and textures.
void AUnloadFrontEndWorld_PSP()
{
	s_FrontEndCamera_PSP.Unload();
	s_FrontEndBarnSign_PSP.Unload();
	s_FrontEndInstanceManager_PSP.Unload();
	s_FrontEndMeadowsouth_PSP.Unload();
	s_FrontEndHilltop_PSP.Unload();
	s_FrontEndHillbase_PSP.Unload();
	s_FrontEndBGbarnwest_PSP.Unload();
	s_FrontEndBGbarneast_PSP.Unload();
	s_FrontEndBarnnorthGlow_PSP.Unload();
	s_FrontEndBarnnorthGrass_PSP.Unload();
	s_FrontEndBarnnorthDetail_PSP.Unload();
	s_FrontEndBarnnorthBase_PSP.Unload();
	s_FrontEndGlow_PSP.Unload();
	s_FrontEndGrass_PSP.Unload();
	s_FrontEndTerrainPack_PSP.Unload();
	s_FrontEndTerrain_PSP.Unload();
	AUpdateGameRuntimeMemory_PSP();
}

TBOOL ACheckNewGameTerrain_PSP()
{
	if ( !ASSeqTerrain_PSP::HasSS01Pack() )
		return TTRUE;

	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	AUnloadFrontEndWorld_PSP();
	const TUINT uiRenderBytesBefore = rReport.uiRenderMainCurrent;
	const TBOOL bLoaded = s_SSeqTerrain_PSP.LoadSS01();
	AUpdateGameRuntimeMemory_PSP();
	rReport.uiNewGameTerrainMeshes = s_SSeqTerrain_PSP.GetMeshCount();
	rReport.uiNewGameTerrainTextures = s_SSeqTerrain_PSP.GetTextureCount();
	rReport.uiNewGameTerrainBytes = rReport.uiRenderMainCurrent - uiRenderBytesBefore;
	rReport.uiNewGameTerrainHeapFree = rReport.uiHeapFree;

	// The chooser still owns the transition while the sequence state is being ported.
	s_SSeqTerrain_PSP.Unload();
	AUpdateGameRuntimeMemory_PSP();
	rReport.bNewGameTerrainReleased = rReport.uiRenderMainCurrent == uiRenderBytesBefore;
	ALoadFrontEndWorld_PSP();
	if ( !bLoaded )
		ASetGameRuntimeFailedPath_PSP( s_SSeqTerrain_PSP.GetFailedPath() );
	return bLoaded;
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
