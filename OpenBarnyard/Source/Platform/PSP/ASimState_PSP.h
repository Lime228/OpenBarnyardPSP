#pragma once

#include "GameInterface/AGameState.h"
#include "ASimPlayer_PSP.h"
#include "ATerrainCollision_PSP.h"

class ASimState_PSP : public AGameState
{
	TDECLARE_CLASS( ASimState_PSP, AGameState );

public:
	TBOOL Create( TUINT a_uiGender, TUINT a_uiCostume );
	void ReturnToFrontend();
	void OnActivate() override;
	void OnRemoval() override;
	TBOOL OnUpdate( TFLOAT a_fDeltaTime ) override;
	void OnRender( TFLOAT a_fDeltaTime ) override;
	TBOOL ProcessCommand( AInputCommand a_eCommand, const Toshi::TInputInterface::InputEvent* a_pEvent, TBOOL& a_rStopEvents ) override;

private:
	static constexpr TFLOAT CAMERA_DISTANCE = 5.0f;

	struct TerrainRow
	{
		TUINT32 uiSection;
		TUINT32 uiCollisionMask;
		TCHAR szName[ 24 ];
	};

	TBOOL LoadTerrainRows();
	TBOOL UpdateTerrainRow( const Toshi::TVector4& a_rCameraPosition );
	void UpdateCamera( const Toshi::TVector4& a_rPlayerPosition, TFLOAT a_fDeltaTime, TBOOL a_bPlayerMoved );

	ASimPlayer_PSP m_Player;
	ATerrainCollision_PSP m_TerrainCollision;
	Toshi::TRenderCamera_PSP m_Camera;
	Toshi::TVector3 m_vCameraOrbitPosition;
	TFLOAT m_fCameraDistance = CAMERA_DISTANCE;
	TFLOAT m_fCameraTargetDistance = CAMERA_DISTANCE;
	Toshi::TVector2 m_vStick = Toshi::TVector2::VEC_ZERO;
	TerrainRow m_aTerrainRows[ ATerrainCollision_PSP::MAX_CATALOG_CELLS ] = {};
	TUINT m_uiTerrainRowCount = 0;
	TUINT m_uiTerrainRow = 0;
	TINT m_iRejectedTerrainSection = -1;
};
