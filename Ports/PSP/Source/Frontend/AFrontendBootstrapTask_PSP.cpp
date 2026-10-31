#include "AFrontendBootstrapTask_PSP.h"

#include <Platform/PSP/Input/TInputInterface_PSP.h>

TDEFINE_CLASS( AFrontendInputTask_PSP );
TDEFINE_CLASS( AFrontendUpdateTask_PSP );
TDEFINE_CLASS( AFrontendRenderTask_PSP );

AFrontendInputTask_PSP::AFrontendInputTask_PSP()
    : m_pState( TNULL )
{
}

TBOOL AFrontendInputTask_PSP::OnCreate()
{
	return m_pState != TNULL;
}

TBOOL AFrontendInputTask_PSP::OnUpdate( TFLOAT a_fDeltaTime )
{
	(void)a_fDeltaTime;
	auto* pInputInterface = static_cast<Toshi::TInputInterface_PSP*>( Toshi::TInputInterface::GetSingleton() );
	if ( pInputInterface == TNULL )
		return TTRUE;

	const Toshi::TInputDeviceController_PSP& rController = pInputInterface->GetController();
	m_pState->fAnalogX = rController.GetAxisFloat( Toshi::TInputDeviceController::STICK_LEFT, 0 );
	m_pState->bConfirmDown = rController.IsDown( Toshi::TInputDeviceController::BUTTON_CONTEXT_3 );
	return TTRUE;
}

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
	m_pState->fXOffset += m_pState->fAnalogX * 120.0f * a_fDeltaTime;
	if ( m_pState->fXOffset < -100.0f ) m_pState->fXOffset = -100.0f;
	if ( m_pState->fXOffset > 100.0f ) m_pState->fXOffset = 100.0f;

	m_pState->fModelRotation += 0.9f * a_fDeltaTime;
	m_pState->uiFrameCount += 1;
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
	const TUINT32 uiClearColor = m_pContext->bBootstrapReady
	    ? ( m_pState->bConfirmDown ? 0xff402010 : 0xff201810 )
	    : 0xff202080;

	m_pContext->pRenderInterface->BeginScene( uiClearColor );
	m_pContext->pRenderInterface->DrawColourTriangle( m_pState->fXOffset );
	if ( m_pContext->bMeshReady && m_pContext->pVertices != TNULL && m_pContext->pIndices != TNULL )
		m_pContext->pRenderInterface->DrawIndexedColourTriangles( *m_pContext->pVertices, *m_pContext->pIndices );
	if ( m_pContext->pTexture != TNULL )
		m_pContext->pRenderInterface->DrawTextureSprite( *m_pContext->pTexture, 176.0f + m_pState->fXOffset, 72.0f, 128.0f, 128.0f );
	if ( m_pContext->pModel != TNULL && m_pContext->pTexture != TNULL && m_pContext->pMaterial != TNULL )
		m_pContext->pRenderInterface->DrawModel( *m_pContext->pModel, m_pState->fModelRotation, *m_pContext->pMaterial, m_pContext->pTexture );
	m_pContext->pRenderInterface->EndScene();
	return TTRUE;
}
