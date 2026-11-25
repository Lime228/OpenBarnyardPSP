#include "pch.h"

#include "ARenderer_PSP.h"
#include "AFrontEndBarnSign_PSP.h"
#include "AFrontEndCamera_PSP.h"
#include "AFrontEndInstanceManager_PSP.h"
#include "AFrontEndTerrain_PSP.h"
#include "AFrontEndTerrainPack_PSP.h"
#include "AGameRuntimeReport_PSP.h"
#include "ASSeqActors_PSP.h"
#include "ASSeqTerrain_PSP.h"
#include "ASkyDome_PSP.h"
#include "AVOStream_PSP.h"
#include "GUI/AGUI2.h"
#include "Movie/AMoviePlayer.h"
#include "Render/ARenderer.h"

#include <File/TFile.h>
#include <Math/TMathInline.h>

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
static ASkyDome_PSP s_SSeqSkyDome_PSP;
static ASSeqActors_PSP s_SSeqActors_PSP;
static constexpr TFLOAT s_fFrontEndStartHour_PSP = 6.0f;
static constexpr TCHAR s_szOpeningConfig_PSP[] = "local:Data/ScriptedSequence/SS01/opening.psco";
static AFrontEndCamera_PSP s_aSSeqOpeningCameras_PSP[ AGameRuntimeReport_PSP::MAX_OPENING_SHOTS ];
static TFLOAT s_afOpeningDurations_PSP[ AGameRuntimeReport_PSP::MAX_OPENING_SHOTS ];
static TFLOAT s_fOpeningTotal_PSP = 0.0f;
static TUINT s_uiOpeningShots_PSP = 0;
static TFLOAT s_fOpeningElapsed_PSP = 0.0f;
static TFLOAT s_fOpeningShotProgress_PSP = 0.0f;

struct ASSeqOpeningShot_PSP
{
	TFLOAT fHorizontalFOV;
	TFLOAT fNear;
	TFLOAT fDuration;
};

static const TRenderCamera_PSP& AUpdateSSeqOpeningCamera_PSP( TFLOAT a_fDeltaTime, AGameRuntimeReport_PSP& a_rReport )
{
	if ( s_uiOpeningShots_PSP == 1 )
	{
		a_rReport.uiNewGameOpeningShot = 1;
		s_aSSeqOpeningCameras_PSP[ 0 ].Update( a_fDeltaTime );
		return s_aSSeqOpeningCameras_PSP[ 0 ].GetRenderCamera();
	}

	if ( s_fOpeningElapsed_PSP < s_fOpeningTotal_PSP )
	{
		const TINT iVoiceMS = AGetSSeqVOTime_PSP();
		if ( iVoiceMS >= 0 )
		{
			a_rReport.iNewGameVoiceMS = iVoiceMS;
			a_rReport.uiNewGameVoiceUnderruns = AGetSSeqVOUnderruns_PSP();
			s_fOpeningElapsed_PSP = TMath::Max( s_fOpeningElapsed_PSP, TFLOAT( iVoiceMS ) * 0.001f );
		}
		else if ( TMath::IsFinite( a_fDeltaTime ) && a_fDeltaTime > 0.0f )
			s_fOpeningElapsed_PSP += a_fDeltaTime;
		if ( s_fOpeningElapsed_PSP >= s_fOpeningTotal_PSP )
		{
			s_fOpeningElapsed_PSP = s_fOpeningTotal_PSP;
			AStopSSeqVO_PSP();
		}
	}

	TUINT uiShot = 0;
	TFLOAT fShotElapsed = s_fOpeningElapsed_PSP;
	while ( uiShot + 1 < s_uiOpeningShots_PSP && fShotElapsed >= s_afOpeningDurations_PSP[ uiShot ] )
	{
		fShotElapsed -= s_afOpeningDurations_PSP[ uiShot ];
		uiShot++;
	}
	AFrontEndCamera_PSP& rCamera = s_aSSeqOpeningCameras_PSP[ uiShot ];
	const TFLOAT fSampleDuration = TFLOAT( rCamera.GetFrameCount() - 1 ) / rCamera.GetSampleRate();
	s_fOpeningShotProgress_PSP = TMath::Min( fShotElapsed / s_afOpeningDurations_PSP[ uiShot ], 1.0f );
	rCamera.SetTime( s_fOpeningShotProgress_PSP * fSampleDuration );
	a_rReport.uiNewGameOpeningShot = uiShot + 1;
	return rCamera.GetRenderCamera();
}

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
	if ( s_uiOpeningShots_PSP != 0 )
	{
		const TUINT uiPreviousShot = rReport.uiNewGameOpeningShot;
		const TRenderCamera_PSP& rCamera = AUpdateSSeqOpeningCamera_PSP( a_fDeltaTime, rReport );
		const TUINT uiShot = rReport.uiNewGameOpeningShot - 1;
		if ( uiPreviousShot != rReport.uiNewGameOpeningShot && !s_SSeqActors_PSP.SetShot( uiShot ) )
		{
			ASetGameRuntimeFailedPath_PSP( s_SSeqActors_PSP.GetFailedPath() );
			AStopSSeqOpening_PSP( TTRUE );
			return TTRUE;
		}
		s_RenderInterface_PSP.BeginScene( 0xff000000 );
		rReport.uiNewGameSkyDraws += s_SSeqSkyDome_PSP.Render( s_RenderInterface_PSP, rCamera, a_fDeltaTime );
		rReport.uiNewGameOpeningDraws += s_SSeqTerrain_PSP.Render( s_RenderInterface_PSP, rCamera );
		const TUINT uiActorDraws = s_SSeqActors_PSP.Render(
		    s_RenderInterface_PSP, rCamera, uiShot, s_fOpeningShotProgress_PSP, s_afOpeningDurations_PSP[ uiShot ] );
		rReport.auiNewGameActorDraws[ uiShot ] = TMath::Max( rReport.auiNewGameActorDraws[ uiShot ], uiActorDraws );
		s_RenderInterface_PSP.EndScene();
		rReport.uiNewGameOpeningFrames++;
		if ( uiPreviousShot != rReport.uiNewGameOpeningShot || rReport.uiRendererFrames % 60 == 0 )
			AWriteGameRuntimeReport_PSP();
		return TTRUE;
	}

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
	AStopSSeqOpening_PSP( TFALSE );
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

