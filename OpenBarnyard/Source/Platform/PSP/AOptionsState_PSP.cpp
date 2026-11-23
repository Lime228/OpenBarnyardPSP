#include "pch.h"

#include "GameInterface/AOptionsState.h"
#include "GameInterface/AAudioOptionsState.h"
#include "GameInterface/AControlOptionsState.h"
#include "GameInterface/AGameStateController.h"
#include "AOptions.h"
#include "GUI/AGUI2.h"
#include "Locale/ALocaleManager.h"
#include "Platform/PSP/AGameRuntimeReport_PSP.h"

#include <GUI/T2GUIFontManager.h>
#include <GUI/T2GUITextureSectionManager.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TDEFINE_CLASS( AOptionsState );

AOptionsState::AOptionsState()
    : m_iActivatedButtonID( -1 )
{
}

AOptionsState::~AOptionsState() = default;

TBOOL AOptionsState::ProcessCommand(
	AInputCommand a_eInputCommand,
	const TInputInterface::InputEvent* a_pInputEvent,
	TBOOL& a_rStopEvents
)
{
	return BaseClass::ProcessCommand( a_eInputCommand, a_pInputEvent, a_rStopEvents );
}

TBOOL AOptionsState::OnUpdate( TFLOAT a_fDeltaTime )
{
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.iMenuState              = m_eMenuState;
	AGUI2MenuItem* pFocusedItem     = m_oMenu.GetFocusedMenuItem();
	rReport.iMenuFocusID            = pFocusedItem != TNULL ? TSTATICCAST( AGUI2Button, pFocusedItem )->GetId() : -1;
	const AOptions* pOptions        = AOptions::GetSingleton();
	rReport.iOptionEffects          = pOptions->GetSfxVolume();
	rReport.iOptionMusic            = pOptions->GetMusicVolume();
	rReport.iOptionSpeech           = pOptions->GetVOVolume();
	rReport.bOptionInverted          = pOptions->IsMouseInverted();
	rReport.fOptionSensitivity      = pOptions->GetSensitivity();

	return BaseClass::OnUpdate( a_fDeltaTime );
}

const TWCHAR* AOptionsState::GetButtonText( TINT a_iButton )
{
	return g_pLocaleManager->GetString( OPTIONSBUTTON_TEXTS[ a_iButton ] );
}

void AOptionsState::OnInsertion()
{
	ASetGameRuntimePhase_PSP( "frontend-options-inserting" );

	T2GUIFont* pFont = T2GUIFontManager::FindFont( AGUI2STYLE_FONT_PRIMARY );
	if ( pFont == TNULL || g_pLocaleManager == TNULL )
	{
		ASetGameRuntimePhase_PSP( "frontend-options-data-failed" );
		return;
	}

	for ( TINT i = 0; i < OPTIONSBUTTON_NUMOF; i++ )
	{
		const TWCHAR* wszText = GetButtonText( i );

		m_aButtons[ i ].Create( pFont, pFont->GetTextWidth( wszText ), wszText, 10.0f );
		m_aButtons[ i ].SetFocusedColour( AGUI2STYLE_COLOR_FOCUSED );
		m_aButtons[ i ].SetEnabledColour( AGUI2STYLE_COLOR_SECONDARY );
		m_aButtons[ i ].SetDisabledColour( AGUI2STYLE_COLOR_DISABLED );
		m_aButtons[ i ].SetFocusedScale( 1.0f );
		m_aButtons[ i ].OnFocusLost();
		m_aButtons[ i ].SetId( i );
		m_aButtons[ i ].SetTextShadow( TTRUE, 0xa0000000, 0.6f );
		m_aButtons[ i ].SetTextShadowOffset( 5.0f );
		m_oMenu.AddMenuItem( m_aButtons[ i ] );
	}

	m_oMenu.SetFocusAt( m_aButtons[ 0 ] );
	m_oDialogTitle.SetText( g_pLocaleManager->GetString( 284 ) );

	BaseClass::OnInsertion();
	ASetGameRuntimePhase_PSP( "frontend-options-ready" );
}

void AOptionsState::OnRemoval()
{
	for ( TINT i = 0; i < OPTIONSBUTTON_NUMOF; i++ )
		m_aButtons[ i ].RemoveSelf();

	BaseClass::OnRemoval();
}

void AOptionsState::OnActivate()
{
	m_iActivatedButtonID = -1;
	BaseClass::OnActivate();
	if ( m_oMenu.GetFocusedMenuItem() != TNULL )
		ASetGameRuntimePhase_PSP( "frontend-options-ready" );
}

void AOptionsState::OnMenuItemActivated( AGUI2MenuItem* a_pMenuItem )
{
	AGUI2Button* pButton = TSTATICCAST( AGUI2Button, a_pMenuItem );
	m_iActivatedButtonID = pButton->GetId();

	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.iMenuActivatedButtonID  = m_iActivatedButtonID;
	rReport.uiMenuActivationCount++;
	OnButtonActivated( pButton );
}

void AOptionsState::OnMenuClose()
{
	AGetGameRuntimeReport_PSP().uiMenuCloseCount++;

	if ( m_iActivatedButtonID != -1 )
	{
		const TCHAR* const aSections[] = { "left_arrow", "left_arrow2", "Statue_GoldStar" };
		for ( const TCHAR* pName : aSections )
		{
			T2GUITextureSection* pSection = T2GUITextureSectionManager::GetTextureSection( pName );
			if ( pSection == TNULL || pSection->GetMaterial() == TNULL )
			{
				OnActivate();
				ASetGameRuntimeFailedPath_PSP( pName );
				ASetGameRuntimePhase_PSP( "frontend-options-widgets-missing" );
				return;
			}
		}
		AGameState* pState = m_iActivatedButtonID == OPTIONSBUTTON_AUDIO ?
		    static_cast<AGameState*>( new AAudioOptionsState() ) : new AControlOptionsState();
		AGameStateController::GetSingleton()->PushState( pState );
		return;
	}

	ASetGameRuntimePhase_PSP( "frontend-options-return" );
	AGameState::Remove();
}
