#include "pch.h"
#include "AStartMenuState.h"
#include "AGameStateController.h"
#include "AOptionsState.h"
#include "GUI/AGUI2.h"
#include "Locale/ALocaleManager.h"
#ifdef TOSHI_SKU_PSP
#  include "Platform/PSP/AGameRuntimeReport_PSP.h"
#  include "Platform/PSP/ASimState_PSP.h"
#else
#include "Sound/APositionalSoundSet.h"
#include "Sound/ASoundManager.h"
#include "Sound/AMultiplexStream.h"
#include "Sound/AMusicManager.h"
#include "GUI/AGUI2ModelRendererScreen.h"
#include "Helpers/AGameLoader.h"
#include "MiniGame/AMiniGameManager.h"
#include "Tasks/ARootTask.h"
#include "Quests/AQuestManager.h"
#include "Quests/AQuestInstance.h"
#include "Quests/AQuest.h"
#endif

#include <GUI/T2GUIFontManager.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

// NOTE: GetClass 0042ef00, CreateObject 0042f270, CreateObjectInPlace 0042f2d0
TDEFINE_CLASS_NORUNTIME( AStartMenuState );

#ifndef TOSHI_SKU_PSP
TPSTRING8_DECLARE( AGopherShopGame );
TPSTRING8_DECLARE( ABarnExploreMinigameState );
#endif

// $Barnyard: FUNCTION 0042f0a0
AStartMenuState::AStartMenuState()
    : m_fnQuitCallback( TNULL )
    , m_pQuitCallbackUserData( TNULL )
{
	ms_aButtonTexts[ BUTTON_QUIT ] = 333;

	m_pButtonRotations                   = new TFLOAT[ BUTTON_NUMOF ];
	m_pButtonRotations[ BUTTON_RESUME ]  = TMath::DegToRad( 2.0f );
	m_pButtonRotations[ BUTTON_RESTART ] = TMath::DegToRad( -3.0f );
	m_pButtonRotations[ BUTTON_OPTIONS ] = TMath::DegToRad( -2.0f );
	m_pButtonRotations[ BUTTON_HELP ]    = TMath::DegToRad( 2.0f );
	m_pButtonRotations[ BUTTON_QUIT ]    = TMath::DegToRad( 3.0f );
}

// $Barnyard: FUNCTION 0042ef10
// $Barnyard: FUNCTION 0042f1a0
AStartMenuState::~AStartMenuState()
{
}

// $Barnyard: FUNCTION 0042efb0
TBOOL AStartMenuState::ProcessCommand( AInputCommand a_eInputCommand, const TInputInterface::InputEvent* a_pInputEvent, TBOOL& a_rStopEvents )
{
	// START closes the menu like CANCEL
	if ( m_eMenuState == MENUSTATE_MENU_VISIBLE && !m_bIgnoreInputs && a_eInputCommand == AInputCommand_Start )
		a_eInputCommand = AInputCommand_Cancel;

	return ABYardMenuState::ProcessCommand( a_eInputCommand, a_pInputEvent, a_rStopEvents );
}

// $Barnyard: FUNCTION 0042f010
TBOOL AStartMenuState::OnUpdate( TFLOAT a_fDeltaTime )
{
#ifdef TOSHI_SKU_PSP
	if ( m_bQuitConfirmed_PSP )
	{
		AGameStateController* pController = AGameStateController::GetSingleton();
		Remove();
		TSTATICCAST( ASimState_PSP, pController->GetCurrentState() )->ReturnToFrontend();
		return TTRUE;
	}

	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.iMenuState = m_eMenuState;
	AGUI2MenuItem* pFocus = m_oMenu.GetFocusedMenuItem();
	rReport.iMenuFocusID = pFocus != TNULL ? TSTATICCAST( AGUI2Button, pFocus )->GetId() : -1;
#endif

	UpdateMenuButtons( a_fDeltaTime );

	if ( m_eMenuState == MENUSTATE_MENU_VISIBLE && m_bIgnoreInputs == TTRUE )
		UpdateFocusedButton( a_fDeltaTime );

	return ABYardMenuState::OnUpdate( a_fDeltaTime );
}

