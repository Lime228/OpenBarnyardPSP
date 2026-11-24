#include "pch.h"
#include "AFrontEndNewGameSelectState.h"
#include "AGameStateController.h"
#include "GUI/T2GUIFontManager.h"
#include "Locale/ALocaleManager.h"
#include "Sound/ASoundManager.h"
#include "SoundBank/ui.h"

#ifdef BARNYARD_PSP_FRONTEND_SLICE
#  include "AFrontEndMainMenuState2.h"
#  include "Platform/PSP/AGameRuntimeReport_PSP.h"
#  include "Platform/PSP/ARenderer_PSP.h"
#  include "Units/ASSeqFile.h"
#else
#  include "AHackedQuickSave.h"
#  include "Player/APlayerProfileManager.h"
#  include "Helpers/AGameLoader.h"
#  include "Helpers/AHighScoreManager.h"
#  include "Helpers/AHintManager.h"
#  include "Mod/AModEvents.h"

#  include <GUI/T2GUITextureSectionManager.h>
#  include <Toshi/TDateTime.h>
#endif

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

#ifdef BARNYARD_PSP_FRONTEND_SLICE
static void APauseMenuMusicForOpening_PSP( TBOOL a_bPause )
{
	AGameState* pState = AGameStateController::GetSingleton()->FindGameStateOnStack( TPString8( "AFrontEndMainMenuState2" ) );
	if ( pState != TNULL )
		TSTATICCAST( AFrontEndMainMenuState2, pState )->PauseBackgroundMusic_PSP( a_bPause );
}
#endif

// NOTE: GetClass 0040c140
TDEFINE_CLASS( AFrontEndNewGameSelectState );

#ifndef BARNYARD_PSP_FRONTEND_SLICE
void AFrontEnd_ReadSaveSlot( const APlayerProgress::SaveSlot* a_pSlot, TWCHAR* a_wszOutName, TWCHAR* a_wszOutDate, TINT& a_rOutGender, TINT& a_rOutCostume )
{
	TDateTime oDateTime;
	oDateTime.UnPack( a_pSlot->uiDate, a_pSlot->uiTime );

	const TINT iYear   = oDateTime.uiYear;
	const TINT iMonth  = oDateTime.uiMonth;
	const TINT iDay    = oDateTime.uiDay;
	const TINT iHour   = oDateTime.uiHour;
	const TINT iMinute = oDateTime.uiMinute;

	// The profile buffer of the quick save starts with the gender, the costume and the name of the player
	const TUINT8* pProfile = a_pSlot->aQuickSave + AHackedQuickSave::GetBufferDataOffset( AHackedQuickSave::BUFFER_PROFILE );
	a_rOutGender           = AHackedQuickSave::ReadInt( pProfile );
	a_rOutCostume          = AHackedQuickSave::ReadInt( pProfile + 4 );

	a_wszOutName[ 0 ] = L'\0';
	AHackedQuickSave::ReadWString( pProfile + 8, a_wszOutName, 32 );

	const TINT iHour12 = ( iHour == 0 ) ? 12 : ( ( iHour > 12 ) ? iHour - 12 : iHour );

	switch ( ALocaleManager::Interface()->GetLanguage() )
	{
		case ALocaleManager::Lang_English:
			// month/day/year
			TStringManager::String16Format( a_wszOutDate, 32, L"  (%d/%d/%02d %2d:%02d", iMonth + 1, iDay, iYear % 100, iHour12, iMinute );
			break;
		case ALocaleManager::Lang_Japanese:
			// year/month/day
			TStringManager::String16Format( a_wszOutDate, 32, L"  (%02d/%d/%d %2d:%02d", iYear % 100, iMonth + 1, iDay, iHour12, iMinute );
			break;
		case ALocaleManager::Lang_German:
		case ALocaleManager::Lang_Italian:
		case ALocaleManager::Lang_Spanish:
		case ALocaleManager::Lang_French:
			// day/month/year, 24 hours
			TStringManager::String16Format( a_wszOutDate, 32, L"  (%d/%d/%02d %2d:%02d)", iDay, iMonth + 1, iYear % 100, iHour, iMinute );
			return;
		default:
			// day/month/year
			TStringManager::String16Format( a_wszOutDate, 32, L"  (%d/%d/%02d %2d:%02d", iDay, iMonth + 1, iYear % 100, iHour12, iMinute );
			break;
	}

	// AM / PM
	TINT iLength = TStringManager::String16Length( a_wszOutDate );
	TStringManager::String16Format( a_wszOutDate + iLength, 32 - iLength, L"%s)", ALocaleManager::Interface()->GetString( ( iHour > 11 ) ? 1175 : 1176 ) );
}
#endif

