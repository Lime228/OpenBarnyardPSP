#include "pch.h"

#include "GameInterface/AGameStateController.h"
#include "GameInterface/AStartMenuState.h"
#include "GUI/AGUI2.h"
#include "ASimState_PSP.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TDEFINE_CLASS( AGameStateController );

AGameStateController::AGameStateController()
    : m_UnkVector( m_VectorData, TARRAYSIZE( m_VectorData ), 0 )
{
	m_eFlagsRaw      = FLAGS_NONE;
	m_flOverlayAlpha = 0.0f;
	m_flSoundVolume  = 1.0f;
	m_flOverlayGoal  = 0.0f;
}

AGameStateController::~AGameStateController()
{
	TASSERT( m_oStates.Size() == 0 );
}

TBOOL AGameStateController::OnCreate()
{
	InsertGameState( new AGameState );

	TFLOAT fWidth;
	TFLOAT fHeight;
	AGUI2::GetSingleton()->GetDimensions( fWidth, fHeight );
	m_oOverlay.SetDimensions( fWidth, fHeight );
	m_oOverlay.SetAttachment( T2GUIATTACHMENT_BOTTOMCENTER, T2GUIATTACHMENT_BOTTOMCENTER );
	m_oOverlay.SetInFront();
	m_oOverlay.Hide();
	return TTRUE;
}

TBOOL AGameStateController::OnUpdate( TFLOAT a_fDeltaTime )
{
	if ( m_oStates.Size() > 1 )
	{
		if ( UpdateTransition( a_fDeltaTime ) )
			return TTRUE;

		if ( !HASANYFLAG( m_eFlagsRaw, FLAGS_IGNORE_INPUT | FLAGS_AWAITING_TRANSITION | FLAGS_FADING_IN ) )
			( *m_oStates.Back() )->OnUpdate( a_fDeltaTime );
	}

	UpdateScreenOverlay();
	return TTRUE;
}

void AGameStateController::OnDestroy()
{
	ResetStack();
	if ( m_oStates.Size() == 1 )
	{
		AGameState* pState = *m_oStates.Back();
		m_oStates.PopBack();
		pState->Destroy();
	}
}

void AGameStateController::InsertGameState( AGameState* a_pGameState )
{
	TGlobalEmitter<AGameStateControllerEvent>::Throw(
	    AGameStateControllerEvent( a_pGameState, AGameStateControllerEvent::Type_GameStateBeingAdded )
	);
	m_oStates.PushBack( a_pGameState );
	a_pGameState->OnInsertion();
	a_pGameState->Activate();
}

void AGameStateController::ReplaceState( AGameState* a_pGameState )
{
	if ( m_oStates.Size() > 1 )
	{
		AGameState* pCurrentState = *m_oStates.Back();
		m_oStates.PopBack();
		pCurrentState->Destroy();
	}
	InsertGameState( a_pGameState );
}

void AGameStateController::PushState( AGameState* a_pGameState )
{
	if ( m_oStates.Size() > 1 )
	{
		AGameState* pCurrentState = *m_oStates.Back();
		pCurrentState->OnDeactivate();
		TGlobalEmitter<AGameStateControllerEvent>::Throw(
		    AGameStateControllerEvent( pCurrentState, AGameStateControllerEvent::Type_GameStateDeactivated )
		);
		pCurrentState->OnSuspend();
	}
	InsertGameState( a_pGameState );
}

void AGameStateController::PopState( AGameState* a_pGameState )
{
	AGameState* pCurrentState = *m_oStates.Back();
	TASSERT( a_pGameState == pCurrentState );
	m_oStates.PopBack();
	pCurrentState->Destroy();

	if ( m_oStates.Size() > 1 )
	{
		AGameState* pResumedState = *m_oStates.Back();
		pResumedState->OnResume( a_pGameState );
		pResumedState->OnActivate();
		TGlobalEmitter<AGameStateControllerEvent>::Throw(
		    AGameStateControllerEvent( pResumedState, AGameStateControllerEvent::Type_GameStateActivated )
		);
	}
}

void AGameStateController::PopCurrentGameState()
{
	if ( m_oStates.Size() > 1 )
		PopState( *m_oStates.Back() );
}

