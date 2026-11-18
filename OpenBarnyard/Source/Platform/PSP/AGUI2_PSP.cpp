#include "pch.h"

#include "GUI/AGUI2.h"
#include "Mod/AModEvents.h"
#include "Render/ABackend.h"

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
	T2GUI::CreateContext()->SetRenderer( new AGUI2Renderer_PSP );
	m_pRootElement = T2GUI::GetContext()->GetRootElement();
	if ( m_pRootElement == TNULL )
		return TFALSE;

	m_pRootElement->SetDimensions( 480.0f, 272.0f );
	m_pRootElement->SetPostRenderCallback( MainPostRenderCallback );
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