// $Barnyard: FUNCTION 0040c260
AFrontEndNewGameSelectState::AFrontEndNewGameSelectState()
    : m_iActivatedSlot( -1 )
    , m_bOverwrite( TFALSE )
{
}

// $Barnyard: FUNCTION 0040c150
// $Barnyard: FUNCTION 0040c320
AFrontEndNewGameSelectState::~AFrontEndNewGameSelectState()
{
}

// NOTE: shares 00407aa0 with AOptionsState::ProcessCommand
TBOOL AFrontEndNewGameSelectState::ProcessCommand( AInputCommand a_eInputCommand, const TInputInterface::InputEvent* a_pInputEvent, TBOOL& a_rStopEvents )
{
#ifdef BARNYARD_PSP_FRONTEND_SLICE
	if ( m_bOpeningPlaying_PSP )
	{
		a_rStopEvents = TTRUE;
		if ( a_eInputCommand == AInputCommand_Cancel )
		{
			m_bOpeningPlaying_PSP = TFALSE;
			AStopSSeqOpening_PSP( TTRUE );
			APauseMenuMusicForOpening_PSP( TFALSE );
			ASetGameRuntimePhase_PSP( "frontend-new-game-return" );
			Remove();
		}
		return TTRUE;
	}
#endif
	if ( m_eMenuState == MENUSTATE_MENU_VISIBLE && a_eInputCommand == AInputCommand_Cancel )
		ASoundManager::GetSingleton()->PlayCue( soundbank::UI_MENUBACK );

	return BaseClass::ProcessCommand( a_eInputCommand, a_pInputEvent, a_rStopEvents );
}

#ifdef BARNYARD_PSP_FRONTEND_SLICE
TBOOL AFrontEndNewGameSelectState::OnUpdate( TFLOAT a_fDeltaTime )
{
	if ( m_bOpeningPlaying_PSP )
		return TTRUE;
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.iMenuState = m_eMenuState;
	AGUI2MenuItem* pFocusedItem = m_oMenu.GetFocusedMenuItem();
	rReport.iMenuFocusID = pFocusedItem != TNULL ? TSTATICCAST( AGUI2Button, pFocusedItem )->GetId() : -1;
	return BaseClass::OnUpdate( a_fDeltaTime );
}
#endif