void AGameStateController::ResetStack()
{
	UpdateScreenOverlay();
	m_eFlagsRaw = FLAGS_NONE;
	TBOOL bFirst = TTRUE;
	while ( m_oStates.Size() > 1 )
	{
		AGameState* pState = *m_oStates.Back();
		m_oStates.PopBack();
		pState->Destroy( bFirst );
		bFirst = TFALSE;
	}
}

void AGameStateController::ResetOverlays()
{
	for ( auto it = m_oStates.Begin(); it != m_oStates.End(); it++ )
		( *it )->SetOverlay( AGameState::OVERLAY_1 );
}

AGameState* AGameStateController::FindGameStateOnStack( const TPString8& a_rcClassName )
{
	for ( auto it = m_oStates.Begin(); it != m_oStates.End(); it++ )
	{
		if ( TPString8( ( *it )->GetClass()->GetName() ) == a_rcClassName )
			return *it;
	}
	return TNULL;
}

AGameState* AGameStateController::FindGameStateOnStack( const TClass* a_pClass )
{
	for ( auto it = m_oStates.Begin(); it != m_oStates.End(); it++ )
	{
		if ( ( *it )->IsA( a_pClass ) )
			return *it;
	}
	return TNULL;
}

void AGameStateController::AddChildStateByName( const TPString8& a_rcClassName )
{
	const TClass* pClass = TClass::Find( a_rcClassName.GetString(), &TGetClass( AGameState ) );
	if ( pClass != TNULL )
		GetCurrentState()->AddChildState( TSTATICCAST( AGameState, pClass->CreateObject() ) );
}

void AGameStateController::StartGameState( TPString8 )
{
}

void AGameStateController::FinishGameState()
{
}

void AGameStateController::CreateAndPushNamedState( TPString8 a_strClassName )
{
	const TClass* pClass = TClass::Find( a_strClassName.GetString8().GetString(), &TGetClass( AGameState ) );
	if ( pClass != TNULL )
		PushState( TSTATICCAST( AGameState, pClass->CreateObject() ) );
}

void AGameStateController::CreateAndTransferToNamedState( const TPString8& a_rcClassName )
{
	const TClass* pClass = TClass::Find( a_rcClassName.GetString8().GetString(), &TGetClass( AGameState ) );
	if ( pClass != TNULL )
		ReplaceState( TSTATICCAST( AGameState, pClass->CreateObject() ) );
}

void AGameStateController::ReturnToNamedState( TPString8 a_strClassName )
{
	AGameState* pState = FindGameStateOnStack( a_strClassName );
	while ( pState != TNULL && GetCurrentState() != pState )
		PopCurrentGameState();
}

TBOOL AGameStateController::IsStateOnStack( const TPString8& a_rcClassName )
{
	return FindGameStateOnStack( a_rcClassName ) != TNULL;
}

TBOOL AGameStateController::IsBusy()
{
	return HASANYFLAG( GetSingleton()->m_eFlagsRaw, FLAGS_AWAITING_TRANSITION | FLAGS_FADING_IN );
}

void AGameStateController::SetFlags( FLAGS a_eFlags )
{
	m_eFlagsRaw |= a_eFlags;
	m_flSoundVolume = 1.0f;
	if ( m_eFlagsRaw & FLAGS_KEEP_OVERLAY_ALPHA )
	{
		m_eFlagsRaw &= ~FLAGS_KEEP_OVERLAY_ALPHA;
		m_flOverlayAlpha = m_flOverlayGoal;
	}
	else
	{
		m_flOverlayGoal = 0.0f;
		m_flOverlayAlpha = a_eFlags == FLAGS_FADING_IN ? 1.0f : 0.0f;
	}
	UpdateOverlayAlpha();
	GetCurrentState()->SetOverlay( AGameState::OVERLAY_3 );
	UpdateScreenOverlay();
}

void AGameStateController::StartMenuIfPossible()
{
	if ( GetCurrentState() != TNULL && IsCurrentState( &TGetClass( ASimState_PSP ) ) )
		PushState( new AStartMenuState() );
}

void AGameStateController::SaveCurrentGameState( TBOOL, TBOOL, TBOOL )
{
}

