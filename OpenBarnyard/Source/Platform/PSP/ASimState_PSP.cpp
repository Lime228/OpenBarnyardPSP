#include "pch.h"
#include "ASimState_PSP.h"
#include "AGameRuntimeReport_PSP.h"
#include "ARenderer_PSP.h"
#include "GameInterface/AFrontEndMainMenuState2.h"
#include "GameInterface/AGameStateController.h"

#include <Math/TMathInline.h>

TOSHI_NAMESPACE_USING

TDEFINE_CLASS( ASimState_PSP );

static TRenderCamera_PSP ACreateSimStartCamera_PSP( const ASimStart_PSP& a_rStart )
{
	// AGTAStyleCameraHelper's default orbit and ResetPosition's direction behind the unit.
	TVector3 vBackward( -a_rStart.vFacing.x, 0.0f, -a_rStart.vFacing.z );
	vBackward.Normalize();
	vBackward.y = -TMath::ATan( TMath::DegToRad( 11.0f ) );
	vBackward.Normalize();
	const TVector4& rSpawn = a_rStart.vPosition;
	const TFLOAT fAspect = TFLOAT( TRenderInterface_PSP::SCREEN_WIDTH ) / TRenderInterface_PSP::SCREEN_HEIGHT;
	const TFLOAT fFOV = TMath::ATan( TMath::Tan( TMath::DegToRad( 30.0f ) ) / fAspect ) * ( 360.0f / TMath::PI );
	return {
		{ rSpawn.x + vBackward.x * 5.0f, rSpawn.y - 1.7f + vBackward.y * 5.0f, rSpawn.z + vBackward.z * 5.0f },
		{ -vBackward.x, -vBackward.y, -vBackward.z },
		{ 0.0f, -1.0f, 0.0f },
		fFOV,
	};
}

TBOOL ASimState_PSP::Create( TUINT a_uiGender, TUINT a_uiCostume )
{
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.bNewGameSimStartReady = AReadSimStart_PSP( rReport.oNewGameSimStart, m_TerrainCollision );
	rReport.uiNewGameSimCollisionPacks = m_TerrainCollision.GetPackCount();
	rReport.uiNewGameSimCollisionTriangles = m_TerrainCollision.GetTriangleCount();
	if ( !rReport.bNewGameSimStartReady )
		return TFALSE;
	m_Camera = ACreateSimStartCamera_PSP( rReport.oNewGameSimStart );
	if ( !m_Player.Load( rReport.oNewGameSimStart, a_uiGender, a_uiCostume ) )
	{
		ASetGameRuntimeFailedPath_PSP( m_Player.GetFailedPath() );
		return TFALSE;
	}
	if ( !ALoadSimWorld_PSP() )
	{
		m_Player.Unload();
		return TFALSE;
	}
	return TTRUE;
}

void ASimState_PSP::OnActivate()
{
	BaseClass::OnActivate();
	SetInputMap( TPString8( "NormalBarnyard" ) );
	ASetGameRuntimePhase_PSP( "sim-world-loading" );
}

void ASimState_PSP::OnRemoval()
{
	m_Player.Unload();
	m_TerrainCollision.Unload();
	AUnloadSimWorld_PSP();
	BaseClass::OnRemoval();
}

TBOOL ASimState_PSP::OnUpdate( TFLOAT a_fDeltaTime )
{
	if ( !m_Player.Update( a_fDeltaTime ) )
	{
		ASetGameRuntimeFailedPath_PSP( "envmain-player-idle" );
		ASetGameRuntimePhase_PSP( "sim-world-failed" );
	}
	return BaseClass::OnUpdate( a_fDeltaTime );
}

void ASimState_PSP::OnRender( TFLOAT a_fDeltaTime )
{
	TRenderInterface_PSP& rRenderer = AGetRenderInterface_PSP();
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	ARenderSimWorld_PSP( m_Camera, a_fDeltaTime );
	rReport.uiSimPlayerDraws = m_Player.Render( rRenderer, m_Camera );
	rReport.uiSimFrames++;
}

TBOOL ASimState_PSP::ProcessCommand( AInputCommand a_eCommand, const TInputInterface::InputEvent* a_pEvent, TBOOL& a_rStopEvents )
{
	if ( a_eCommand != AInputCommand_Quit && a_eCommand != AInputCommand_Cancel )
		return BaseClass::ProcessCommand( a_eCommand, a_pEvent, a_rStopEvents );
	if ( a_pEvent->GetEventType() != TInputInterface::EVENT_TYPE_GONE_DOWN )
		return TTRUE;

	a_rStopEvents = TTRUE;
	AGameState* pMenu = AGameStateController::GetSingleton()->FindGameStateOnStack( TPString8( "AFrontEndMainMenuState2" ) );
	Remove();
	ALoadFrontEndWorld_PSP();
	if ( pMenu != TNULL )
		TSTATICCAST( AFrontEndMainMenuState2, pMenu )->PauseBackgroundMusic_PSP( TFALSE );
	ASetGameRuntimePhase_PSP( "frontend-new-game-return" );
	return TTRUE;
}
