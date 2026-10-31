#include "AGameStateController_PSP.h"
#include "../Input/AInputHandler_PSP.h"

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
	else
	{
		const AInputHandler_PSP* pInputHandler = AInputHandler_PSP::GetSingleton();
		if ( pInputHandler != TNULL )
		{
			m_pState->bConfirmDown = pInputHandler->IsActionDown( AInputAction_PSP_Confirm );
			if ( m_pState->eState == AFrontendState_PSP_Menu &&
			     pInputHandler->IsActionPressed( AInputAction_PSP_Confirm ) )
			{
				EnterState( AFrontendState_PSP_Probe );
			}
			else if ( m_pState->eState == AFrontendState_PSP_Probe &&
			          pInputHandler->IsActionPressed( AInputAction_PSP_Cancel ) )
			{
				EnterState( AFrontendState_PSP_Menu );
			}
		}
	}

	return TTRUE;
}

void AGameStateController_PSP::EnterState( AFrontendState_PSP a_eState )
{
	if ( m_pState->eState == a_eState && m_pState->uiTransitionCount != 0 )
		return;

	m_pState->eState = a_eState;
	m_pState->fStateTime = 0.0f;
	m_pState->uiTransitionCount += 1;

	if ( a_eState == AFrontendState_PSP_Probe )
	{
		m_pState->fXOffset = 0.0f;
		m_pState->fModelRotation = 0.0f;
	}

	TDebug_FinalPrintf(
	    "[PSP frontend] state %u transition %u\n",
	    TUINT( a_eState ),
	    m_pState->uiTransitionCount
	);
}
