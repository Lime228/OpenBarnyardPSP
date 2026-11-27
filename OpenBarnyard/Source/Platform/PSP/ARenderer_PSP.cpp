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
#include "ASimState_PSP.h"
#include "ASkyDome_PSP.h"
#include "AVOStream_PSP.h"
#include "GUI/AGUI2.h"
#include "GameInterface/AGameStateController.h"
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
static constexpr TCHAR s_szSimTerrainIndex_PSP[] = "local:Data/Terrain/EnvMain/Barn/terrain.ptld";
static constexpr TCHAR s_szSimSkyRoot_PSP[] = "local:Data/Terrain/EnvMain/Sky";
static constexpr TFLOAT s_fFrontEndStartHour_PSP = 6.0f;
static constexpr TCHAR s_szOpeningRoot_PSP[] = "local:Data/ScriptedSequence/SS01";
static constexpr TCHAR s_szOpeningConfig_PSP[] = "local:Data/ScriptedSequence/SS01/opening.psco";
static constexpr TCHAR s_szContinuationRoot_PSP[] = "local:Data/ScriptedSequence/SS01/AfterCharacterSetup";
static constexpr TCHAR s_szContinuationConfig_PSP[] = "local:Data/ScriptedSequence/SS01/AfterCharacterSetup/opening.psco";
static AFrontEndCamera_PSP s_aSSeqOpeningCameras_PSP[ AGameRuntimeReport_PSP::MAX_OPENING_SHOTS ];
static TFLOAT s_afOpeningDurations_PSP[ AGameRuntimeReport_PSP::MAX_OPENING_SHOTS ];
static TFLOAT s_fOpeningTotal_PSP = 0.0f;
static TUINT s_uiOpeningShots_PSP = 0;
static TFLOAT s_fOpeningElapsed_PSP = 0.0f;
static TFLOAT s_fOpeningShotProgress_PSP = 0.0f;
static TUINT32 s_uiSSeqBeadyIntDayShots_PSP = 0;
static TBOOL s_bSSeqBeadyIntDayLoaded_PSP = TFALSE;
static AFrontEndCamera_PSP s_SSeqCharacterCamera_PSP;
static TBOOL s_bSSeqCharacterSetupVisible_PSP = TFALSE;
static constexpr TCHAR s_szSSeqCharacterConfig_PSP[] = "local:Data/ScriptedSequence/SS01/opening-ui.psui";
static constexpr TCHAR s_szSSeqCharacterCamera_PSP[] = "local:Data/ScriptedSequence/SS01/opening-ui.pcam";

struct ASSeqOpeningShot_PSP
{
	TFLOAT fHorizontalFOV;
	TFLOAT fNear;
	TFLOAT fDuration;
};