// $Barnyard: FUNCTION 0042f4b0
void AStartMenuState::OnInsertion()
{
#ifndef TOSHI_SKU_PSP
	if ( AGameStateController::GetSingleton()->FindGameStateOnStack( TPS8( ABarnExploreMinigameState ) ) )
	{
		AMultiplexStream::GetSingleton()->Pause( TTRUE );
	}
#endif

	m_bHasRectangle2 = TTRUE;

	T2GUIFont* pFont = T2GUIFontManager::FindFont( "Rekord26" );

	// Help shows the summary of the last assigned quest
	m_bHasHint = TFALSE;

#ifndef TOSHI_SKU_PSP
	if ( AQuestManager::IsSingletonCreated() && AQuestManager::GetSingleton()->GetNumAssignedQuests() > 0 )
	{
		AQuestManager* pQuestManager = AQuestManager::GetSingleton();

		if ( pQuestManager->GetAssignedQuest( pQuestManager->GetNumAssignedQuests() - 1 )->GetQuest()->GetSummaryId() > 0 )
			m_bHasHint = TTRUE;
	}
#endif

	// Title
	const TWCHAR* wszTitle = g_pLocaleManager->GetString( 288 );
	m_oTitle.Create( pFont, pFont->GetTextWidth( wszTitle, 1.2f ) );
	m_oTitle.SetText( wszTitle );
	m_oTitle.SetAttachment( T2GUIATTACHMENT_TOPCENTER, T2GUIATTACHMENT_TOPCENTER );
	m_oTitle.SetColour( 0xfffff0af );
	m_oTitle.SetTransform( 0.0f, 20.0f );
	m_oTitle.SetScale( 1.2f );
	m_oTitle.SetShadow( TTRUE, 0xa0000000 );
	m_oTitle.SetShadowAlpha( 0.6f );
	m_oTitle.SetShadowOffset( m_oDialogTitle.GetScale() * 5.0f, m_oDialogTitle.GetScale() * 5.0f );

	// Buttons
	for ( TINT i = 0; i < BUTTON_NUMOF; i++ )
	{
#ifdef TOSHI_SKU_PSP
		if ( i == BUTTON_RESTART || i == BUTTON_HELP )
			continue;

		const TWCHAR* wszText = g_pLocaleManager->GetString( ms_aButtonTexts[ i ] );
#else
		// Restart and Help are only there in a minigame, Help also when there's a hint
		if ( !AGameLoader::IsInMiniGame() && ( i == BUTTON_RESTART || i == BUTTON_HELP ) && !( m_bHasHint && i == BUTTON_HELP ) )
			continue;

		const TWCHAR* wszText;

		if ( i == BUTTON_QUIT && AGameLoader::IsInMiniGame() == TTRUE && AGameLoader::IsStoryMode() == TTRUE )
		{
			// Back to the farm
			wszText = g_pLocaleManager->GetString( ( AGameLoader::WasBarnExploreMiniGame() == TTRUE ) ? 336 : 337 );
		}
		else
		{
			wszText = g_pLocaleManager->GetString( ms_aButtonTexts[ i ] );
		}
#endif

		AGUI2Button& rButton = m_aButtons[ i ];
		rButton.Create( pFont, pFont->GetTextWidth( wszText ), wszText, 5.0f );
		rButton.SetFocusedColour( 0xffffffff );
		rButton.SetEnabledColour( 0xffffc800 );
		rButton.SetDisabledColour( 0xff4b4b4b );
		rButton.SetFocusedScale( 1.0f );
		rButton.OnFocusLost();
		rButton.SetId( i );
		rButton.SetScaleOnFocus( TFALSE );
		rButton.SetImage( "Panel_01", 1.25f, 1.5f );
		rButton.GetTransform().Rotate( m_pButtonRotations[ i ] );

		// Make sure the button isn't wider than half of the screen
		TFLOAT fWidth  = 0.0f;
		TFLOAT fHeight = 0.0f;
		rButton.GetDimensions( fWidth, fHeight );

		if ( AGUI2::GetRootElement()->GetWidth() < fWidth + fWidth )
		{
			rButton.GetTextBox().SetWidth( AGUI2::GetRootElement()->GetWidth() * 0.5f );
			rButton.ResizeImage( 1.0f, 1.0f );
		}

		AGUI2TextBox& rText = rButton.GetTextBox();
		rText.SetShadow( TTRUE, 0xa0000000 );
		rText.SetShadowAlpha( 0.6f );
		rText.SetShadowOffset( rText.GetScale() * 5.0f, rText.GetScale() * 5.0f );

		m_oMenu.AddMenuItem( rButton );
	}

	m_oMenu.SetFocusAt( m_aButtons[ BUTTON_RESUME ] );

#ifndef TOSHI_SKU_PSP
	// Pause the game
	ARootTask::GetSingleton()->SetPaused( TTRUE );

	APositionalSoundSet::GetSingleton()->SetPaused( TTRUE );
#endif

	AGameStateController::ms_bPausedByMenu = TTRUE;
	m_oDialog.Hide();
	m_bFocusedElementBloated = TFALSE;

#ifndef TOSHI_SKU_PSP
	if ( !AGameLoader::IsMainGameLoaded() || AGameStateController::GetSingleton()->FindGameStateOnStack( TPS8( AGopherShopGame ) ) )
	{
		AMusicManager::GetSingleton()->SetEnabled( TFALSE, TTRUE );
	}
#endif

	ABYardMenuState::OnInsertion();
#ifdef TOSHI_SKU_PSP
	m_oRootElement.AddChildTail( m_oTitle );
#else
	AGUI2::GetRootElement()->AddChildTail( m_oTitle );

	if ( AGUI2ModelRendererScreen::GetSingleton() )
		AGUI2ModelRendererScreen::GetSingleton()->SetEnabled( TFALSE );
#endif
}

