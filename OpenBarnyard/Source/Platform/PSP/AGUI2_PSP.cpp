#include "pch.h"

#include "GUI/AGUI2.h"
#include "Mod/AModEvents.h"
#include "Render/ABackend.h"
#include "AGameRuntimeReport_PSP.h"

#include <GUI/T2GUIFontManager.h>
#include <GUI/T2GUITextureSectionManager.h>

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TDEFINE_CLASS( AGUI2 );

AGUI2::AGUI2()
{
	m_pRootElement       = TNULL;
	m_bShowMouseCursor   = TFALSE;
	m_bShowMemStatsInfo  = TFALSE;
	m_bShowFPSInfo       = TFALSE;
	m_bShowPlayerInfo    = TFALSE;
	m_bShowTexturesInfo  = TFALSE;
}

AGUI2::~AGUI2() = default;

TBOOL AGUI2::OnCreate()
{
	ASetGameRuntimePhase_PSP( "gui2-context-creating" );
	T2GUI::CreateContext()->SetRenderer( new AGUI2Renderer_PSP );
	m_pRootElement = T2GUI::GetContext()->GetRootElement();
	if ( m_pRootElement == TNULL )
		return TFALSE;

	// Keep the original 800x600 GUI coordinate space. The PSP renderer maps it
	// to 480x272 just like the desktop renderer maps it to the active display.
	m_pRootElement->SetDimensions( 800.0f, 600.0f );
	m_pRootElement->SetPostRenderCallback( MainPostRenderCallback );
	ASetGameRuntimePhase_PSP( "gui2-fonts-opening" );
	if ( !T2GUIFontManager::Open( "Data/GUI/fonts.trb" ) )
		return TFALSE;
	ASetGameRuntimePhase_PSP( "gui2-sections-opening" );
	if ( !T2GUITextureSectionManager::Open( "Data/GUI/texsec.trb", TNULL ) )
	{
		T2GUIFontManager::Close( "Data/GUI/fonts.trb" );
		return TFALSE;
	}
	ASetGameRuntimePhase_PSP( "gui2-materials-creating" );
	T2GUITextureSectionManager::CreateMaterials();
	AGetGameRuntimeReport_PSP().bGUI2Created = TTRUE;
	ASetGameRuntimePhase_PSP( "gui2-created" );
	FIRE_EVENT( AGUI2::Created, this );
	return TTRUE;
}

TBOOL AGUI2::OnUpdate( TFLOAT a_fDeltaTime )
{
	if ( T2GUI::GetContext() != TNULL )
		T2GUI::GetContext()->Tick( a_fDeltaTime );
	return TTRUE;
}

void AGUI2::OnDestroy()
{
	T2GUITextureSectionManager::Close();
	T2GUIFontManager::Close( "Data/GUI/fonts.trb" );
	T2GUIRenderer* pRenderer = T2GUI::GetRenderer();
	if ( pRenderer != TNULL )
		delete pRenderer;
	T2GUI::DestroyContext();
	m_pRootElement = TNULL;
	TTask::OnDestroy();
}

void AGUI2::GetDimensions( TFLOAT& a_rWidth, TFLOAT& a_rHeight )
{
	if ( m_pRootElement != TNULL )
		m_pRootElement->GetDimensions( a_rWidth, a_rHeight );
	else
	{
		a_rWidth  = 480.0f;
		a_rHeight = 272.0f;
	}
}

void AGUI2::MainPostRenderCallback()
{
	FIRE_EVENT( AGUI2::PostRender );
}