static const TRenderCamera_PSP& AUpdateSSeqOpeningCamera_PSP( TFLOAT a_fDeltaTime, TUINT& a_ruiShot, TINT& a_riVoiceMS )
{
	if ( s_uiOpeningShots_PSP == 1 )
	{
		a_ruiShot = 1;
		s_aSSeqOpeningCameras_PSP[ 0 ].Update( a_fDeltaTime );
		return s_aSSeqOpeningCameras_PSP[ 0 ].GetRenderCamera();
	}

	if ( s_fOpeningElapsed_PSP < s_fOpeningTotal_PSP )
	{
		const TINT iVoiceMS = AGetSSeqVOTime_PSP();
		if ( iVoiceMS >= 0 )
		{
			a_riVoiceMS = iVoiceMS;
			AGetGameRuntimeReport_PSP().uiNewGameVoiceUnderruns = AGetSSeqVOUnderruns_PSP();
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

	if ( AIsSSeqCharacterSetupReady_PSP() )
	{
		s_fOpeningShotProgress_PSP = 1.0f;
		s_SSeqCharacterCamera_PSP.Update( a_fDeltaTime );
		a_ruiShot = s_uiOpeningShots_PSP;
		return s_SSeqCharacterCamera_PSP.GetRenderCamera();
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
	a_ruiShot = uiShot + 1;
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
	AGameStateController* pController = AGameStateController::GetSingletonSafe();
	AGameState* pSim = pController != TNULL ? pController->FindGameStateOnStack( &TGetClass( ASimState_PSP ) ) : TNULL;
	if ( pSim != TNULL )
	{
		const TBOOL bSimIsCurrent = pController->GetCurrentState() == pSim;
		s_RenderInterface_PSP.BeginScene( 0xff000000 );
		pSim->OnRender( bSimIsCurrent ? a_fDeltaTime : 0.0f );
		if ( m_bRenderGUI && AGUI2::GetContext() != TNULL && AGUI2::GetRenderer() != TNULL )
			RenderGUI();
		s_RenderInterface_PSP.EndScene();
		if ( bSimIsCurrent && rReport.uiSimFrames == 1 && rReport.uiSimTerrainDraws != 0 && rReport.uiSimPlayerDraws != 0 )
			ASetGameRuntimePhase_PSP( "sim-world-ready" );
		else if ( rReport.uiRendererFrames % 60 == 0 )
			AWriteGameRuntimeReport_PSP();
		return TTRUE;
	}
	if ( s_uiOpeningShots_PSP != 0 )
	{
		const TBOOL bContinuation = s_uiSSeqBeadyIntDayShots_PSP != 0;
		TUINT& ruiCurrentShot = bContinuation ? rReport.uiNewGameContinuationShot : rReport.uiNewGameOpeningShot;
		TINT& riVoiceMS = bContinuation ? rReport.iNewGameContinuationVoiceMS : rReport.iNewGameVoiceMS;
		const TUINT uiPreviousShot = ruiCurrentShot;
		const TRenderCamera_PSP& rCamera = AUpdateSSeqOpeningCamera_PSP( a_fDeltaTime, ruiCurrentShot, riVoiceMS );
		const TUINT uiShot = ruiCurrentShot - 1;
		if ( uiPreviousShot != ruiCurrentShot && !s_SSeqActors_PSP.SetShot( uiShot ) )
		{
			ASetGameRuntimeFailedPath_PSP( s_SSeqActors_PSP.GetFailedPath() );
			AStopSSeqOpening_PSP( TTRUE );
			return TTRUE;
		}
		const TBOOL bBeadyIntDay = ( s_uiSSeqBeadyIntDayShots_PSP & ( 1U << uiShot ) ) != 0;
		if ( bBeadyIntDay != s_bSSeqBeadyIntDayLoaded_PSP )
		{
			if ( !s_SSeqTerrain_PSP.LoadSS01( bBeadyIntDay ) )
			{
				ASetGameRuntimeFailedPath_PSP( s_SSeqTerrain_PSP.GetFailedPath() );
				AStopSSeqOpening_PSP( TTRUE );
				return TTRUE;
			}
			s_bSSeqBeadyIntDayLoaded_PSP = bBeadyIntDay;
			rReport.bNewGameBeadyIntDay = bBeadyIntDay;
			rReport.uiNewGameTerrainMeshes = s_SSeqTerrain_PSP.GetMeshCount();
			rReport.uiNewGameTerrainTextures = s_SSeqTerrain_PSP.GetTextureCount();
		}
		s_RenderInterface_PSP.BeginScene( 0xff000000 );
		rReport.uiNewGameSkyDraws += s_SSeqSkyDome_PSP.Render( s_RenderInterface_PSP, rCamera, a_fDeltaTime );
		rReport.uiNewGameOpeningDraws += s_SSeqTerrain_PSP.Render( s_RenderInterface_PSP, rCamera );
		const TUINT uiActorDraws = s_SSeqActors_PSP.Render(
		    s_RenderInterface_PSP, rCamera, uiShot, s_fOpeningShotProgress_PSP, s_afOpeningDurations_PSP[ uiShot ] );
		TUINT& ruiActorDraws = bContinuation ? rReport.auiNewGameContinuationActorDraws[ uiShot ] : rReport.auiNewGameActorDraws[ uiShot ];
		ruiActorDraws = TMath::Max( ruiActorDraws, uiActorDraws );
		if ( s_bSSeqCharacterSetupVisible_PSP && AGUI2::GetContext() != TNULL && AGUI2::GetRenderer() != TNULL )
			RenderGUI();
		s_RenderInterface_PSP.EndScene();
		rReport.uiNewGameOpeningFrames++;
		if ( uiPreviousShot != ruiCurrentShot || rReport.uiRendererFrames % 60 == 0 )
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

static TBOOL AHasSSeqTimeline_PSP( const TCHAR* a_szPath )
{
	TFile* pFile = TFile::Create( a_szPath, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TFALSE;
	pFile->Destroy();
	return TTRUE;
}

TBOOL AHasSSeqOpening_PSP()
{
	return AHasSSeqTimeline_PSP( s_szOpeningConfig_PSP );
}

TBOOL AHasSSeqContinuation_PSP()
{
	return AHasSSeqTimeline_PSP( s_szContinuationConfig_PSP );
}

static TBOOL AReadSSeqOpening_PSP( const TCHAR* a_szConfig, TBOOL a_bContinuation,
                                  ASSeqOpeningShot_PSP* a_pShots, TUINT32& a_ruiShotCount, TINT32& a_riVoice )
{
	static_assert( sizeof( ASSeqOpeningShot_PSP ) == 12 );
	TFile* pFile = TFile::Create( a_szConfig, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TFALSE;
	TUINT32 aHeader[ 2 ];
	TBOOL bRead = pFile->Read( aHeader, sizeof( aHeader ) ) == sizeof( aHeader ) && aHeader[ 0 ] == 0x4f435350;
	if ( bRead && !a_bContinuation && aHeader[ 1 ] == 1 )
	{
		// The previous camera-only package stored FOV and near plane, without a timeline.
		a_ruiShotCount = 1;
		a_riVoice = -1;
		bRead = pFile->GetSize() == 16 && pFile->Read( &a_pShots[ 0 ], 8 ) == 8;
	}
	else if ( bRead && aHeader[ 1 ] == ( a_bContinuation ? 3U : 2U ) )
	{
		bRead = pFile->Read( &a_ruiShotCount, sizeof( a_ruiShotCount ) ) == sizeof( a_ruiShotCount ) &&
		        pFile->Read( &a_riVoice, sizeof( a_riVoice ) ) == sizeof( a_riVoice ) && a_riVoice >= 0 &&
		        a_ruiShotCount >= 2 && a_ruiShotCount <= TARRAYSIZE( s_aSSeqOpeningCameras_PSP );
		if ( bRead )
		{
			const TSIZE uiBytes = a_ruiShotCount * sizeof( ASSeqOpeningShot_PSP );
			const TSIZE uiMaskBytes = a_bContinuation ? sizeof( s_uiSSeqBeadyIntDayShots_PSP ) : 0;
			bRead = pFile->GetSize() == 16 + uiBytes + uiMaskBytes && pFile->Read( a_pShots, uiBytes ) == uiBytes;
			if ( bRead && a_bContinuation )
			{
				bRead = pFile->Read( &s_uiSSeqBeadyIntDayShots_PSP, uiMaskBytes ) == uiMaskBytes &&
				        s_uiSSeqBeadyIntDayShots_PSP != 0 && ( s_uiSSeqBeadyIntDayShots_PSP & 1U ) == 0 &&
				        s_uiSSeqBeadyIntDayShots_PSP < ( 1U << a_ruiShotCount );
			}
		}
	}
	else
		bRead = TFALSE;
	pFile->Destroy();
	return bRead;
}

static TBOOL ALoadSSeqCharacterCamera_PSP( TUINT a_uiShotCount, TFLOAT a_fAspect )
{
	s_SSeqCharacterCamera_PSP.Unload();
	TFile* pFile = TFile::Create( s_szSSeqCharacterConfig_PSP, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TTRUE;

	struct Config
	{
		TUINT32 uiMagic;
		TUINT32 uiVersion;
		TFLOAT fFOV;
		TFLOAT fNear;
	};
	static_assert( sizeof( Config ) == 16 );
	Config oConfig = {};
	const TBOOL bRead = pFile->GetSize() == sizeof( oConfig ) && pFile->Read( &oConfig, sizeof( oConfig ) ) == sizeof( oConfig );
	pFile->Destroy();
	if ( !bRead || oConfig.uiMagic != 0x49555350 || oConfig.uiVersion != 1 ||
	     a_uiShotCount != AGameRuntimeReport_PSP::MAX_OPENING_SHOTS ||
	     !( oConfig.fFOV >= 1.0f && oConfig.fFOV < 170.0f && oConfig.fNear > 0.0f && oConfig.fNear < 280.0f ) )
	{
		ASetGameRuntimeFailedPath_PSP( s_szSSeqCharacterConfig_PSP );
		return TFALSE;
	}
	const TFLOAT fHalfFOV = TMath::DegToRad( oConfig.fFOV * 0.5f );
	const TFLOAT fVerticalFOV = TMath::ATan( TMath::Tan( fHalfFOV ) / a_fAspect ) * ( 360.0f / TMath::PI );
	if ( !s_SSeqCharacterCamera_PSP.Load( s_szSSeqCharacterCamera_PSP, fVerticalFOV, oConfig.fNear ) )
	{
		ASetGameRuntimeFailedPath_PSP( s_szSSeqCharacterCamera_PSP );
		return TFALSE;
	}
	return TTRUE;
}

static TBOOL ALoadSSeqTimeline_PSP( TBOOL a_bContinuation, TUINT32& a_ruiShotCount, TINT32& a_riVoice )
{
	const TCHAR* szRoot = a_bContinuation ? s_szContinuationRoot_PSP : s_szOpeningRoot_PSP;
	const TCHAR* szConfig = a_bContinuation ? s_szContinuationConfig_PSP : s_szOpeningConfig_PSP;
	ASSeqOpeningShot_PSP aShots[ TARRAYSIZE( s_aSSeqOpeningCameras_PSP ) ] = {};
	s_uiSSeqBeadyIntDayShots_PSP = 0;
	if ( !AReadSSeqOpening_PSP( szConfig, a_bContinuation, aShots, a_ruiShotCount, a_riVoice ) )
	{
		ASetGameRuntimeFailedPath_PSP( szConfig );
		return TFALSE;
	}
	const TFLOAT fAspect = TFLOAT( TRenderInterface_PSP::SCREEN_WIDTH ) / TRenderInterface_PSP::SCREEN_HEIGHT;
	s_fOpeningTotal_PSP = 0.0f;
	for ( TUINT i = 0; i < a_ruiShotCount; i++ )
	{
		const ASSeqOpeningShot_PSP& rShot = aShots[ i ];
		if ( !( rShot.fHorizontalFOV >= 1.0f && rShot.fHorizontalFOV < 170.0f ) ||
		     !( rShot.fNear > 0.0f && rShot.fNear < 280.0f ) ||
		     ( a_ruiShotCount > 1 && !( rShot.fDuration > 0.0f && rShot.fDuration <= 60.0f ) ) )
		{
			ASetGameRuntimeFailedPath_PSP( szConfig );
			return TFALSE;
		}
		const TFLOAT fHalfFOV = TMath::DegToRad( rShot.fHorizontalFOV * 0.5f );
		const TFLOAT fVerticalFOV = TMath::ATan( TMath::Tan( fHalfFOV ) / fAspect ) * ( 360.0f / TMath::PI );
		TCHAR szCameraPath[ 88 ];
		if ( i == 0 )
			TStringManager::String8Format( szCameraPath, sizeof( szCameraPath ), "%s/opening.pcam", szRoot );
		else
			TStringManager::String8Format( szCameraPath, sizeof( szCameraPath ), "%s/opening-%u.pcam", szRoot, i );
		if ( !s_aSSeqOpeningCameras_PSP[ i ].Load( szCameraPath, fVerticalFOV, rShot.fNear ) )
		{
			ASetGameRuntimeFailedPath_PSP( szCameraPath );
			return TFALSE;
		}
		s_afOpeningDurations_PSP[ i ] = rShot.fDuration;
		s_fOpeningTotal_PSP += rShot.fDuration;
	}

	return a_bContinuation || ALoadSSeqCharacterCamera_PSP( a_ruiShotCount, fAspect );
}

static void APlaySSeqTimeline_PSP( TUINT a_uiShotCount )
{
	s_fOpeningElapsed_PSP = 0.0f;
	s_fOpeningShotProgress_PSP = 0.0f;
	s_uiOpeningShots_PSP = a_uiShotCount;
	AUpdateGameRuntimeMemory_PSP();
}

TBOOL AStartSSeqOpening_PSP()
{
	TUINT32 uiShotCount = 0;
	TINT32 iVoice = -1;
	if ( !ALoadSSeqTimeline_PSP( TFALSE, uiShotCount, iVoice ) )
	{
		AStopSSeqOpening_PSP( TFALSE );
		return TFALSE;
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
	rReport.uiNewGameContinuationShot = 0;
	rReport.iNewGameContinuationVoiceMS = -1;
	rReport.bNewGameBeadyIntDay = TFALSE;
	rReport.bNewGameContinuationComplete = TFALSE;
	rReport.bNewGameSimStartReady = TFALSE;
	rReport.uiNewGameSimCollisionPacks = 0;
	rReport.uiNewGameSimCollisionTriangles = 0;
	rReport.oNewGameSimStart = {};
	rReport.uiSimFrames = 0;
	rReport.uiSimTerrainMeshes = 0;
	rReport.uiSimTerrainTextures = 0;
	rReport.uiSimTerrainDraws = 0;
	rReport.uiSimSkyDraws = 0;
	rReport.uiSimPlayerDraws = 0;
	for ( TUINT& ruiDraws : rReport.auiNewGameActorDraws )
		ruiDraws = 0;
	for ( TUINT& ruiDraws : rReport.auiNewGameContinuationActorDraws )
		ruiDraws = 0;
	rReport.iNewGameVoiceMS = -1;
	rReport.uiNewGameVoiceUnderruns = 0;
	rReport.uiNewGameVoiceNonSilentBlocks = 0;
	if ( iVoice >= 0 )
		APlaySSeqVO_PSP( iVoice );
	APlaySSeqTimeline_PSP( uiShotCount );
	return TTRUE;
}

TBOOL AStartSSeqContinuation_PSP( TUINT a_uiGender, TUINT a_uiCostume )
{
	if ( !AIsSSeqCharacterSetupReady_PSP() || a_uiGender > 1 || a_uiCostume >= 7 )
	{
		ASetGameRuntimeFailedPath_PSP( s_szContinuationConfig_PSP );
		return TFALSE;
	}

	AStopSSeqVO_PSP();
	s_SSeqCharacterCamera_PSP.Unload();
	for ( AFrontEndCamera_PSP& rCamera : s_aSSeqOpeningCameras_PSP )
		rCamera.Unload();
	s_SSeqActors_PSP.Unload();

	TUINT32 uiShotCount = 0;
	TINT32 iVoice = -1;
	if ( !ALoadSSeqTimeline_PSP( TTRUE, uiShotCount, iVoice ) )
		return TFALSE;

	static const TCHAR* const s_aszActors[] = {
		"local:Data/ScriptedSequence/SS01/AfterCharacterSetup/Male/Actors",
		"local:Data/ScriptedSequence/SS01/AfterCharacterSetup/Female/Actors",
	};
	TCHAR szCostume[ 80 ];
	TStringManager::String8Format( szCostume, TARRAYSIZE( szCostume ), "local:Data/ScriptedSequence/SS01/Costumes/ss_player%s_quad-%u.ptex",
	                              a_uiGender == 0 ? "" : "f", a_uiCostume );
	if ( !s_SSeqActors_PSP.Load( uiShotCount, s_aszActors[ a_uiGender ], szCostume ) )
	{
		ASetGameRuntimeFailedPath_PSP( s_SSeqActors_PSP.GetFailedPath() );
		return TFALSE;
	}
	if ( !APlaySSeqVO_PSP( iVoice ) )
	{
		TCHAR szVoicePath[ 72 ];
		TStringManager::String8Format( szVoicePath, TARRAYSIZE( szVoicePath ), "local:Data/ScriptedSequence/SS01/Voice/%d.padp", iVoice );
		ASetGameRuntimeFailedPath_PSP( szVoicePath );
		return TFALSE;
	}
	APlaySSeqTimeline_PSP( uiShotCount );
	return TTRUE;
}

TBOOL AIsSSeqContinuationComplete_PSP()
{
	return s_uiSSeqBeadyIntDayShots_PSP != 0 && s_fOpeningElapsed_PSP >= s_fOpeningTotal_PSP;
}

TBOOL AIsSSeqOpeningPlaying_PSP()
{
	return s_uiOpeningShots_PSP != 0;
}

TBOOL AIsSSeqCharacterSetupReady_PSP()
{
	return s_SSeqCharacterCamera_PSP.IsLoaded() && s_fOpeningElapsed_PSP >= s_fOpeningTotal_PSP;
}

void ASetSSeqCharacterSetupVisible_PSP( TBOOL a_bVisible )
{
	s_bSSeqCharacterSetupVisible_PSP = a_bVisible;
}

void AStopSSeqOpening_PSP( TBOOL a_bRestoreFrontend )
{
	AStopSSeqVO_PSP();
	s_uiOpeningShots_PSP = 0;
	s_uiSSeqBeadyIntDayShots_PSP = 0;
	s_bSSeqBeadyIntDayLoaded_PSP = TFALSE;
	s_bSSeqCharacterSetupVisible_PSP = TFALSE;
	s_SSeqCharacterCamera_PSP.Unload();
	for ( AFrontEndCamera_PSP& rCamera : s_aSSeqOpeningCameras_PSP )
		rCamera.Unload();
	s_SSeqSkyDome_PSP.Unload();
	s_SSeqActors_PSP.Unload();
	s_SSeqTerrain_PSP.Unload();
	if ( a_bRestoreFrontend )
		ALoadFrontEndWorld_PSP();
}

TBOOL AHasSimWorld_PSP()
{
	return AHasSSeqTimeline_PSP( s_szSimTerrainIndex_PSP );
}

TBOOL ALoadSimWorld_PSP()
{
	if ( !ALoadSimTerrainRow_PSP( "Barn" ) )
		return TFALSE;
	if ( !s_SSeqSkyDome_PSP.Load( s_szSimSkyRoot_PSP ) )
	{
		ASetGameRuntimeFailedPath_PSP( s_SSeqSkyDome_PSP.GetFailedPath() );
		AUnloadSimWorld_PSP();
		return TFALSE;
	}
	AUpdateGameRuntimeMemory_PSP();
	return TTRUE;
}

TBOOL ALoadSimTerrainRow_PSP( const TCHAR* a_szName )
{
	TCHAR szRoot[ 56 ];
	TStringManager::String8Format( szRoot, sizeof( szRoot ), "local:Data/Terrain/EnvMain/%s", a_szName );
	if ( !s_SSeqTerrain_PSP.Load( szRoot ) )
	{
		ASetGameRuntimeFailedPath_PSP( s_SSeqTerrain_PSP.GetFailedPath() );
		return TFALSE;
	}
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.uiSimTerrainMeshes = s_SSeqTerrain_PSP.GetMeshCount();
	rReport.uiSimTerrainTextures = s_SSeqTerrain_PSP.GetTextureCount();
	AUpdateGameRuntimeMemory_PSP();
	return TTRUE;
}

void AUnloadSimWorld_PSP()
{
	s_SSeqSkyDome_PSP.Unload();
	s_SSeqTerrain_PSP.Unload();
}

void ARenderSimWorld_PSP( const TRenderCamera_PSP& a_rCamera, TFLOAT a_fDeltaTime )
{
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.uiSimSkyDraws = s_SSeqSkyDome_PSP.Render( s_RenderInterface_PSP, a_rCamera, a_fDeltaTime );
	rReport.uiSimTerrainDraws = s_SSeqTerrain_PSP.Render( s_RenderInterface_PSP, a_rCamera );
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