// $Barnyard: FUNCTION 0042f8c0
void AStartMenuState::OnRemoval()
{
#ifndef TOSHI_SKU_PSP
	ARootTask::GetSingleton()->SetPaused( TFALSE );

	// Don't resume the sounds if the game is being left
	TUINT16 uiFlags  = AGameStateController::GetSingleton()->GetFlags();
	TBOOL   bLeaving = ( uiFlags & ( AGameStateController::FLAGS_IGNORE_INPUT | AGameStateController::FLAGS_LEAVE_IMMEDIATELY ) ) || ( uiFlags & AGameStateController::FLAGS_UNK10 );

	if ( !bLeaving )
	{
		APositionalSoundSet::GetSingleton()->SetPaused( TFALSE );
	}
#endif

	AGameStateController::ms_bPausedByMenu = TFALSE;

	for ( TINT i = 0; i < BUTTON_NUMOF; i++ )
	{
#ifdef TOSHI_SKU_PSP
		if ( i != BUTTON_RESTART && i != BUTTON_HELP )
#else
		if ( AGameLoader::IsInMiniGame() || i != BUTTON_RESTART )
#endif
			m_aButtons[ i ].RemoveSelf();
	}

#ifndef TOSHI_SKU_PSP
	if ( !AGameLoader::IsMainGameLoaded() || AGameStateController::GetSingleton()->FindGameStateOnStack( TPS8( AGopherShopGame ) ) )
	{
		AMusicManager::GetSingleton()->SetEnabled( TTRUE, TTRUE );
	}

	if ( !bLeaving && AGameStateController::GetSingleton()->FindGameStateOnStack( TPS8( ABarnExploreMinigameState ) ) )
	{
		AMultiplexStream::GetSingleton()->Pause( TFALSE );
	}
#endif

	m_oTitle.RemoveSelf();

#ifndef TOSHI_SKU_PSP
	if ( AGUI2ModelRendererScreen::GetSingleton() )
		AGUI2ModelRendererScreen::GetSingleton()->SetEnabled( TTRUE );
#endif

	ABYardMenuState::OnRemoval();
}

// $Barnyard: FUNCTION 0042efe0
void AStartMenuState::OnActivate()
{
	m_iActivatedButton = -1;
	ABYardMenuState::OnActivate();
	m_eMenuState = MENUSTATE_MENU_APPEAR;
	m_oDialogTitleBackground.Hide();
#ifdef TOSHI_SKU_PSP
	ASetGameRuntimePhase_PSP( "sim-paused" );
#endif
}

