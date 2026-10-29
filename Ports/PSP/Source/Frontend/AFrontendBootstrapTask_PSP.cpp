#include "AFrontendBootstrapTask_PSP.h"

#include <Platform/PSP/Input/TInputInterface_PSP.h>

TDEFINE_CLASS( AFrontendBootstrapTask_PSP );

AFrontendBootstrapTask_PSP::AFrontendBootstrapTask_PSP()
    : m_fXOffset( 0.0f )
    , m_fModelRotation( 0.0f )
    , m_bConfirmDown( TFALSE )
    , m_uiFrameCount( 0 )
{
}

TBOOL AFrontendBootstrapTask_PSP::OnCreate()
{
	m_fXOffset       = 0.0f;
	m_fModelRotation = 0.0f;
	m_bConfirmDown   = TFALSE;
	m_uiFrameCount   = 0;
	return TTRUE;
}

TBOOL AFrontendBootstrapTask_PSP::OnUpdate( TFLOAT a_fDeltaTime )
{
	auto* pInputInterface = static_cast<Toshi::TInputInterface_PSP*>( Toshi::TInputInterface::GetSingleton() );
	if ( pInputInterface == TNULL )
		return TTRUE;

	const Toshi::TInputDeviceController_PSP& rController = pInputInterface->GetController();
	const TFLOAT fAnalogX = rController.GetAxisFloat( Toshi::TInputDeviceController::STICK_LEFT, 0 );
	m_fXOffset += fAnalogX * 120.0f * a_fDeltaTime;
	if ( m_fXOffset < -100.0f ) m_fXOffset = -100.0f;
	if ( m_fXOffset > 100.0f ) m_fXOffset = 100.0f;

	m_fModelRotation += 0.9f * a_fDeltaTime;
	m_bConfirmDown = rController.IsDown( Toshi::TInputDeviceController::BUTTON_CONTEXT_3 );
	m_uiFrameCount += 1;
	return TTRUE;
}
