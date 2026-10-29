#include "ToshiPCH.h"
#include "TInputDeviceController_PSP.h"

TOSHI_NAMESPACE_START

TDEFINE_CLASS_NORUNTIME( TInputDeviceController_PSP );

static constexpr TInputDevice::Doodad s_aPSPButtons[] = {
	TInputDeviceController::BUTTON_DPAD_UP,
	TInputDeviceController::BUTTON_DPAD_RIGHT,
	TInputDeviceController::BUTTON_DPAD_DOWN,
	TInputDeviceController::BUTTON_DPAD_LEFT,
	TInputDeviceController::BUTTON_SELECT,
	TInputDeviceController::BUTTON_START,
	TInputDeviceController::BUTTON_CONTEXT_1,
	TInputDeviceController::BUTTON_CONTEXT_2,
	TInputDeviceController::BUTTON_CONTEXT_3,
	TInputDeviceController::BUTTON_CONTEXT_4,
	TInputDeviceController::BUTTON_SHOULDER_LEFT_1,
	TInputDeviceController::BUTTON_SHOULDER_RIGHT_1,
};

TInputDeviceController_PSP::TInputDeviceController_PSP()
    : m_oState{}
    , m_oOldState{}
    , m_bPolled( TFALSE )
{
	m_oState.Lx    = STICK_CENTRE;
	m_oState.Ly    = STICK_CENTRE;
	m_oOldState.Lx = STICK_CENTRE;
	m_oOldState.Ly = STICK_CENTRE;
}

TBOOL TInputDeviceController_PSP::Acquire()
{
	if ( sceCtrlSetSamplingCycle( 0 ) < 0 || sceCtrlSetSamplingMode( PSP_CTRL_MODE_ANALOG ) < 0 )
		return TFALSE;

	SceCtrlData oState{};
	if ( sceCtrlPeekBufferPositive( &oState, 1 ) < 0 )
		return TFALSE;

	m_oState    = oState;
	m_oOldState = oState;
	m_bPolled   = TFALSE;
	SetAcquired( TTRUE );
	return TTRUE;
}

TBOOL TInputDeviceController_PSP::Unacquire()
{
	SetAcquired( TFALSE );
	m_bPolled = TFALSE;
	return TTRUE;
}

void TInputDeviceController_PSP::Release()
{
}

void TInputDeviceController_PSP::Update( TFLOAT )
{
	SceCtrlData oState{};
	const TINT  iSamples = sceCtrlPeekBufferPositive( &oState, 1 );
	if ( iSamples <= 0 )
	{
		m_bPolled = TFALSE;
		return;
	}

	m_oOldState = m_oState;
	m_oState    = oState;
	UpdateVirtualButtons();
	m_bPolled = TTRUE;
}

TBOOL TInputDeviceController_PSP::Flush()
{
	m_oOldState = m_oState;
	StopAllRepeats();
	return TTRUE;
}

TINT TInputDeviceController_PSP::ProcessEvents( EventEmitter& a_rEmitter, TFLOAT a_fDeltaTime )
{
	if ( !m_bPolled )
		return 0;

	TINT iNumProcessed = ProcessRepeats( a_rEmitter, a_fDeltaTime );
	iNumProcessed += ProcessVirtualButtons( a_rEmitter, a_fDeltaTime );

	for ( TSIZE i = 0; i < TARRAYSIZE( s_aPSPButtons ); i++ )
	{
		const Doodad iDoodad  = s_aPSPButtons[ i ];
		const TBOOL  bIsDown  = IsDown( iDoodad );
		const TBOOL  bWasDown = WasDown( iDoodad );

		if ( bIsDown != bWasDown )
		{
			a_rEmitter.Throw( TInputInterface::InputEvent(
			    this,
			    iDoodad,
			    bIsDown ? TInputInterface::EVENT_TYPE_GONE_DOWN : TInputInterface::EVENT_TYPE_GONE_UP
			) );
			iNumProcessed += 1;
		}
	}

	a_rEmitter.Throw( TInputInterface::InputEvent(
	    this,
	    STICK_LEFT,
	    TInputInterface::EVENT_TYPE_VIRTUAL,
	    GetAxisFloat( STICK_LEFT, 0 ),
	    GetAxisFloat( STICK_LEFT, 1 )
	) );

	return iNumProcessed + 1;
}