// $Barnyard: FUNCTION 0042f050
void AStartMenuState::OnMenuItemActivated( AGUI2MenuItem* a_pMenuItem )
{
	AGUI2Button* pButton = TSTATICCAST( AGUI2Button, a_pMenuItem );

	m_iActivatedButton = pButton->GetId();
	OnButtonActivated( pButton );
	m_eMenuState = MENUSTATE_MENU_VISIBLE;
}

// $Barnyard: FUNCTION 0042fa40
void AStartMenuState::OnMenuClose()
{
	AGameStateController* pController = AGameStateController::GetSingleton();

	switch ( m_iActivatedButton )
	{
		case -1:
		case BUTTON_RESUME:
			Remove();
			break;
		case BUTTON_OPTIONS:
			pController->PushState( new AOptionsState() );
			break;
#ifndef TOSHI_SKU_PSP
		case BUTTON_RESTART:
			// Fade out and restart the minigame (see AGameStateController::OnUpdate)
			pController->AddFlags( AGameStateController::FLAGS_LEAVE_IMMEDIATELY | AGameStateController::FLAGS_KEEP_OVERLAY_ALPHA | AGameStateController::FLAGS_UNK10 | AGameStateController::FLAGS_RESTART_MINIGAME );
			pController->SetOverlayGoal( 0.5f );
			ASoundManager::GetSingleton()->SetAllCuesVolume( 0.0f );
			Remove();
			pController->ShowOverlay( TTRUE, TFALSE, 0.5f, TTRUE );
			break;
		case BUTTON_HELP:
			if ( m_bHasHint )
			{
				ShowHint();
			}
			else
			{
				// Instructions of the minigame
				AMiniGameManager* pMiniGameManager = AMiniGameManager::GetSingleton();
				AMiniGame*        pMiniGame        = pMiniGameManager->GetNextMiniGame();

				AMessagePopupState* pPopup = new AMessagePopupState(
				    g_pLocaleManager->GetString( 295 ),
				    g_pLocaleManager->GetString( pMiniGame->GetVariant( pMiniGameManager->GetMiniGameVariant() ).eLocaleDescription ),
				    TPString8(),
				    TFALSE,
				    AInputCommand_OK,
				    AInputCommand_Empty,
				    AInputCommand_Empty,
				    AInputCommand_Empty,
				    TFALSE,
				    0.0f,
				    0
				);

				m_oHelpListener.Connect( pPopup->GetResponseEmitter(), this, OnHelpPopupResponse, 0 );
				pController->PushState( pPopup );
			}

			break;
#endif
		case BUTTON_QUIT:
		{
			// Ask for a confirmation
#ifdef TOSHI_SKU_PSP
			const TINT iMessage = 321;
			const TBOOL bRemoveOnResponse = TTRUE;
#else
			TINT iMessage;

			if ( AGameLoader::IsMainGameLoaded() || AGameLoader::IsBarnExploreMiniGame() )
				iMessage = 321;
			else
				iMessage = ( ms_aButtonTexts[ BUTTON_QUIT ] != 333 ) ? 322 : 320;
			const TBOOL bRemoveOnResponse = TFALSE;
#endif

			AMessagePopupState* pPopup = new AMessagePopupState(
			    g_pLocaleManager->GetString( 1064 ),
			    g_pLocaleManager->GetString( iMessage ),
			    TPString8(),
			    AInputCommand_No,
			    bRemoveOnResponse,
			    AInputCommand_Yes,
			    AInputCommand_No,
			    AInputCommand_Empty,
			    AInputCommand_Empty,
			    TFALSE,
			    60.0f,
			    0
			);

			pPopup->SetUnk19b0( TTRUE );
			pPopup->SetUnk19b8( 0.18f );

			m_oQuitListener.Connect( pPopup->GetResponseEmitter(), this, OnQuitPopupResponse, 0 );
			pController->PushState( pPopup );
			break;
		}
	}
}

