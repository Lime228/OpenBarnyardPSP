#include "pch.h"

#include "GUI/AGUISystem.h"
#include "AGameRuntimeReport_PSP.h"

//-----------------------------------------------------------------------------
// Enables memory debugging.
// Note: Should be the last include!
//-----------------------------------------------------------------------------
#include <Core/TMemoryDebugOn.h>

TOSHI_NAMESPACE_USING

TDEFINE_CLASS( AGUISystem );

TPSTRING8_DECLARE( Start );
TPSTRING8_DECLARE( ParkManager );

AGUISystem::AGUISystem()
{
	m_Unk1          = 0;
	m_Unk2          = 0;
	m_Unk3          = 0;
	m_pStrings      = new TPString8[ 2 ];
	m_pStrings[ 0 ] = TPS8( ParkManager );
	m_pStrings[ 1 ] = TPS8( Start );

	for ( TUINT i = 0; i < NUM_RENDER_OBJECTS; i++ )
		m_pRenderObjects[ i ] = TNULL;

#ifdef BARNYARD_COMMUNITY_PATCH
	m_pTGUIDisplayContext = TNULL;
	m_pTGUIScreen         = TNULL;
	m_pTGUISoundFactory   = TNULL;
	m_pTGUIAudioContext   = TNULL;
	m_pTGUIInputContext   = TNULL;
	m_bTGUIMaterialsDirty = TFALSE;
	m_bTGUIMatLibLoaded   = TFALSE;
	m_bTGUIPointerMode    = TFALSE;
	m_iNumOwnerDraws      = 0;
	for ( TUINT i = 0; i < NUM_RENDER_OBJECTS; i++ )
		m_apTGUIViewportHandlers[ i ] = TNULL;
#  ifndef TOSHI_FINAL
	m_pTGUIDemo = TNULL;
#  endif
#endif
}

AGUISystem::~AGUISystem()
{
	delete[] m_pStrings;
}

TBOOL AGUISystem::OnCreate()
{
	m_RenderGUIListener.Connect(
		ARenderer::GetSingleton()->GetRenderGUIEmitter(),
		this,
		OnGUIRender,
		0xffff8000
	);
	AGetGameRuntimeReport_PSP().bGUISystemCreated = TTRUE;
	ASetGameRuntimePhase_PSP( "gui-system-created" );
	return TTRUE;
}

void AGUISystem::OnDestroy()
{
	m_MatLibPicture.Flush();
	m_RenderGUIListener.Disconnect();
	m_SomeListener.Disconnect();
}

TBOOL AGUISystem::OnGUIRender( AGUISystem* a_pGUISystem, ARenderer*, TINT* a_pPass )
{
	if ( *a_pPass == 0 )
		a_pGUISystem->m_MatLibPicture.Render();
	return TTRUE;
}

void AGUISystem::AllowBackgroundClear( TBOOL )
{
}

void AGUISystem::SetPicture( const TPString8& a_rName )
{
	const TBOOL bHasName = !a_rName.IsEmpty() && a_rName.GetString8().Length() != 0;
	if ( bHasName && a_rName != m_MatLibPicture.GetName() )
	{
		m_MatLibPicture.Create( a_rName );
		m_MatLibPicture.Cache();
		m_MatLibPicture.SetVisible();
	}
	else if ( !bHasName )
	{
		m_MatLibPicture.Flush();
		m_MatLibPicture.Create( a_rName );
		m_MatLibPicture.SetHidden();
	}
}

void AGUISystem::ResetPicture()
{
	m_MatLibPicture.Flush();
	m_MatLibPicture.Create( TPString8() );
}
