#include "AInputHandler_PSP.h"

#include <Platform/PSP/Input/TInputInterface_PSP.h>

TDEFINE_CLASS( AInputHandler_PSP );

AInputHandler_PSP::AInputHandler_PSP()
    : m_uiActionsDown( 0 )
    , m_uiActionsPressed( 0 )
    , m_fMoveX( 0.0f )
{
}

TBOOL AInputHandler_PSP::OnCreate()
{
	m_uiActionsDown = 0;
	m_uiActionsPressed = 0;
	m_fMoveX = 0.0f;
	return Toshi::TInputInterface::GetSingleton() != TNULL;
}

TBOOL AInputHandler_PSP::OnUpdate( TFLOAT a_fDeltaTime )
{
	(void)a_fDeltaTime;
	auto* pInputInterface = static_cast<Toshi::TInputInterface_PSP*>( Toshi::TInputInterface::GetSingleton() );
	if ( pInputInterface == TNULL )
		return TTRUE;

	const Toshi::TInputDeviceController_PSP& rController = pInputInterface->GetController();
	const TUINT32 uiPreviousActions = m_uiActionsDown;
	m_uiActionsDown = 0;

	if ( rController.IsDown( Toshi::TInputDeviceController::BUTTON_CONTEXT_3 ) )
		m_uiActionsDown |= AInputAction_PSP_Confirm;
	if ( rController.IsDown( Toshi::TInputDeviceController::BUTTON_CONTEXT_2 ) )
		m_uiActionsDown |= AInputAction_PSP_Cancel;
	if ( rController.IsDown( Toshi::TInputDeviceController::BUTTON_START ) )
		m_uiActionsDown |= AInputAction_PSP_Pause;

	m_uiActionsPressed = m_uiActionsDown & ~uiPreviousActions;
	m_fMoveX = rController.GetAxisFloat( Toshi::TInputDeviceController::STICK_LEFT, 0 );
	return TTRUE;
}
