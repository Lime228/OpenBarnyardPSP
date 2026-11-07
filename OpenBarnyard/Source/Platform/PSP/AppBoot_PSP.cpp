#include "pch.h"

#include "AppBoot.h"
#include "Tasks/ARootTask.h"

#include <Toshi/TScheduler.h>

TOSHI_NAMESPACE_USING

MODAPI AApplication g_oTheApp;

static TBOOL CreateDummyTask_PSP( TScheduler* a_pScheduler, ADummyTask*& a_rTask, const TCHAR* a_szName )
{
	a_rTask = a_pScheduler->CreateTask<ADummyTask>();
	if ( a_rTask == TNULL || !a_rTask->Create() )
		return TFALSE;

	a_rTask->Activate( TTRUE );
	a_rTask->SetName( a_szName );
	return TTRUE;
}

AApplication::~AApplication() = default;

TBOOL AApplication::OnCreate( TINT a_iArgCount, TCHAR** a_ppArgValues )
{
	TINFO( "Starting Barnyard PSP...\n" );

	auto pScheduler = g_oSystemManager.GetScheduler();
	if ( pScheduler == TNULL )
		return TFALSE;

	if ( !CreateDummyTask_PSP( pScheduler, m_pRenderTask, "RenderTask" ) ||
	     !CreateDummyTask_PSP( pScheduler, m_pUpdate3Task, "Update3" ) ||
	     !CreateDummyTask_PSP( pScheduler, m_pUpdate2Task, "Update2" ) ||
	     !CreateDummyTask_PSP( pScheduler, m_pUpdate1Task, "Update1" ) ||
	     !CreateDummyTask_PSP( pScheduler, m_pInputTask, "InputTask" ) )
	{
		pScheduler->DestroyAllTasks();
		return TFALSE;
	}

	m_pRootTask = pScheduler->CreateTask<ARootTask>();
	if ( m_pRootTask == TNULL || !m_pRootTask->Create() )
		return TFALSE;

	m_pRootTask->Activate( TTRUE );
	return TApplication::OnCreate( a_iArgCount, a_ppArgValues );
}
