#include "AFrontEndMainMenuState2_PSP.h"
#include "AFrontendLocale_PSP.h"
#include "../GUI/AGUIAudioContext_PSP.h"
#include "../Input/AInputHandler_PSP.h"

#include <Platform/PSP/T2GUIFont_PSP.h>

static const AFrontendLocaleString_PSP s_aeMenuItemLabels_PSP[ AFrontEndMainMenuState2_PSP::FRONTENDBUTTON_NUMOF ] = {
	AFrontendLocaleString_PSP_NewGame,
	AFrontendLocaleString_PSP_LoadGame,
	AFrontendLocaleString_PSP_Options,
	AFrontendLocaleString_PSP_Bonuses,
	AFrontendLocaleString_PSP_Antics,
	AFrontendLocaleString_PSP_Quit,
};

static void DrawFrontendTerrain_PSP( const AFrontendRenderContext_PSP& a_rContext )
{
	if ( a_rContext.pTerrainModels == TNULL ||
	     a_rContext.pTerrainTextures == TNULL ||
	     a_rContext.uiTerrainModelCount == 0 ||
	     a_rContext.pMaterial == TNULL )
	{
		return;
	}

	const Toshi::TRenderSceneTransform_PSP oFrontendTerrainTransform = {
		{ 0.0f, -0.45f, -4.8f },
		{ 0.0f, 0.0f, 0.0f },
		0.85f,
	};
	for ( TUINT i = 0; i < a_rContext.uiTerrainModelCount; i++ )
	{
		a_rContext.pRenderInterface->DrawModel(
		    a_rContext.pTerrainModels[ i ],
		    oFrontendTerrainTransform,
		    *a_rContext.pMaterial,
		    &a_rContext.pTerrainTextures[ i ],
		    a_rContext.oTerrainModelTransform
		);
	}
}

AFrontEndMainMenuState2_PSP::AFrontEndMainMenuState2_PSP()
    : m_uiSelectedItem( FRONTENDBUTTON_NEWGAME )
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
	{
		m_uiSelectedItem = ( m_uiSelectedItem + FRONTENDBUTTON_NUMOF - 1 ) % FRONTENDBUTTON_NUMOF;
		if ( a_rState.pGUIAudioContext != TNULL )
			a_rState.pGUIAudioContext->PlaySound( ASoundId_PSP_UI_MENUCLICK );
	}
	if ( a_rInputHandler.IsActionPressed( AInputAction_PSP_Down ) )
	{
		m_uiSelectedItem = ( m_uiSelectedItem + 1 ) % FRONTENDBUTTON_NUMOF;
		if ( a_rState.pGUIAudioContext != TNULL )
			a_rState.pGUIAudioContext->PlaySound( ASoundId_PSP_UI_MENUCLICK );
	}

	if ( !a_rInputHandler.IsActionPressed( AInputAction_PSP_Confirm ) )
		return AFrontendState_PSP_Menu;
	if ( a_rState.pGUIAudioContext != TNULL )
		a_rState.pGUIAudioContext->PlaySound( ASoundId_PSP_UI_MENUOK );

	return AFrontendState_PSP_Menu;
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
	DrawFrontendTerrain_PSP( a_rContext );

	if ( a_rContext.pFrontendLogo != TNULL )
	{
		pRenderInterface->DrawTextureRegion(
		    *a_rContext.pFrontendLogo,
		    0.0f,
		    0.0f,
		    TFLOAT( a_rContext.pFrontendLogo->GetWidth() ),
		    TFLOAT( a_rContext.pFrontendLogo->GetHeight() ),
		    144.0f,
		    8.0f,
		    192.0f,
		    48.0f,
		    0xffffffff
		);
	}

	const TFLOAT fItemScale = 2.0f;
	for ( TUINT i = 0; i < FRONTENDBUTTON_NUMOF; i++ )
	{
		const TFLOAT fItemY = 76.0f + i * 30.0f;
		const TBOOL bSelected = i == m_uiSelectedItem;
		const TWCHAR* pLabel = a_rContext.pLocale->GetString( s_aeMenuItemLabels_PSP[ i ] );
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