TINT TInputDeviceController_PSP::GetButtonCount() const
{
	return TINT( TARRAYSIZE( s_aPSPButtons ) );
}

TINT TInputDeviceController_PSP::GetAxisCount() const
{
	return 1;
}

TBOOL TInputDeviceController_PSP::IsDown( Doodad a_iDoodad ) const
{
	const TUINT uiButton = TranslateDoodadToPSPButton( a_iDoodad );
	return uiButton != 0 ? ( m_oState.Buttons & uiButton ) != 0 : TInputDeviceController::IsDown( a_iDoodad );
}

TBOOL TInputDeviceController_PSP::WasDown( Doodad a_iDoodad ) const
{
	const TUINT uiButton = TranslateDoodadToPSPButton( a_iDoodad );
	return uiButton != 0 ? ( m_oOldState.Buttons & uiButton ) != 0 : TInputDeviceController::WasDown( a_iDoodad );
}

TINT TInputDeviceController_PSP::GetAxisInt( Doodad a_iDoodad, TINT a_iAxis ) const
{
	const TINT iValue = GetAxisInt2( a_iDoodad, a_iAxis );
	return iValue >= -STICK_DEADZONE && iValue <= STICK_DEADZONE ? 0 : iValue;
}

TINT TInputDeviceController_PSP::GetAxisInt2( Doodad a_iDoodad, TINT a_iAxis ) const
{
	if ( a_iDoodad != STICK_LEFT )
		return 0;

	TINT iValue;
	if ( a_iAxis == 0 )
		iValue = TINT( m_oState.Lx ) - STICK_CENTRE;
	else if ( a_iAxis == 1 )
		iValue = TINT( m_oState.Ly ) - STICK_CENTRE;
	else
		return 0;

	if ( iValue < -STICK_SATURATE ) iValue = -STICK_SATURATE;
	if ( iValue > STICK_SATURATE ) iValue = STICK_SATURATE;
	return iValue;
}

TFLOAT TInputDeviceController_PSP::GetAxisFloat( Doodad a_iDoodad, TINT a_iAxis ) const
{
	return TFLOAT( GetAxisInt( a_iDoodad, a_iAxis ) ) * ( 1.0f / TFLOAT( STICK_SATURATE ) );
}

TFLOAT TInputDeviceController_PSP::GetAxisFloat2( Doodad a_iDoodad, TINT a_iAxis ) const
{
	return TFLOAT( GetAxisInt2( a_iDoodad, a_iAxis ) ) * ( 1.0f / TFLOAT( STICK_SATURATE ) );
}

TBOOL TInputDeviceController_PSP::IsEnabled() const
{
	return TTRUE;
}

TINT TInputDeviceController_PSP::GetPortIndex()
{
	return 0;
}

TUINT TInputDeviceController_PSP::TranslateDoodadToPSPButton( Doodad a_iDoodad )
{
	switch ( a_iDoodad )
	{
		case BUTTON_DPAD_UP: return PSP_CTRL_UP;
		case BUTTON_DPAD_RIGHT: return PSP_CTRL_RIGHT;
		case BUTTON_DPAD_DOWN: return PSP_CTRL_DOWN;
		case BUTTON_DPAD_LEFT: return PSP_CTRL_LEFT;
		case BUTTON_SELECT: return PSP_CTRL_SELECT;
		case BUTTON_START: return PSP_CTRL_START;
		case BUTTON_CONTEXT_1: return PSP_CTRL_TRIANGLE;
		case BUTTON_CONTEXT_2: return PSP_CTRL_CIRCLE;
		case BUTTON_CONTEXT_3: return PSP_CTRL_CROSS;
		case BUTTON_CONTEXT_4: return PSP_CTRL_SQUARE;
		case BUTTON_SHOULDER_LEFT_1: return PSP_CTRL_LTRIGGER;
		case BUTTON_SHOULDER_RIGHT_1: return PSP_CTRL_RTRIGGER;
		default: return 0;
	}
}

TOSHI_NAMESPACE_END
