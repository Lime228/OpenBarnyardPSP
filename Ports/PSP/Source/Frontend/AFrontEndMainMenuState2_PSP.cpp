#include "AFrontEndMainMenuState2_PSP.h"
#include "../Input/AInputHandler_PSP.h"

#include <Platform/PSP/T2GUIFont_PSP.h>

AFrontendState_PSP AFrontEndMainMenuState2_PSP::OnUpdate(
    AFrontendBootstrapState_PSP& a_rState,
    const AInputHandler_PSP& a_rInputHandler,
    TFLOAT a_fDeltaTime
)
{
	(void)a_rState;
	(void)a_fDeltaTime;
	return a_rInputHandler.IsActionPressed( AInputAction_PSP_Confirm )
	    ? AFrontendState_PSP_Probe
	    : AFrontendState_PSP_Menu;
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
	Toshi::T2GUIFont_PSP oFont( pRenderInterface );

	static const TWCHAR s_wszTitle[]  = L"OPENBARNYARD";
	static const TWCHAR s_wszPrompt[] = L"CROSS - START";

	pRenderInterface->DrawColourRectangle( 44.0f, 18.0f, 392.0f, 40.0f, 0xff181008 );
	pRenderInterface->DrawColourRectangle( 124.0f, 222.0f, 232.0f, 28.0f, 0xff181008 );

	const TFLOAT fTitleScale = 3.0f;
	const TFLOAT fTitleX = ( Toshi::TRenderInterface_PSP::SCREEN_WIDTH - oFont.GetTextWidth( s_wszTitle, fTitleScale ) ) * 0.5f;
	oFont.DrawTextSingleLine( s_wszTitle, fTitleX + 2.0f, 29.0f, 0xff000000, fTitleScale );
	oFont.DrawTextSingleLine( s_wszTitle, fTitleX, 27.0f, 0xff5ac8fa, fTitleScale );

	if ( a_rContext.pTexture != TNULL )
		pRenderInterface->DrawTextureSprite( *a_rContext.pTexture, 176.0f, 72.0f, 128.0f, 128.0f );

	const TFLOAT fPromptScale = 2.0f;
	const TFLOAT fPromptX = ( Toshi::TRenderInterface_PSP::SCREEN_WIDTH - oFont.GetTextWidth( s_wszPrompt, fPromptScale ) ) * 0.5f;
	oFont.DrawTextSingleLine( s_wszPrompt, fPromptX + 1.0f, 230.0f, 0xff000000, fPromptScale );
	oFont.DrawTextSingleLine( s_wszPrompt, fPromptX, 229.0f, 0xfff0cf65, fPromptScale );
}