// $Barnyard: FUNCTION 0040c340
void AFrontEndNewGameSelectState::OnInsertion()
{
	m_bFocusedElementBloated = TFALSE;

	T2GUIFont* pFont     = T2GUIFontManager::FindFont( "Rekord26" );
	T2GUIFont* pDateFont = T2GUIFontManager::FindFont( "Rekord18" );
	TFLOAT     fMaxText  = 0.0f;
	TFLOAT     fMaxImage = 0.0f;

#ifdef BARNYARD_PSP_FRONTEND_SLICE
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.iNewGameRequestedSlot = -1;
	rReport.iNewGameScriptTracks = 0;
	rReport.uiNewGameTerrainMeshes = 0;
	rReport.uiNewGameTerrainTextures = 0;
	rReport.uiNewGameTerrainBytes = 0;
	rReport.uiNewGameTerrainHeapFree = 0;
	rReport.bNewGameTerrainReleased = TFALSE;
	rReport.uiNewGameOpeningDraws = 0;
	rReport.uiNewGameOpeningFrames = 0;
#else
	APlayerProgress* pProgress = APlayerProgress::GetSingleton();
#endif

	for ( TINT i = 0; i < APlayerProgress::NUM_SAVE_SLOTS; i++ )
	{
#ifdef BARNYARD_PSP_FRONTEND_SLICE
		const TWCHAR* wszText;
#else
		APlayerProgress::SaveSlot* pSlot   = pProgress->GetSlot( i );
		const TWCHAR*              wszText = m_awszSlotNames[ i ];

		if ( pSlot->bOccupied == 1 )
		{
			TINT iGender, iCostume;
			AFrontEnd_ReadSaveSlot( pSlot, m_awszSlotNames[ i ], m_awszSlotDates[ i ], iGender, iCostume );

			// The player character of the slot, left of the button
			const TCHAR* szPicture = APlayerProfileManager::GetSingleton()->GetMainProfile()->GetHideTextureName( iCostume, iGender );
			m_aSlotPictures[ i ].SetTextureSection( *T2GUITextureSectionManager::GetTextureSection( szPicture ) );
			m_aSlotPictures[ i ].SetAttachment( T2GUIATTACHMENT_MIDDLELEFT, T2GUIATTACHMENT_MIDDLERIGHT );
			m_aSlotPictures[ i ].SetTransform( 0.0f, pDateFont->GetLineHeight() * -0.5f );
			m_aSlotButtons[ i ].AddChildTail( m_aSlotPictures[ i ] );

			fMaxImage = TMath::Max( fMaxImage, m_aSlotPictures[ i ].GetWidth() );
		}
		else
#endif
		{
			// Empty
			wszText                   = ALocaleManager::Interface()->GetString( 1283 );
			m_awszSlotDates[ i ][ 0 ] = L'\0';
		}

		fMaxText = TMath::Max( fMaxText, pFont->GetTextWidth( wszText ) );

		TFLOAT fGap = TMath::Max( ( m_aSlotPictures[ i ].GetHeight() - pFont->GetLineHeight() ) * 0.25f, 0.0f ) + 4.0f;

		AGUI2Button& rButton = m_aSlotButtons[ i ];
		rButton.Create( pFont, pFont->GetTextWidth( wszText ), wszText, fGap );
		rButton.SetFocusedColour( 0xffffffff );
		rButton.SetUnkColour( 0xffffc800 );
		rButton.SetEnabledColour( 0xffb56f22 );
		rButton.SetDisabledColour( 0xff4b4b4b );
		rButton.SetFocusedScale( 1.0f );
		rButton.SetScale( 0.8f );
		rButton.SetId( i );
		rButton.SetScaleOnFocus( TTRUE );
		rButton.GetTextBox().SetTextAlign( T2GUIFont::TextAlign_Left );
		rButton.SetTextShadow( TTRUE, 0xa0000000, 0.6f );
		rButton.SetTextShadowOffset( rButton.GetTextBox().GetScale() * 5.0f );

		// The save date, below the name
		AGUI2TextBox& rDate = m_aSlotDateTexts[ i ];
		rDate.Create( pDateFont, pDateFont->GetTextWidth( m_awszSlotDates[ i ] ) );
		rDate.SetText( m_awszSlotDates[ i ] );
		rDate.SetScale( rButton.GetTextBox().GetScale() );
		rDate.SetShadow( TTRUE, 0xa0000000 );
		rDate.SetShadowAlpha( 0.6f );
		rDate.SetShadowOffset( rButton.GetTextBox().GetScale() * 5.0f, rButton.GetTextBox().GetScale() * 5.0f );
		rDate.SetAttachment( T2GUIATTACHMENT_BOTTOMCENTER, T2GUIATTACHMENT_TOPCENTER );
		rDate.SetTransform( 0.0f, 8.0f - pDateFont->GetLineHeight() * 0.5f );
		rButton.GetTextBox().AddChildTail( rDate );

		rButton.OnFocusLost();
		m_oMenu.AddMenuItem( rButton );
	}

	m_oMenu.SetFocusAt( m_aSlotButtons[ 0 ] );
	m_oMenu.SetFocus( TFALSE );
	m_oDialogTitle.SetText( ALocaleManager::Interface()->GetString( 11 ) );

	// Make the dialog wide enough for the pictures
	AGUI2TextBox& rFirstText = m_aSlotButtons[ 0 ].GetTextBox();
	TFLOAT        fWidth     = rFirstText.GetWidth();
	rFirstText.SetWidth( fMaxImage * 0.5f + fMaxText );

	BaseClass::OnInsertion();

	rFirstText.SetWidth( fWidth );
#ifdef BARNYARD_PSP_FRONTEND_SLICE
	ASetGameRuntimePhase_PSP( "frontend-new-game-ready" );
#endif
}

