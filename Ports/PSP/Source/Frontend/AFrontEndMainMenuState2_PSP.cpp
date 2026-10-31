#include "AFrontEndMainMenuState2_PSP.h"
#include "../Input/AInputHandler_PSP.h"

#include <Platform/PSP/T2GUIFont_PSP.h>

static const TWCHAR* const s_apMenuItemLabels_PSP[ AFrontEndMainMenuState2_PSP::MenuItem_Count ] = {
	L"START PROBE",
	L"PORT STATUS",
};

AFrontEndMainMenuState2_PSP::AFrontEndMainMenuState2_PSP()
    : m_uiSelectedItem( MenuItem_Probe )
{
}

AFrontendState_PSP AFrontEndMainMenuState2_PSP::OnUpdate(
    AFrontendBootstrapState_PSP& a_rState,
    const AInputHandler_PSP& a_rInputHandler,
    TFLOAT a_fDeltaTime
)
{
	(void)a_rState;
	(void)a_fDeltaTime;

	if ( a_rInputHandler.IsActionPressed( AInputAction_PSP_Up ) )
		m_uiSelectedItem = ( m_uiSelectedItem + MenuItem_Count - 1 ) % MenuItem_Count;
	if ( a_rInputHandler.IsActionPressed( AInputAction_PSP_Down ) )
		m_uiSelectedItem = ( m_uiSelectedItem + 1 ) % MenuItem_Count;

	if ( !a_rInputHandler.IsActionPressed( AInputAction_PSP_Confirm ) )
		return AFrontendState_PSP_Menu;

	return m_uiSelectedItem == MenuItem_Probe
	    ? AFrontendState_PSP_Probe
	    : AFrontendState_PSP_Info;
}

TUINT32 AFrontEndMainMenuState2_PSP::GetClearColor( const AFrontendBootstrapState_PSP& a_rState ) const
{
	(void)a_rState;
	return 0xff302010;
}

void AFrontEndMainMenuState2_PSP::OnRender(
    const AFrontendBootstrapState_PSP& a_rState,
    const AFrontendRenderContext_PSP& a_rContext
)
{
	(void)a_rState;
	auto* pRenderInterface = a_rContext.pRenderInterface;
	const auto& oFont = *a_rContext.pFont;

	static const TWCHAR s_wszTitle[] = L"OPENBARNYARD";

	pRenderInterface->DrawColourRectangle( 44.0f, 18.0f, 392.0f, 40.0f, 0xff181008 );

	const TFLOAT fTitleScale = 3.0f;
	const TFLOAT fTitleX = ( Toshi::TRenderInterface_PSP::SCREEN_WIDTH - oFont.GetTextWidth( s_wszTitle, fTitleScale ) ) * 0.5f;
	oFont.DrawTextSingleLine( s_wszTitle, fTitleX + 2.0f, 29.0f, 0xff000000, fTitleScale );
	oFont.DrawTextSingleLine( s_wszTitle, fTitleX, 27.0f, 0xff5ac8fa, fTitleScale );

	if ( a_rContext.pTexture != TNULL )
		pRenderInterface->DrawTextureSprite( *a_rContext.pTexture, 176.0f, 72.0f, 128.0f, 128.0f );

	const TFLOAT fItemScale = 2.0f;
	for ( TUINT i = 0; i < MenuItem_Count; i++ )
	{
		const TFLOAT fItemY = 216.0f + i * 26.0f;
		const TBOOL bSelected = i == m_uiSelectedItem;
		const TWCHAR* pLabel = s_apMenuItemLabels_PSP[ i ];
		const TFLOAT fItemX = ( Toshi::TRenderInterface_PSP::SCREEN_WIDTH - oFont.GetTextWidth( pLabel, fItemScale ) ) * 0.5f;

		pRenderInterface->DrawColourRectangle( 124.0f, fItemY - 4.0f, 232.0f, 22.0f, bSelected ? 0xff302010 : 0xff181008 );
		if ( bSelected )
		{
			oFont.DrawTextSingleLine( L"-", fItemX - 18.0f, fItemY + 1.0f, 0xfff0cf65, fItemScale );
			oFont.DrawTextSingleLine( L"-", fItemX + oFont.GetTextWidth( pLabel, fItemScale ) + 8.0f, fItemY + 1.0f, 0xfff0cf65, fItemScale );
		}

		oFont.DrawTextSingleLine( pLabel, fItemX + 1.0f, fItemY + 1.0f, 0xff000000, fItemScale );
		oFont.DrawTextSingleLine( pLabel, fItemX, fItemY, bSelected ? 0xfff0cf65 : 0xffc0c0c0, fItemScale );
	}
}
