#include "pch.h"

#include "GameInterface/AFrontEndMainMenuState2.h"
#include "GUI/AGUI2.h"
#include "Locale/ALocaleManager.h"
#include "Platform/PSP/AGameRuntimeReport_PSP.h"
#include "SoundBank/ui.h"

#include <GUI/T2GUIFontManager.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TDEFINE_CLASS( AFrontEndMainMenuState2 );

AFrontEndMainMenuState2::AFrontEndMainMenuState2( AWindmillHelper* a_pWindmillHelper, TBOOL a_bFlag )
{
	m_iActivatedButtonID = -1;
	m_pWindmillHelper    = a_pWindmillHelper;
	m_bFlag              = a_bFlag;
	m_fAFKTime           = 0.0f;

	m_pButtonRotations                            = new TFLOAT[ FRONTENDBUTTON_NUMOF ];
	m_pButtonRotations[ FRONTENDBUTTON_NEWGAME ]  = TMath::DegToRad( 2.0f );
	m_pButtonRotations[ FRONTENDBUTTON_LOADGAME ] = TMath::DegToRad( -3.0f );
	m_pButtonRotations[ FRONTENDBUTTON_OPTIONS ]  = TMath::DegToRad( 2.0f );
	m_pButtonRotations[ FRONTENDBUTTON_BONUSES ]  = TMath::DegToRad( -2.0f );
	m_pButtonRotations[ FRONTENDBUTTON_ANTICS ]   = TMath::DegToRad( 2.0f );
	m_pButtonRotations[ FRONTENDBUTTON_QUIT ]     = TMath::DegToRad( -1.0f );
}

AFrontEndMainMenuState2::~AFrontEndMainMenuState2() = default;

TBOOL AFrontEndMainMenuState2::StartIntroVideo()
{
	return TFALSE;
}

void AFrontEndMainMenuState2::SetupCamera()
{
}

TBOOL AFrontEndMainMenuState2::ProcessInput( const TInputInterface::InputEvent* a_pInputEvent )
{
	if ( a_pInputEvent != TNULL && a_pInputEvent->GetEventType() == TInputInterface::EVENT_TYPE_GONE_DOWN )
	{
		AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
		rReport.iMenuLastPressedDoodad  = a_pInputEvent->GetDoodad();
		rReport.uiMenuInputDownCount++;
	}

	return ABYardMenuState::ProcessInput( a_pInputEvent );
}

TBOOL AFrontEndMainMenuState2::ProcessCommand( AInputCommand a_eInputCommand, const TInputInterface::InputEvent* a_pInputEvent, TBOOL& a_rStopEvents )
{
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.iMenuLastCommand        = a_eInputCommand;
	rReport.iMenuLastEventType      = a_pInputEvent != TNULL ? a_pInputEvent->GetEventType() : -1;
	rReport.iMenuLastDoodad         = a_pInputEvent != TNULL ? a_pInputEvent->GetDoodad() : -1;
	if ( a_eInputCommand != AInputCommand_Move )
	{
		rReport.iMenuLastActionCommand   = a_eInputCommand;
		rReport.iMenuLastActionEventType = rReport.iMenuLastEventType;
		rReport.iMenuLastActionDoodad    = rReport.iMenuLastDoodad;
		rReport.uiMenuActionCommandCount++;

		if ( a_eInputCommand == AInputCommand_Up )
			rReport.uiMenuCommandUpCount++;
		if ( a_eInputCommand == AInputCommand_Down )
			rReport.uiMenuCommandDownCount++;
		if ( a_eInputCommand == AInputCommand_OK )
			rReport.uiMenuCommandOKCount++;
	}
	return ABYardMenuState::ProcessCommand( a_eInputCommand, a_pInputEvent, a_rStopEvents );
}

TBOOL AFrontEndMainMenuState2::OnUpdate( TFLOAT a_fDeltaTime )
{
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.iMenuState              = m_eMenuState;
	AGUI2MenuItem* pFocusedItem     = m_oMenu.GetFocusedMenuItem();
	rReport.iMenuFocusID            = pFocusedItem != TNULL ? TSTATICCAST( AGUI2Button, pFocusedItem )->GetId() : -1;

	UpdateMenuButtons( a_fDeltaTime );
	if ( m_eMenuState == MENUSTATE_MENU_VISIBLE && m_bIgnoreInputs )
		UpdateFocusedButton( a_fDeltaTime );

	return BaseClass::OnUpdate( a_fDeltaTime );
}