#ifndef TOSHI_SKU_PSP
// $Barnyard: FUNCTION 0042f380
void AStartMenuState::ShowHint()
{
	// The summary of the last assigned quest
	AQuestManager* pQuestManager = AQuestManager::GetSingleton();
	TINT           iHint         = pQuestManager->GetAssignedQuest( pQuestManager->GetNumAssignedQuests() - 1 )->GetQuest()->GetSummaryId();

	const TWCHAR* wszTitle = g_pLocaleManager->GetString( 295 );
	const TWCHAR* wszHint  = ( iHint >= 0 ) ? g_pLocaleManager->GetString( iHint ) : L"Place Holder Quest Requirement";

	AMessagePopupState* pPopup = new AMessagePopupState(
	    wszTitle,
	    wszHint,
	    TPString8(),
	    TFALSE,
	    AInputCommand_OK,
	    AInputCommand_Empty,
	    AInputCommand_Empty,
	    AInputCommand_Empty,
	    TFALSE,
	    0.0f,
	    0
	);

	m_oHelpListener.Connect( pPopup->GetResponseEmitter(), this, OnHelpPopupResponse, 0 );
	AGameStateController::GetSingleton()->PushState( pPopup );
}

// $Barnyard: FUNCTION 0042f1c0
TBOOL AStartMenuState::OnQuitResponse( AMessagePopupState* a_pPopup, AMessagePopupState::ResponseEvent* a_pEvent )
{
	if ( m_pQuitCallbackUserData && m_fnQuitCallback )
		return m_fnQuitCallback( this, m_pQuitCallbackUserData, a_pPopup, a_pEvent );

	AGameStateController* pController = AGameStateController::GetSingleton();

	if ( a_pEvent->eButton == AInputCommand_Yes )
	{
		if ( AGameLoader::IsInMiniGame() == TTRUE )
		{
			// AMiniGameManager::OnUpdate leaves the minigame
			pController->AddFlags( AGameStateController::FLAGS_KEEP_OVERLAY_ALPHA | AGameStateController::FLAGS_UNK10 );
			pController->SetOverlayGoal( 0.5f );
			AMiniGameManager::GetSingleton()->RequestQuit();
		}
		else
		{
			// AGameStateController::OnUpdate returns to the frontend
			pController->AddFlags( AGameStateController::FLAGS_LEAVE_IMMEDIATELY | AGameStateController::FLAGS_KEEP_OVERLAY_ALPHA | AGameStateController::FLAGS_UNK10 );
			pController->SetOverlayGoal( 0.5f );
		}

		ASoundManager::GetSingleton()->SetAllCuesVolume( 0.0f );
		pController->PopCurrentGameState();
		ms_bQuitConfirmed = TTRUE;

		return TTRUE;
	}

	pController->PopCurrentGameState();
	return TFALSE;
}

// $Barnyard: FUNCTION 0042f310
TBOOL __stdcall AStartMenuState::OnQuitPopupResponse( AStartMenuState* a_pStartMenu, AMessagePopupState* a_pPopup, AMessagePopupState::ResponseEvent* a_pEvent )
{
	ms_bQuitConfirmed = TFALSE;
	TBOOL bResult     = a_pStartMenu->OnQuitResponse( a_pPopup, a_pEvent );

	if ( ms_bQuitConfirmed )
	{
		AGameStateController::GetSingleton()->ShowOverlay( TTRUE, TFALSE, 0.5f, TFALSE );
		a_pStartMenu->Remove();
	}

	return bResult;
}

// $Barnyard: FUNCTION 0042f360
TBOOL __stdcall AStartMenuState::OnHelpPopupResponse( AStartMenuState* a_pStartMenu, AMessagePopupState* a_pPopup, AMessagePopupState::ResponseEvent* a_pEvent )
{
	if ( a_pEvent->eButton != AInputCommand_OK )
		return TFALSE;

	AGameStateController::GetSingleton()->PopCurrentGameState();
	return TTRUE;
}
#else
TBOOL __stdcall AStartMenuState::OnQuitPopupResponse( AStartMenuState* a_pStartMenu, AMessagePopupState*, AMessagePopupState::ResponseEvent* a_pEvent )
{
	// Leave the story after the popup has finished dispatching its response.
	a_pStartMenu->m_bQuitConfirmed_PSP = a_pEvent->eButton == AInputCommand_Yes;
	return TTRUE;
}
#endif
