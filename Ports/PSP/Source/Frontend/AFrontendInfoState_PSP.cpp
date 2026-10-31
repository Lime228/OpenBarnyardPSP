#include "AFrontendInfoState_PSP.h"
#include "AFrontendLocale_PSP.h"
#include "../Input/AInputHandler_PSP.h"

#include <Platform/PSP/T2GUIFont_PSP.h>

static void FormatUnsignedLine_PSP(
    TWCHAR* a_pOutput,
    TUINT a_uiCapacity,
    const TWCHAR* a_pLabel,
    TUINT a_uiValue
)
{
	if ( a_pOutput == TNULL || a_uiCapacity == 0 )
		return;

	TUINT uiOutput = 0;
	while ( a_pLabel != TNULL && *a_pLabel != L'\0' && uiOutput + 1 < a_uiCapacity )
		a_pOutput[ uiOutput++ ] = *a_pLabel++;

	if ( uiOutput + 1 < a_uiCapacity )
		a_pOutput[ uiOutput++ ] = L' ';

	TWCHAR aDigits[ 10 ];
	TUINT uiDigits = 0;
	do
	{
		aDigits[ uiDigits++ ] = TWCHAR( L'0' + a_uiValue % 10 );
		a_uiValue /= 10;
	} while ( a_uiValue != 0 && uiDigits < TARRAYSIZE( aDigits ) );

	while ( uiDigits != 0 && uiOutput + 1 < a_uiCapacity )
		a_pOutput[ uiOutput++ ] = aDigits[ --uiDigits ];

	a_pOutput[ uiOutput ] = L'\0';
}

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
	auto* pRenderInterface = a_rContext.pRenderInterface;
	const auto& oFont = *a_rContext.pFont;

	const TWCHAR* pTitle = a_rContext.pLocale->GetString( AFrontendLocaleString_PSP_StatusTitle );
	const TWCHAR* pCore = a_rContext.pLocale->GetString( AFrontendLocaleString_PSP_CoreReady );
	const TWCHAR* pRender = a_rContext.pLocale->GetString( AFrontendLocaleString_PSP_RenderReady );
	const TWCHAR* pBack = a_rContext.pLocale->GetString( AFrontendLocaleString_PSP_Back );
	TWCHAR aFrameText[ 32 ];
	FormatUnsignedLine_PSP(
	    aFrameText,
	    TARRAYSIZE( aFrameText ),
	    a_rContext.pLocale->GetString( AFrontendLocaleString_PSP_Frames ),
	    a_rState.uiFrameCount
	);

	pRenderInterface->DrawColourRectangle( 52.0f, 30.0f, 376.0f, 212.0f, 0xff181008 );

	const TFLOAT fTitleScale = 3.0f;
	const TFLOAT fTitleX = ( Toshi::TRenderInterface_PSP::SCREEN_WIDTH - oFont.GetTextWidth( pTitle, fTitleScale ) ) * 0.5f;
	oFont.DrawTextSingleLine( pTitle, fTitleX, 50.0f, 0xff5ac8fa, fTitleScale );

	const TFLOAT fLineScale = 2.0f;
	oFont.DrawTextSingleLine( pCore, 150.0f, 116.0f, 0xff65d26e, fLineScale );
	oFont.DrawTextSingleLine( pRender, 138.0f, 148.0f, 0xff65d26e, fLineScale );
	oFont.DrawTextSingleLine( aFrameText, 162.0f, 180.0f, 0xffc0c0c0, fLineScale );

	const TFLOAT fBackX = ( Toshi::TRenderInterface_PSP::SCREEN_WIDTH - oFont.GetTextWidth( pBack, fLineScale ) ) * 0.5f;
	oFont.DrawTextSingleLine( pBack, fBackX, 210.0f, 0xfff0cf65, fLineScale );
}
