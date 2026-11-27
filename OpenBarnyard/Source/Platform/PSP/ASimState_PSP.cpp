#include "pch.h"
#include "ASimState_PSP.h"
#include "AGameRuntimeReport_PSP.h"
#include "ARenderer_PSP.h"
#include "GameInterface/AFrontEndMainMenuState2.h"
#include "GameInterface/AGameStateController.h"

#include <File/TFile.h>
#include <Math/TMathInline.h>
#include <Toshi/TStringManager.h>

TOSHI_NAMESPACE_USING

TDEFINE_CLASS( ASimState_PSP );

static constexpr TCHAR s_szSimTerrainRows_PSP[] = "local:Data/Terrain/EnvMain/rows.psvs";

static TBOOL AIsSimTerrainRowName_PSP( const TCHAR* a_szName, TSIZE a_uiCapacity )
{
	for ( TSIZE i = 0; i < a_uiCapacity; i++ )
	{
		const TCHAR c = a_szName[ i ];
		if ( c == '\0' )
			return i != 0;
		if ( !( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_' ) )
			return TFALSE;
	}
	return TFALSE;
}

TBOOL ASimState_PSP::LoadTerrainRows()
{
	TFile* pFile = TFile::Create( s_szSimTerrainRows_PSP, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TTRUE;

	static_assert( sizeof( TerrainRow ) == 32 );
	TUINT32 aHeader[ 3 ];
	TBOOL bRead = pFile->Read( aHeader, sizeof( aHeader ) ) == sizeof( aHeader ) &&
	              aHeader[ 0 ] == 0x53565350 && aHeader[ 1 ] == 1 &&
	              aHeader[ 2 ] > 0 && aHeader[ 2 ] <= TARRAYSIZE( m_aTerrainRows );
	if ( bRead )
	{
		const TSIZE uiBytes = aHeader[ 2 ] * sizeof( TerrainRow );
		bRead = pFile->GetSize() == sizeof( aHeader ) + uiBytes &&
		        pFile->Read( m_aTerrainRows, uiBytes ) == uiBytes;
	}
	pFile->Destroy();
	if ( !bRead )
		return TFALSE;

	const TUINT uiCatalogMask = m_TerrainCollision.GetCatalogMask();
	TUINT uiSeen = 0;
	for ( TUINT i = 0; i < aHeader[ 2 ]; i++ )
	{
		const TerrainRow& rRow = m_aTerrainRows[ i ];
		if ( rRow.uiSection >= ATerrainCollision_PSP::MAX_CATALOG_CELLS || ( rRow.uiCollisionMask & ~uiCatalogMask ) != 0 ||
		     !AIsSimTerrainRowName_PSP( rRow.szName, sizeof( rRow.szName ) ) )
			return TFALSE;
		const TUINT uiBit = 1U << rRow.uiSection;
		if ( ( rRow.uiCollisionMask & uiBit ) == 0 || ( uiSeen & uiBit ) != 0 )
			return TFALSE;
		uiSeen |= uiBit;
	}
	if ( TStringManager::String8Compare( m_aTerrainRows[ 0 ].szName, "Barn" ) != 0 ||
	     m_aTerrainRows[ 0 ].uiCollisionMask != m_TerrainCollision.GetActivePackMask() )
		return TFALSE;
	m_uiTerrainRowCount = aHeader[ 2 ];
	return TTRUE;
}

TBOOL ASimState_PSP::UpdateTerrainRow( const TVector4& a_rCameraPosition )
{
	if ( m_uiTerrainRowCount < 2 )
		return TFALSE;
	const TerrainRow& rCurrent = m_aTerrainRows[ m_uiTerrainRow ];
	const TINT iSection = m_TerrainCollision.FindSection( a_rCameraPosition, rCurrent.uiSection );
	if ( iSection < 0 || iSection == TINT( rCurrent.uiSection ) )
	{
		m_iRejectedTerrainSection = -1;
		return TFALSE;
	}
	if ( iSection == m_iRejectedTerrainSection )
		return TFALSE;
	for ( TUINT i = 0; i < m_uiTerrainRowCount; i++ )
	{
		const TerrainRow& rNext = m_aTerrainRows[ i ];
		if ( TINT( rNext.uiSection ) != iSection )
			continue;
		AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
		const TBOOL bCollisionReady = m_TerrainCollision.ActivatePacks( rNext.uiCollisionMask );
		// Each row uses the same render heap. Release the previous row before loading the next.
		if ( !bCollisionReady || !ALoadSimTerrainRow_PSP( rNext.szName ) )
		{
			m_iRejectedTerrainSection = iSection;
			rReport.uiSimTerrainSwapFailures++;
			const TBOOL bRestored = bCollisionReady ?
			    m_TerrainCollision.ActivatePacks( rCurrent.uiCollisionMask ) && ALoadSimTerrainRow_PSP( rCurrent.szName ) :
			    m_TerrainCollision.GetActivePackMask() == rCurrent.uiCollisionMask;
			if ( bRestored )
				ASetGameRuntimeFailedPath_PSP( "" );
			else
			{
				m_uiTerrainRowCount = 0;
				if ( m_TerrainCollision.GetActivePackMask() != rCurrent.uiCollisionMask )
					ASetGameRuntimeFailedPath_PSP( m_TerrainCollision.GetFailedPath() );
				ASetGameRuntimePhase_PSP( "sim-world-failed" );
			}
			return TFALSE;
		}
		m_uiTerrainRow = i;
		m_iRejectedTerrainSection = -1;
		rReport.uiSimTerrainCollisionMask = rNext.uiCollisionMask;
		rReport.uiSimTerrainSwaps++;
		TStringManager::String8Copy( rReport.szSimTerrainRow, rNext.szName, sizeof( rReport.szSimTerrainRow ) );
		return TTRUE;
	}
	return TFALSE;
}

void ASimState_PSP::UpdateCamera( const TVector4& a_rPlayerPosition, TFLOAT a_fDeltaTime, TBOOL a_bPlayerMoved )
{
	if ( !a_bPlayerMoved && m_fCameraDistance == m_fCameraTargetDistance )
		return;
	const TVector4 vPivot( a_rPlayerPosition.x, a_rPlayerPosition.y - 1.7f, a_rPlayerPosition.z, 1.0f );
	// AGTAStyleCameraHelper keeps the default orbit separate from the camera that is moved in.
	TVector4 vBackward( m_vCameraOrbitPosition.x - vPivot.x, 0.0f, m_vCameraOrbitPosition.z - vPivot.z, 0.0f );
	vBackward.NormaliseXZ();
	vBackward.y = -TMath::ATan( TMath::DegToRad( 11.0f ) );
	vBackward.Normalise();
	if ( a_bPlayerMoved )
	{
		m_vCameraOrbitPosition.Set( vPivot.x + vBackward.x * CAMERA_DISTANCE, vPivot.y + vBackward.y * CAMERA_DISTANCE,
		                           vPivot.z + vBackward.z * CAMERA_DISTANCE );
		m_fCameraTargetDistance = m_TerrainCollision.GetCameraDistance( vPivot, vBackward, CAMERA_DISTANCE );
	}
	// Clamp to a new obstruction immediately and move out at the retail maximum recovery speed.
	m_fCameraDistance = TMath::Min( m_fCameraTargetDistance, m_fCameraDistance + TMath::Max( a_fDeltaTime, 0.0f ) * 3.0f );
	// TRenderCamera_PSP stores the inverse of visual up, like the converted retail cameras.
	TVector3 vUp( -vBackward.x * vBackward.y, 1.0f - vBackward.y * vBackward.y, -vBackward.z * vBackward.y );
	vUp.Normalize();
	m_Camera = {
		{ vPivot.x + vBackward.x * m_fCameraDistance, vPivot.y + vBackward.y * m_fCameraDistance, vPivot.z + vBackward.z * m_fCameraDistance },
		{ -vBackward.x, -vBackward.y, -vBackward.z },
		{ vUp.x, vUp.y, vUp.z },
		m_Camera.fFOV,
		TMath::Min( 1.0f, TMath::Max( 0.05f, 0.25f * m_fCameraDistance ) ),
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
	if ( !LoadTerrainRows() )
	{
		ASetGameRuntimeFailedPath_PSP( s_szSimTerrainRows_PSP );
		return TFALSE;
	}
	const ASimStart_PSP& rStart = rReport.oNewGameSimStart;
	const TFLOAT fAspect = TFLOAT( TRenderInterface_PSP::SCREEN_WIDTH ) / TRenderInterface_PSP::SCREEN_HEIGHT;
	m_Camera.fFOV = TMath::ATan( TMath::Tan( TMath::DegToRad( 30.0f ) ) / fAspect ) * ( 360.0f / TMath::PI );
	m_vCameraOrbitPosition.Set( rStart.vPosition.x - rStart.vFacing.x * CAMERA_DISTANCE, rStart.vPosition.y,
	                           rStart.vPosition.z - rStart.vFacing.z * CAMERA_DISTANCE );
	UpdateCamera( rStart.vPosition, 0.0f, TTRUE );
	if ( !m_Player.Load( rReport.oNewGameSimStart, a_uiGender, a_uiCostume ) )
	{
		ASetGameRuntimeFailedPath_PSP( m_Player.GetFailedPath() );
		return TFALSE;
	}
	if ( !m_Miles.Load() )
	{
		ASetGameRuntimeFailedPath_PSP( m_Miles.GetFailedPath() );
		m_Player.Unload();
		return TFALSE;
	}
	if ( !ALoadSimWorld_PSP() )
	{
		m_Miles.Unload();
		m_Player.Unload();
		return TFALSE;
	}
	rReport.bSimPlayerSteering = m_Player.HasSteering();
	rReport.bSimMilesLoaded = m_Miles.IsLoaded();
	rReport.vSimMilesPosition = m_Miles.GetPosition();
	rReport.uiSimMilesDraws = rReport.uiSimMilesPoseUpdates = 0;
	rReport.uiSimFrames = 0;
	rReport.uiSimUpdates = 0;
	rReport.vSimPlayerPosition = m_Player.GetPosition();
	rReport.fSimPlayerSpeed = 0.0f;
	rReport.uiSimPlayerMotion = 0;
	rReport.uiSimPlayerMotionMask = 0;
	rReport.uiSimTerrainContacts = 0;
	rReport.uiSimTerrainSlides = 0;
	rReport.uiSimTerrainCollisionMask = m_TerrainCollision.GetActivePackMask();
	rReport.uiSimCollisionResidentPacks = m_TerrainCollision.GetPackCount();
	rReport.uiSimCollisionResidentBytes = TUINT( m_TerrainCollision.GetResidentBytes() );
	rReport.uiSimCollisionPeakBytes = rReport.uiSimCollisionResidentBytes;
	rReport.uiSimTerrainSwaps = rReport.uiSimTerrainSwapFailures = 0;
	TStringManager::String8Copy( rReport.szSimTerrainRow, "Barn", sizeof( rReport.szSimTerrainRow ) );
	rReport.fSimCameraDistance = m_fCameraDistance;
	rReport.fSimCameraMinDistance = m_fCameraDistance;
	return TTRUE;
}

void ASimState_PSP::OnActivate()
{
	BaseClass::OnActivate();
	SetInputMap( TPString8( "NormalBarnyard" ) );
	ASetGameRuntimePhase_PSP( AGetGameRuntimeReport_PSP().uiSimFrames == 0 ? "sim-world-loading" : "sim-world-ready" );
}

void ASimState_PSP::OnRemoval()
{
	m_Miles.Unload();
	m_Player.Unload();
	m_TerrainCollision.Unload();
	AUnloadSimWorld_PSP();
	BaseClass::OnRemoval();
}

TBOOL ASimState_PSP::OnUpdate( TFLOAT a_fDeltaTime )
{
	const TVector2 vStick = m_vStick;
	m_vStick = TVector2::VEC_ZERO;
	const TVector4 vPrevious = m_Player.GetPosition();
	if ( !m_Player.Update( a_fDeltaTime, vStick, m_Camera, m_TerrainCollision ) )
	{
		ASetGameRuntimeFailedPath_PSP( "envmain-player-pose" );
		ASetGameRuntimePhase_PSP( "sim-world-failed" );
	}
	const TVector4& rPosition = m_Player.GetPosition();
	if ( !m_Miles.Update( a_fDeltaTime, rPosition ) )
	{
		ASetGameRuntimeFailedPath_PSP( "envmain-miles-pose" );
		ASetGameRuntimePhase_PSP( "sim-world-failed" );
	}
	const TBOOL bPlayerMoved = rPosition.x != vPrevious.x || rPosition.y != vPrevious.y || rPosition.z != vPrevious.z;
	const TFLOAT fPreviousCameraDistance = m_fCameraDistance;
	UpdateCamera( rPosition, a_fDeltaTime, bPlayerMoved );
	if ( bPlayerMoved || m_fCameraDistance != fPreviousCameraDistance )
	{
		const TVector4 vCamera( m_Camera.aPosition[ 0 ], m_Camera.aPosition[ 1 ], m_Camera.aPosition[ 2 ], 1.0f );
		if ( UpdateTerrainRow( vCamera ) )
			UpdateCamera( rPosition, 0.0f, TTRUE );
	}
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.uiSimUpdates++;
	rReport.uiSimMilesPoseUpdates = m_Miles.GetPoseUpdates();
	rReport.vSimPlayerPosition = rPosition;
	rReport.vSimCameraPosition.Set( m_Camera.aPosition[ 0 ], m_Camera.aPosition[ 1 ], m_Camera.aPosition[ 2 ], 1.0f );
	rReport.fSimPlayerSpeed = m_Player.GetMeasuredSpeed();
	rReport.uiSimPlayerMotion = m_Player.GetMotion();
	rReport.uiSimPlayerMotionMask |= 1U << rReport.uiSimPlayerMotion;
	rReport.uiSimTerrainContacts = m_Player.GetTerrainContactCount();
	rReport.uiSimTerrainSlides = m_Player.GetTerrainSlideCount();
	rReport.uiSimCollisionResidentPacks = m_TerrainCollision.GetPackCount();
	rReport.uiSimCollisionResidentBytes = TUINT( m_TerrainCollision.GetResidentBytes() );
	rReport.uiSimCollisionPeakBytes = TMath::Max( rReport.uiSimCollisionPeakBytes, rReport.uiSimCollisionResidentBytes );
	rReport.fSimCameraDistance = m_fCameraDistance;
	rReport.fSimCameraMinDistance = TMath::Min( rReport.fSimCameraMinDistance, m_fCameraDistance );
	return BaseClass::OnUpdate( a_fDeltaTime );
}

void ASimState_PSP::OnRender( TFLOAT a_fDeltaTime )
{
	TRenderInterface_PSP& rRenderer = AGetRenderInterface_PSP();
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	ARenderSimWorld_PSP( m_Camera, a_fDeltaTime );
	rReport.uiSimPlayerDraws = m_Player.Render( rRenderer, m_Camera );
	rReport.uiSimMilesDraws = m_Miles.Render( rRenderer, m_Camera );
	rReport.uiSimFrames++;
}

TBOOL ASimState_PSP::ProcessCommand( AInputCommand a_eCommand, const TInputInterface::InputEvent* a_pEvent, TBOOL& a_rStopEvents )
{
	if ( a_eCommand == AInputCommand_Move )
	{
		m_vStick.x += a_pEvent->GetMagnitudeFloat( 0 );
		m_vStick.y -= a_pEvent->GetMagnitudeFloat( 1 );
		a_rStopEvents = TTRUE;
		return TTRUE;
	}
	if ( a_eCommand != AInputCommand_Start && a_eCommand != AInputCommand_Quit && a_eCommand != AInputCommand_Cancel )
		return BaseClass::ProcessCommand( a_eCommand, a_pEvent, a_rStopEvents );
	if ( a_pEvent->GetEventType() != TInputInterface::EVENT_TYPE_GONE_DOWN )
		return TTRUE;

	a_rStopEvents = TTRUE;
	m_vStick = TVector2::VEC_ZERO;
	AGameStateController::GetSingleton()->StartMenuIfPossible();
	return TTRUE;
}

void ASimState_PSP::ReturnToFrontend()
{
	AGameState* pMenu = AGameStateController::GetSingleton()->FindGameStateOnStack( TPString8( "AFrontEndMainMenuState2" ) );
	Remove();
	ALoadFrontEndWorld_PSP();
	if ( pMenu != TNULL )
		TSTATICCAST( AFrontEndMainMenuState2, pMenu )->PauseBackgroundMusic_PSP( TFALSE );
	ASetGameRuntimePhase_PSP( "frontend-new-game-return" );
}