void AFrontEndMainMenuState2::OnInsertion()
{
	ms_bIsInserted = TTRUE;
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.iMenuState               = MENUSTATE_NOTHING;
	rReport.iMenuFocusID             = -1;
	rReport.iMenuLastPressedDoodad   = -1;
	rReport.iMenuLastCommand         = -1;
	rReport.iMenuLastEventType       = -1;
	rReport.iMenuLastDoodad          = -1;
	rReport.iMenuLastActionCommand   = -1;
	rReport.iMenuLastActionEventType = -1;
	rReport.iMenuLastActionDoodad    = -1;
	rReport.iMenuActivatedButtonID   = -1;
	rReport.uiMenuInputDownCount     = 0;
	rReport.uiMenuActionCommandCount = 0;
	rReport.uiMenuCommandUpCount     = 0;
	rReport.uiMenuCommandDownCount   = 0;
	rReport.uiMenuCommandOKCount     = 0;
	rReport.uiMenuActivationCount    = 0;
	rReport.uiMenuCloseCount         = 0;
	ASetGameRuntimePhase_PSP( "frontend-menu-inserting" );

	T2GUIFont* pFont = T2GUIFontManager::FindFont( AGUI2STYLE_FONT_PRIMARY );
	if ( pFont == TNULL || g_pLocaleManager == TNULL )
	{
		ASetGameRuntimePhase_PSP( "frontend-menu-data-failed" );
		return;
	}
	ASetGameRuntimePhase_PSP( "frontend-menu-data-ready" );

	for ( TINT i = 0; i < FRONTENDBUTTON_NUMOF; i++ )
	{
		const TWCHAR* wszText = g_pLocaleManager->GetString( FRONTENDBUTTON_TEXTS[ i ] );

		m_aButtons[ i ].Create( pFont, pFont->GetTextWidth( wszText ), wszText, 5.0f );
		m_aButtons[ i ].SetFocusedColour( AGUI2STYLE_COLOR_FOCUSED );
		m_aButtons[ i ].SetEnabledColour( AGUI2STYLE_COLOR_PRIMARY );
		m_aButtons[ i ].SetDisabledColour( AGUI2STYLE_COLOR_DISABLED );
		m_aButtons[ i ].SetFocusedScale( 1.0f );
		m_aButtons[ i ].OnFocusLost();
		m_aButtons[ i ].SetId( i );
		m_aButtons[ i ].SetScaleOnFocus( TFALSE );
		m_aButtons[ i ].GetTransform().Rotate( m_pButtonRotations[ i ] );
		m_aButtons[ i ].SetImage( "Panel_01", 1.25f, 1.5f );
		m_aButtons[ i ].SetTextShadow( TTRUE, 0xa0000000, 0.6f );
		m_aButtons[ i ].SetTextShadowOffset( 5.0f, 5.0f );
		m_oMenu.AddMenuItem( m_aButtons[ i ] );
	}
	ASetGameRuntimePhase_PSP( "frontend-menu-buttons-ready" );

	m_oMenu.SetFocusAt( m_aButtons[ 0 ] );
	m_oMenu.SetItemFocusCallback( MenuItemFocusCallback );

	m_oDialogTitleBackground.Hide();
	m_Rectangle1.SetAttachment( T2GUIATTACHMENT_TOPCENTER, T2GUIATTACHMENT_TOPCENTER );
	m_Rectangle1.SetTransform( 0.0f, 10.0f );
	m_Rectangle1.Hide();

	m_Rectangle2.SetAttachment( T2GUIATTACHMENT_TOPCENTER, T2GUIATTACHMENT_TOPCENTER );
	m_Rectangle2.SetTransform( 5.0f, 10.0f );
	m_Rectangle2.SetColour( 0x64000000 );
	m_Rectangle2.Hide();

	AGUI2::GetRootElement()->AddChildTail( m_Rectangle2 );
	AGUI2::GetRootElement()->AddChildTail( m_Rectangle1 );
	ASetGameRuntimePhase_PSP( "frontend-menu-base-inserting" );
	BaseClass::OnInsertion();
	ASetGameRuntimePhase_PSP( "frontend-menu-base-ready" );

	m_oMenu.SetFocusSound( soundbank::UI_FE_MOVE );
	m_oMenu.SetSelectSound( soundbank::UI_FE_SELECT );
	m_oMenu.SetBackSound( soundbank::UI_MENUBACK );
	m_oMenu.SetNegativeSound( soundbank::UI_NEGATIVE );
	m_oDialog.SetTransform( 0.0f, 50.0f );
	m_oMenu.SetTransform( 0.0f, 50.0f );
	m_oDialog.Hide();
	m_bFocusedElementBloated = TFALSE;

	ASetGameRuntimePhase_PSP( "frontend-menu-ready" );
}

void AFrontEndMainMenuState2::OnRemoval()
{
	ms_bIsInserted = TFALSE;
	for ( TINT i = 0; i < FRONTENDBUTTON_NUMOF; i++ )
		m_aButtons[ i ].RemoveSelf();

	m_Rectangle1.RemoveSelf();
	m_Rectangle2.RemoveSelf();
	BaseClass::OnRemoval();
}

void AFrontEndMainMenuState2::OnActivate()
{
	m_fAFKTime           = 0.0f;
	m_iActivatedButtonID = -1;

	BaseClass::OnActivate();
	m_eMenuState = MENUSTATE_MENU_APPEAR;
	m_oDialogTitleBackground.Hide();
}

void AFrontEndMainMenuState2::OnDeactivate()
{
	m_Rectangle1.Hide();
	m_Rectangle2.Hide();
	BaseClass::OnDeactivate();
}

void AFrontEndMainMenuState2::OnMenuItemActivated( AGUI2MenuItem* a_pMenuItem )
{
	AGUI2Button* pButton = TSTATICCAST( AGUI2Button, a_pMenuItem );
	m_iActivatedButtonID = pButton->GetId();
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.iMenuActivatedButtonID  = m_iActivatedButtonID;
	rReport.uiMenuActivationCount++;
	OnButtonActivated( pButton );
	m_eMenuState = MENUSTATE_MENU_VISIBLE;
}

void AFrontEndMainMenuState2::OnMenuClose()
{
	if ( m_iActivatedButtonID == -1 )
		return;

	ASetGameRuntimePhase_PSP( "frontend-menu-selection" );
	AGetGameRuntimeReport_PSP().uiMenuCloseCount++;
	m_iActivatedButtonID = -1;
	m_bIgnoreInputs      = TFALSE;
	m_fMenuOpacity       = 0.0f;
	m_oRootElement.Show();
	m_eMenuState = MENUSTATE_MENU_APPEAR;
}

TBOOL __stdcall AFrontEndMainMenuState2::OnQuitPopupResponse( AFrontEndMainMenuState2*, AMessagePopupState*, AMessagePopupState::ResponseEvent* )
{
	return TFALSE;
}