// $Barnyard: FUNCTION 0040c200
void AFrontEndNewGameSelectState::OnRemoval()
{
#ifdef BARNYARD_PSP_FRONTEND_SLICE
	if ( m_bOpeningPlaying_PSP )
		AStopSSeqOpening_PSP( TFALSE );
#endif
	for ( TINT i = 0; i < APlayerProgress::NUM_SAVE_SLOTS; i++ )
	{
		m_aSlotPictures[ i ].RemoveSelf();
		m_aSlotButtons[ i ].RemoveSelf();
	}

	BaseClass::OnRemoval();
}

// $Barnyard: FUNCTION 0040c250
void AFrontEndNewGameSelectState::OnActivate()
{
	m_iActivatedSlot = -1;
	BaseClass::OnActivate();
}

// $Barnyard: FUNCTION 0040c960
void AFrontEndNewGameSelectState::OnMenuItemActivated( AGUI2MenuItem* a_pMenuItem )
{
	AGUI2Button* pButton = TSTATICCAST( AGUI2Button, a_pMenuItem );
	m_iActivatedSlot     = pButton->GetId();

#ifndef BARNYARD_PSP_FRONTEND_SLICE
	AHighScoreManager::ScoreReset( TFALSE );
	AHintManager::ResetState();

	APlayerProgress* pProgress = APlayerProgress::GetSingleton();
	m_bOverwrite               = pProgress->GetSlot( m_iActivatedSlot )->bOccupied == 1;
	pProgress->SetCurrentSlotIndex( m_iActivatedSlot );
#endif

	OnButtonActivated( pButton );
}

// $Barnyard: FUNCTION 0040ca40
void AFrontEndNewGameSelectState::OnMenuClose()
{
	if ( m_iActivatedSlot == -1 )
	{
#ifdef BARNYARD_PSP_FRONTEND_SLICE
		ASetGameRuntimePhase_PSP( "frontend-new-game-return" );
#endif
		Remove();
		return;
	}

#ifdef BARNYARD_PSP_FRONTEND_SLICE
	static constexpr TCHAR s_szIntroScript_PSP[] = "local:Data/ScriptedSequence/SS01_start.tso";
	AGameRuntimeReport_PSP& rReport = AGetGameRuntimeReport_PSP();
	rReport.iNewGameRequestedSlot = m_iActivatedSlot;
	TUINT8* pScript = ASSeqFile::Read( s_szIntroScript_PSP, rReport.iNewGameScriptTracks );
	const TBOOL bScriptLoaded = pScript != TNULL;
	const TCHAR* szPhase = "frontend-new-game-script-failed";
	if ( bScriptLoaded )
	{
		TFree( pScript );
		ASetGameRuntimeFailedPath_PSP( TNULL );
		if ( AHasSSeqOpening_PSP() )
		{
			ASetGameRuntimePhase_PSP( "frontend-new-game-opening-loading" );
			if ( AStartSSeqOpening_PSP() )
			{
				m_bOpeningPlaying_PSP = TTRUE;
				APauseMenuMusicForOpening_PSP( TTRUE );
				ASetGameRuntimePhase_PSP( "frontend-new-game-opening" );
				return;
			}
			OnActivate();
			ASetGameRuntimePhase_PSP( "frontend-new-game-opening-failed" );
			return;
		}
		ASetGameRuntimePhase_PSP( "frontend-new-game-loading-terrain" );
		szPhase = ACheckNewGameTerrain_PSP() ? "frontend-new-game-awaiting-sequence" : "frontend-new-game-terrain-failed";
	}
	else
		ASetGameRuntimeFailedPath_PSP( s_szIntroScript_PSP );

	// Keep the chooser active until the intro scene and actor controller are ready.
	OnActivate();
	ASetGameRuntimePhase_PSP( szPhase );
#else
	if ( m_bOverwrite == TTRUE )
	{
		// The slot is in use, ask before overwriting it
		AMessagePopupState* pPopup = new AMessagePopupState(
		    ALocaleManager::Interface()->GetString( 1064 ),
		    ALocaleManager::Interface()->GetString( 289 ),
		    TPString8(),
		    AInputCommand_No,
		    TFALSE,
		    AInputCommand_Yes,
		    AInputCommand_No,
		    AInputCommand_Empty,
		    AInputCommand_Empty,
		    TFALSE,
		    60.0f,
		    0
		);

		pPopup->SetUnk19b0( TTRUE );
		pPopup->SetUnk19b4( 0.15f );
		pPopup->SetUnk19b8( 0.18f );

		m_oPopupListener.Connect( pPopup->GetResponseEmitter(), this, OnOverwritePopupResponse, 0 );
		AGameStateController::GetSingleton()->PushState( pPopup );
		return;
	}

	StartNewGame();
#endif
}

