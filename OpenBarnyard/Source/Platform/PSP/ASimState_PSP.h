#pragma once

#include "GameInterface/AGameState.h"
#include "ASimPlayer_PSP.h"
#include "ATerrainCollision_PSP.h"

class ASimState_PSP : public AGameState
{
	TDECLARE_CLASS( ASimState_PSP, AGameState );

public:
	TBOOL Create( TUINT a_uiGender, TUINT a_uiCostume );
	void OnActivate() override;
	void OnRemoval() override;
	TBOOL OnUpdate( TFLOAT a_fDeltaTime ) override;
	void OnRender( TFLOAT a_fDeltaTime ) override;
	TBOOL ProcessCommand( AInputCommand a_eCommand, const Toshi::TInputInterface::InputEvent* a_pEvent, TBOOL& a_rStopEvents ) override;

private:
	ASimPlayer_PSP m_Player;
	ATerrainCollision_PSP m_TerrainCollision;
	Toshi::TRenderCamera_PSP m_Camera;
	Toshi::TVector2 m_vStick = Toshi::TVector2::VEC_ZERO;
};