TBOOL AHasSSeqOpening_PSP()
{
	TFile* pFile = TFile::Create( s_szOpeningConfig_PSP, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TFALSE;
	pFile->Destroy();
	return TTRUE;
}

static TBOOL AReadSSeqOpening_PSP( ASSeqOpeningShot_PSP* a_pShots, TUINT32& a_ruiShotCount, TINT32& a_riVoice )
{
	static_assert( sizeof( ASSeqOpeningShot_PSP ) == 12 );
	TFile* pFile = TFile::Create( s_szOpeningConfig_PSP, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TFALSE;
	TUINT32 aHeader[ 2 ];
	TBOOL bRead = pFile->Read( aHeader, sizeof( aHeader ) ) == sizeof( aHeader ) && aHeader[ 0 ] == 0x4f435350;
	if ( bRead && aHeader[ 1 ] == 1 )
	{
		// The previous camera-only package stored FOV and near plane, without a timeline.
		a_ruiShotCount = 1;
		a_riVoice = -1;
		bRead = pFile->GetSize() == 16 && pFile->Read( &a_pShots[ 0 ], 8 ) == 8;
	}
	else if ( bRead && aHeader[ 1 ] == 2 )
	{
		bRead = pFile->Read( &a_ruiShotCount, sizeof( a_ruiShotCount ) ) == sizeof( a_ruiShotCount ) &&
		        pFile->Read( &a_riVoice, sizeof( a_riVoice ) ) == sizeof( a_riVoice ) && a_riVoice >= 0 &&
		        a_ruiShotCount >= 2 && a_ruiShotCount <= TARRAYSIZE( s_aSSeqOpeningCameras_PSP );
		if ( bRead )
		{
			const TSIZE uiBytes = a_ruiShotCount * sizeof( ASSeqOpeningShot_PSP );
			bRead = pFile->GetSize() == 16 + uiBytes && pFile->Read( a_pShots, uiBytes ) == uiBytes;
		}
	}
	else
		bRead = TFALSE;
	pFile->Destroy();
	return bRead;
}

TBOOL AStartSSeqOpening_PSP()
{
	ASSeqOpeningShot_PSP aShots[ TARRAYSIZE( s_aSSeqOpeningCameras_PSP ) ] = {};
	TUINT32 uiShotCount = 0;
	TINT32 iVoice = -1;
	if ( !AReadSSeqOpening_PSP( aShots, uiShotCount, iVoice ) )
	{
		ASetGameRuntimeFailedPath_PSP( s_szOpeningConfig_PSP );
		return TFALSE;
	}
	const TFLOAT fAspect = TFLOAT( TRenderInterface_PSP::SCREEN_WIDTH ) / TRenderInterface_PSP::SCREEN_HEIGHT;
	s_fOpeningTotal_PSP = 0.0f;
	for ( TUINT i = 0; i < uiShotCount; i++ )
	{
		const ASSeqOpeningShot_PSP& rShot = aShots[ i ];
		if ( !( rShot.fHorizontalFOV >= 1.0f && rShot.fHorizontalFOV < 170.0f ) ||
		     !( rShot.fNear > 0.0f && rShot.fNear < 280.0f ) ||
		     ( uiShotCount > 1 && !( rShot.fDuration > 0.0f && rShot.fDuration <= 60.0f ) ) )
		{
			AStopSSeqOpening_PSP( TFALSE );
			ASetGameRuntimeFailedPath_PSP( s_szOpeningConfig_PSP );
			return TFALSE;
		}
		const TFLOAT fHalfFOV = TMath::DegToRad( rShot.fHorizontalFOV * 0.5f );
		const TFLOAT fVerticalFOV = TMath::ATan( TMath::Tan( fHalfFOV ) / fAspect ) * ( 360.0f / TMath::PI );
		TCHAR szCameraPath[ 64 ];
		if ( i == 0 )
			TStringManager::String8Copy( szCameraPath, "local:Data/ScriptedSequence/SS01/opening.pcam", sizeof( szCameraPath ) );
		else
			TStringManager::String8Format( szCameraPath, sizeof( szCameraPath ), "local:Data/ScriptedSequence/SS01/opening-%u.pcam", i );
		if ( !s_aSSeqOpeningCameras_PSP[ i ].Load( szCameraPath, fVerticalFOV, rShot.fNear ) )
		{
			AStopSSeqOpening_PSP( TFALSE );
			ASetGameRuntimeFailedPath_PSP( szCameraPath );
			return TFALSE;
		}
		s_afOpeningDurations_PSP[ i ] = rShot.fDuration;
		s_fOpeningTotal_PSP += rShot.fDuration;
	}

	AUnloadFrontEndWorld_PSP();
	if ( !s_SSeqTerrain_PSP.LoadSS01() )
	{
		AStopSSeqOpening_PSP( TTRUE );
		ASetGameRuntimeFailedPath_PSP( s_SSeqTerrain_PSP.GetFailedPath() );
		return TFALSE;
	}
	if ( !s_SSeqSkyDome_PSP.Load() )
	{
		AStopSSeqOpening_PSP( TTRUE );
		ASetGameRuntimeFailedPath_PSP( s_SSeqSkyDome_PSP.GetFailedPath() );
		return TFALSE;
	}
	if ( uiShotCount > 1 && !s_SSeqActors_PSP.Load( uiShotCount ) )
	{
		AStopSSeqOpening_PSP( TTRUE );
		ASetGameRuntimeFailedPath_PSP( s_SSeqActors_PSP.GetFailedPath() );
		return TFALSE;
	}
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.uiNewGameTerrainMeshes = s_SSeqTerrain_PSP.GetMeshCount();
	rReport.uiNewGameTerrainTextures = s_SSeqTerrain_PSP.GetTextureCount();
	rReport.uiNewGameOpeningDraws = 0;
	rReport.uiNewGameOpeningFrames = 0;
	rReport.uiNewGameSkyDraws = 0;
	rReport.uiNewGameOpeningShot = 0;
	for ( TUINT& ruiDraws : rReport.auiNewGameActorDraws )
		ruiDraws = 0;
	rReport.iNewGameVoiceMS = -1;
	rReport.uiNewGameVoiceUnderruns = 0;
	rReport.uiNewGameVoiceNonSilentBlocks = 0;
	s_fOpeningElapsed_PSP = 0.0f;
	s_fOpeningShotProgress_PSP = 0.0f;
	s_uiOpeningShots_PSP = uiShotCount;
	if ( iVoice >= 0 )
		APlaySSeqVO_PSP( iVoice );
	AUpdateGameRuntimeMemory_PSP();
	return TTRUE;
}

TBOOL AIsSSeqOpeningPlaying_PSP()
{
	return s_uiOpeningShots_PSP != 0;
}

void AStopSSeqOpening_PSP( TBOOL a_bRestoreFrontend )
{
	AStopSSeqVO_PSP();
	s_uiOpeningShots_PSP = 0;
	for ( AFrontEndCamera_PSP& rCamera : s_aSSeqOpeningCameras_PSP )
		rCamera.Unload();
	s_SSeqSkyDome_PSP.Unload();
	s_SSeqActors_PSP.Unload();
	s_SSeqTerrain_PSP.Unload();
	if ( a_bRestoreFrontend )
		ALoadFrontEndWorld_PSP();
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
