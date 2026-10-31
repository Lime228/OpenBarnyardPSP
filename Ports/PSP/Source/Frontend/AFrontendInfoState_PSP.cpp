#include "AFrontendInfoState_PSP.h"
#include "AFrontendLocale_PSP.h"
#include "../Input/AInputHandler_PSP.h"

#include <Platform/PSP/T2GUIFont_PSP.h>

AFrontendState_PSP AFrontendInfoState_PSP::OnUpdate(
    AFrontendBootstrapState_PSP& a_rState,
    const AInputHandler_PSP& a_rInputHandler,
    TFLOAT a_fDeltaTime
)
{
	(void)a_rState;
	(void)a_fDeltaTime;
	return a_rInputHandler.IsActionPressed( AInputAction_PSP_Cancel )
	    ? AFrontendState_PSP_Menu
	    : AFrontendState_PSP_Info;
}

TUINT32 AFrontendInfoState_PSP::GetClearColor( const AFrontendBootstrapState_PSP& a_rState ) const
{
	(void)a_rState;
	return 0xff201830;
}

void AFrontendInfoState_PSP::OnRender(
    const AFrontendBootstrapState_PSP& a_rState,
    const AFrontendRenderContext_PSP& a_rContext
)
{
	(void)a_rState;
	auto* pRenderInterface = a_rContext.pRenderInterface;
	const auto& oFont = *a_rContext.pFont;

	const TWCHAR* pTitle = a_rContext.pLocale->GetString( AFrontendLocaleString_PSP_StatusTitle );
	const TWCHAR* pCore = a_rContext.pLocale->GetString( AFrontendLocaleString_PSP_CoreReady );
	const TWCHAR* pRender = a_rContext.pLocale->GetString( AFrontendLocaleString_PSP_RenderReady );
	const TWCHAR* pBack = a_rContext.pLocale->GetString( AFrontendLocaleString_PSP_Back );

	pRenderInterface->DrawColourRectangle( 52.0f, 30.0f, 376.0f, 212.0f, 0xff181008 );

	const TFLOAT fTitleScale = 3.0f;
	const TFLOAT fTitleX = ( Toshi::TRenderInterface_PSP::SCREEN_WIDTH - oFont.GetTextWidth( pTitle, fTitleScale ) ) * 0.5f;
	oFont.DrawTextSingleLine( pTitle, fTitleX, 50.0f, 0xff5ac8fa, fTitleScale );

	const TFLOAT fLineScale = 2.0f;
	oFont.DrawTextSingleLine( pCore, 150.0f, 116.0f, 0xff65d26e, fLineScale );
	oFont.DrawTextSingleLine( pRender, 138.0f, 148.0f, 0xff65d26e, fLineScale );

	const TFLOAT fBackX = ( Toshi::TRenderInterface_PSP::SCREEN_WIDTH - oFont.GetTextWidth( pBack, fLineScale ) ) * 0.5f;
	oFont.DrawTextSingleLine( pBack, fBackX, 210.0f, 0xfff0cf65, fLineScale );
}
