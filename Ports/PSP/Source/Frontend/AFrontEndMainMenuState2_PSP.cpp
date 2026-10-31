#include "AFrontEndMainMenuState2_PSP.h"
#include "../Input/AInputHandler_PSP.h"

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
	if ( a_rContext.pTexture != TNULL )
		a_rContext.pRenderInterface->DrawTextureSprite( *a_rContext.pTexture, 176.0f, 72.0f, 128.0f, 128.0f );
}
