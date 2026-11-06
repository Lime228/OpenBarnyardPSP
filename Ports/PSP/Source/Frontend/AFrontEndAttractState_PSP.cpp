#include "AFrontEndAttractState_PSP.h"
#include "AFrontendLocale_PSP.h"
#include "../GUI/AGUIAudioContext_PSP.h"
#include "../Input/AInputHandler_PSP.h"

#include <Platform/PSP/T2GUIFont_PSP.h>

AFrontEndAttractState_PSP::AFrontEndAttractState_PSP()
    : m_fBlinkTime( 0.0f )
    , m_bShowPrompt( TTRUE )
{
}

void AFrontEndAttractState_PSP::OnInsertion( AFrontendBootstrapState_PSP& a_rState )
{
	(void)a_rState;
	m_fBlinkTime = 0.0f;
	m_bShowPrompt = TTRUE;
}

AFrontendState_PSP AFrontEndAttractState_PSP::OnUpdate(
    AFrontendBootstrapState_PSP& a_rState,
    const AInputHandler_PSP& a_rInputHandler,
    TFLOAT a_fDeltaTime
)
{
	m_fBlinkTime += a_fDeltaTime;
	if ( m_fBlinkTime >= 0.75f )
	{
		m_fBlinkTime = 0.0f;
		m_bShowPrompt = !m_bShowPrompt;
	}

	if ( !a_rInputHandler.IsActionPressed( AInputAction_PSP_Pause ) )
		return AFrontendState_PSP_Attract;

	if ( a_rState.pGUIAudioContext != TNULL )
		a_rState.pGUIAudioContext->PlaySound( ASoundId_PSP_UI_MENUOK );
	return AFrontendState_PSP_Menu;
}

TUINT32 AFrontEndAttractState_PSP::GetClearColor( const AFrontendBootstrapState_PSP& a_rState ) const
{
	(void)a_rState;
	return 0xff101c20;
}

void AFrontEndAttractState_PSP::OnRender(
    const AFrontendBootstrapState_PSP& a_rState,
    const AFrontendRenderContext_PSP& a_rContext
)
{
	(void)a_rState;
	if ( a_rContext.pFrontendLogo != TNULL )
	{
		a_rContext.pRenderInterface->DrawTextureRegion(
		    *a_rContext.pFrontendLogo,
		    0.0f,
		    0.0f,
		    TFLOAT( a_rContext.pFrontendLogo->GetWidth() ),
		    TFLOAT( a_rContext.pFrontendLogo->GetHeight() ),
		    48.0f,
		    44.0f,
		    384.0f,
		    96.0f,
		    0xffffffff
		);
	}

	if ( !m_bShowPrompt || a_rContext.pFont == TNULL || a_rContext.pLocale == TNULL )
		return;

	const TWCHAR* pPrompt = a_rContext.pLocale->GetString( AFrontendLocaleString_PSP_PressStart );
	const TFLOAT fScale = 2.0f;
	const TFLOAT fX = ( Toshi::TRenderInterface_PSP::SCREEN_WIDTH - a_rContext.pFont->GetTextWidth( pPrompt, fScale ) ) * 0.5f;
	a_rContext.pRenderInterface->DrawColourRectangle( 126.0f, 211.0f, 228.0f, 28.0f, 0xc0201008 );
	a_rContext.pFont->DrawTextSingleLine( pPrompt, fX + 2.0f, 218.0f, 0xff000000, fScale );
	a_rContext.pFont->DrawTextSingleLine( pPrompt, fX, 216.0f, 0xfff0cf65, fScale );
}