#ifndef BARNYARD_PSP_FRONTEND_SLICE
// NOTE: inlined in OnMenuClose and OnOverwritePopupResponse
void AFrontEndNewGameSelectState::StartNewGame()
{
	APlayerProfileManager::GetSingleton()->SetMainGender( APlayerProfile::GENDER_MALE );
	AGameLoader::ResetSimLoadingPictures();
	FIRE_EVENT( AGameLoader::NewGame );

	// Once the screen has faded out, AGameStateController starts the SS01 intro sequence (AGameLoader::StartNewGame)
	AGameStateController* pController = AGameStateController::GetSingleton();
	pController->AddFlags( AGameStateController::FLAGS_START_NEW_GAME );
	pController->SetFlags( AGameStateController::FLAGS_AWAITING_TRANSITION | AGameStateController::FLAGS_FADE_SOUND );
}

#ifdef BARNYARD_COMMUNITY_PATCH

// NOTE: OnMenuItemActivated, OnMenuClose and OnOverwritePopupResponse in one
TBOOL AFrontEndNewGameSelectState::StartNewGameInSlot( TINT a_iSlot, TBOOL a_bOverwrite )
{
	APlayerProgress* pProgress = APlayerProgress::GetSingleton();
	const TBOOL      bInUse    = pProgress->GetSlot( a_iSlot )->bOccupied == 1;

	if ( bInUse && !a_bOverwrite )
		return TFALSE;

	AHighScoreManager::ScoreReset( TFALSE );
	AHintManager::ResetState();
	pProgress->SetCurrentSlotIndex( a_iSlot );

	APlayerProfileManager::GetSingleton()->SetMainGender( APlayerProfile::GENDER_MALE );
	AGameLoader::ResetSimLoadingPictures();
	FIRE_EVENT( AGameLoader::NewGame );

	if ( bInUse )
		pProgress->ClearCurrentSlot();

	AGameStateController* pController = AGameStateController::GetSingleton();
	pController->AddFlags( AGameStateController::FLAGS_START_NEW_GAME );
	pController->SetFlags( AGameStateController::FLAGS_AWAITING_TRANSITION | AGameStateController::FLAGS_FADE_SOUND );
	return TTRUE;
}

#endif // BARNYARD_COMMUNITY_PATCH

// $Barnyard: FUNCTION 0040ca20
// $Barnyard: FUNCTION 0040c9c0
TBOOL __stdcall AFrontEndNewGameSelectState::OnOverwritePopupResponse( AFrontEndNewGameSelectState* a_pState, AMessagePopupState* a_pPopup, AMessagePopupState::ResponseEvent* a_pEvent )
{
	a_pState->m_oPopupListener.Disconnect();

	if ( a_pEvent->eButton == AInputCommand_Yes )
	{
		APlayerProfileManager::GetSingleton()->SetMainGender( APlayerProfile::GENDER_MALE );
		AGameLoader::ResetSimLoadingPictures();
		FIRE_EVENT( AGameLoader::NewGame );
		APlayerProgress::GetSingleton()->ClearCurrentSlot();

		AGameStateController* pController = AGameStateController::GetSingleton();
		pController->AddFlags( AGameStateController::FLAGS_START_NEW_GAME );
		pController->SetFlags( AGameStateController::FLAGS_AWAITING_TRANSITION | AGameStateController::FLAGS_FADE_SOUND );
	}
	else
	{
		a_pPopup->Remove();
	}

	return TFALSE;
}
#endif
