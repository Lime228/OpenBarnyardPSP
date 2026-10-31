#include "AGameStateController_PSP.h"
#include <Toshi/TDebug.h>

TDEFINE_CLASS( AGameStateController_PSP );

AGameStateController_PSP::AGameStateController_PSP()
    : m_pState( TNULL )
    , m_pContext( TNULL )
{
}

TBOOL AGameStateController_PSP::OnCreate()
{
	if ( m_pState == TNULL || m_pContext == TNULL )
		return TFALSE;

	EnterState( AFrontendState_PSP_Boot );
	return TTRUE;
}

TBOOL AGameStateController_PSP::OnUpdate( TFLOAT a_fDeltaTime )
{
	m_pState->fStateTime += a_fDeltaTime;

	if ( m_pState->eState == AFrontendState_PSP_Boot )
	{
		EnterState(
		    m_pContext->bBootstrapReady
		        ? AFrontendState_PSP_Menu
		        : AFrontendState_PSP_Error
		);
	}
	else if ( m_pState->eRequestedState != m_pState->eState )
	{
		EnterState( m_pState->eRequestedState );
	}

	return TTRUE;
}

void AGameStateController_PSP::EnterState( AFrontendState_PSP a_eState )
{
	if ( m_pState->eState == a_eState && m_pState->uiTransitionCount != 0 )
		return;

	if ( m_pState->pActiveState != TNULL )
		m_pState->pActiveState->OnRemoval( *m_pState );

	m_pState->eState = a_eState;
	m_pState->eRequestedState = a_eState;
	m_pState->pActiveState = GetState( a_eState );
	m_pState->fStateTime = 0.0f;
	m_pState->uiTransitionCount += 1;

	if ( m_pState->pActiveState != TNULL )
		m_pState->pActiveState->OnInsertion( *m_pState );

	TDebug_FinalPrintf(
	    "[PSP frontend] state %u transition %u\n",
	    TUINT( a_eState ),
	    m_pState->uiTransitionCount
	);
}

AGameState_PSP* AGameStateController_PSP::GetState( AFrontendState_PSP a_eState )
{
	switch ( a_eState )
	{
		case AFrontendState_PSP_Menu: return &m_MainMenuState;
		case AFrontendState_PSP_Probe: return &m_ProbeState;
		default: return TNULL;
	}
}
