#include "AFrontendBootstrapTask_PSP.h"
#include "AGameState_PSP.h"
#include "../Input/AInputHandler_PSP.h"

TDEFINE_CLASS( AFrontendUpdateTask_PSP );
TDEFINE_CLASS( AFrontendRenderTask_PSP );

AFrontendUpdateTask_PSP::AFrontendUpdateTask_PSP()
    : m_pState( TNULL )
{
}

TBOOL AFrontendUpdateTask_PSP::OnCreate()
{
	return m_pState != TNULL;
}

TBOOL AFrontendUpdateTask_PSP::OnUpdate( TFLOAT a_fDeltaTime )
{
	const AInputHandler_PSP* pInputHandler = AInputHandler_PSP::GetSingleton();
	if ( m_pState->pActiveState == TNULL || pInputHandler == TNULL )
		return TTRUE;

	m_pState->eRequestedState = m_pState->pActiveState->OnUpdate(
	    *m_pState,
	    *pInputHandler,
	    a_fDeltaTime
	);
	return TTRUE;
}

AFrontendRenderTask_PSP::AFrontendRenderTask_PSP()
    : m_pState( TNULL )
    , m_pContext( TNULL )
{
}

TBOOL AFrontendRenderTask_PSP::OnCreate()
{
	return m_pState != TNULL && m_pContext != TNULL && m_pContext->pRenderInterface != TNULL;
}

TBOOL AFrontendRenderTask_PSP::OnUpdate( TFLOAT a_fDeltaTime )
{
	(void)a_fDeltaTime;
	const TUINT32 uiClearColor = m_pState->pActiveState != TNULL
	    ? m_pState->pActiveState->GetClearColor( *m_pState )
	    : 0xff202080;

	m_pContext->pRenderInterface->BeginScene( uiClearColor );
	if ( m_pState->pActiveState != TNULL )
		m_pState->pActiveState->OnRender( *m_pState, *m_pContext );
	m_pContext->pRenderInterface->EndScene();
	return TTRUE;
}
