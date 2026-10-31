#pragma once

#include "AFrontendBootstrapTask_PSP.h"
#include "AFrontEndMainMenuState2_PSP.h"
#include "AFrontendInfoState_PSP.h"
#include "AProbeState_PSP.h"

class AGameStateController_PSP : public Toshi::TTask
{
public:
	TDECLARE_CLASS( AGameStateController_PSP, Toshi::TTask );

public:
	AGameStateController_PSP();

	virtual TBOOL OnCreate() override;
	virtual TBOOL OnUpdate( TFLOAT a_fDeltaTime ) override;

	void SetState( AFrontendBootstrapState_PSP* a_pState ) { m_pState = a_pState; }
	void SetContext( const AFrontendRenderContext_PSP* a_pContext ) { m_pContext = a_pContext; }

private:
	void EnterState( AFrontendState_PSP a_eState );
	AGameState_PSP* GetState( AFrontendState_PSP a_eState );

private:
	AFrontendBootstrapState_PSP*      m_pState;
	const AFrontendRenderContext_PSP* m_pContext;
	AFrontEndMainMenuState2_PSP       m_MainMenuState;
	AProbeState_PSP                   m_ProbeState;
	AFrontendInfoState_PSP            m_InfoState;
};