#ifdef BARNYARD_LATEST
void AGameStateController::ShowOverlay( TBOOL a_bShow, TBOOL, TFLOAT a_fAlpha, TBOOL, const TCHAR* )
#else
void AGameStateController::ShowOverlay( TBOOL a_bShow, TBOOL, TFLOAT a_fAlpha, TBOOL )
#endif
{
	AGameState* pState = GetCurrentState();
	if ( pState == TNULL )
		return;

	if ( a_bShow )
	{
		pState->SetOverlay( AGameState::OVERLAY_3 );
		OverlayData oData = { TUINT8( a_fAlpha * 255.0f ), 0, 0, 0 };
		SetOverlayParams( AGameState::OVERLAY_3, oData );
	}
	else
	{
		pState->SetOverlay( AGameState::OVERLAY_1 );
	}
	UpdateScreenOverlay();
}

void AGameStateController::RecreateMiniGameState( const TPString8& a_rcClassName )
{
	ResetStack();
	CreateAndPushNamedState( a_rcClassName );
}

void AGameStateController::SetOverlayParams( AGameState::OVERLAY a_eOverlay, const OverlayData& a_rcParams )
{
	TASSERT( a_eOverlay < AGameState::OVERLAY_NUMOF );
	ms_aOverlays[ a_eOverlay ] = a_rcParams;
}

AGameStateController::OverlayData* AGameStateController::GetOverlayParams( AGameState::OVERLAY a_eOverlay )
{
	TASSERT( a_eOverlay < AGameState::OVERLAY_NUMOF );
	return &ms_aOverlays[ a_eOverlay ];
}

TBOOL AGameStateController::UpdateTransition( TFLOAT a_fDeltaTime )
{
	if ( m_eFlagsRaw & FLAGS_AWAITING_TRANSITION )
	{
		m_flOverlayAlpha += a_fDeltaTime * 2.0f;
		if ( m_flOverlayAlpha >= 1.0f )
		{
			m_flOverlayAlpha = 1.0f;
			m_eFlagsRaw &= ~( FLAGS_AWAITING_TRANSITION | FLAGS_FADE_SOUND );
		}
		UpdateOverlayAlpha();
		return TTRUE;
	}

	if ( m_eFlagsRaw & FLAGS_FADING_IN )
	{
		m_flOverlayAlpha -= a_fDeltaTime * 2.0f;
		if ( m_flOverlayAlpha <= 0.0f )
		{
			m_flOverlayAlpha = 0.0f;
			m_eFlagsRaw &= ~FLAGS_FADING_IN;
		}
		UpdateOverlayAlpha();
	}
	return TFALSE;
}

void AGameStateController::RenderStates( TFLOAT a_fDeltaTime )
{
	for ( auto it = m_oStates.Begin(); it != m_oStates.End(); it++ )
		( *it )->OnRender( a_fDeltaTime );
}

void AGameStateController::PostRenderStates( TFLOAT a_fDeltaTime )
{
	for ( auto it = m_oStates.Begin(); it != m_oStates.End(); it++ )
		( *it )->OnPostRender( a_fDeltaTime );
}

void AGameStateController::RenderStatesOverlay( TFLOAT a_fDeltaTime )
{
	for ( auto it = m_oStates.Begin(); it != m_oStates.End(); it++ )
		( *it )->OnRenderOverlay( a_fDeltaTime );
}

void AGameStateController::UpdateScreenOverlay()
{
	TBOOL bShow = TFALSE;
	if ( m_oStates.Size() > 1 && GetCurrentState()->GetOverlay() != AGameState::OVERLAY_1 )
	{
		OverlayData* pOverlay = GetOverlayParams( GetCurrentState()->GetOverlay() );
		m_oOverlay.SetColour( TCOLOR4( pOverlay->uiColorR, pOverlay->uiColorG, pOverlay->uiColorB, pOverlay->uiColorA ) );
		bShow = TTRUE;
	}
	m_oOverlay.RemoveSelf();
	if ( bShow )
	{
		AGUI2::GetRootElement()->AddChildTail( m_oOverlay );
		m_oOverlay.Show();
	}
	else
	{
		m_oOverlay.Hide();
	}
}

TBOOL AGameStateController::ProcessInput( const TInputInterface::InputEvent* a_pEvent )
{
	if ( m_eFlags.bIgnoreInput || m_oStates.Size() <= 1 )
		return m_eFlags.bIgnoreInput;

	AGameState* pState = GetCurrentState();
	if ( pState->SendInputCommands( a_pEvent ) )
		return TTRUE;
	return pState->ProcessInput( a_pEvent );
}
