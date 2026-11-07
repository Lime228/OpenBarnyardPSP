#include "pch.h"

#include "AppBoot.h"
#include "CVar/AVarType.h"
#include "Locale/ALocale.h"
#include "Locale/ALocaleManager.h"
#include "Memory/AMemory.h"
#include "Tasks/ARootTask.h"

#include <Plugins/PPropertyParser/PProperties.h>
#include <Plugins/PPropertyParser/PProperty.h>
#include <Toshi/TPString8.h>
#include <Toshi/TScheduler.h>

TOSHI_NAMESPACE_USING

MODAPI AApplication g_oTheApp;

static TBOOL CreateOptionalSystems_PSP()
{
	TPString8Pool* pStringPool = new TPString8Pool(
	    1024,
	    0,
	    AMemory::GetAllocator( AMemory::POOL_StringPool ),
	    TNULL
	);
	if ( pStringPool == TNULL )
		return TFALSE;

	TUtil::SetTPStringPool( pStringPool );
	AVarType::Create( pStringPool );
	return TTRUE;
}

static TBOOL CreateLocaleManager_PSP()
{
	T2Locale::CreateSingleton<ALocaleManager>();
	return ALocaleManager::Interface() != TNULL;
}

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

	if ( !CreateOptionalSystems_PSP() || !CreateLocaleManager_PSP() )
		return TFALSE;

	PProperties::SetupFreeList( 100, 0 );
	PProperty::SetupFreeList( 100, 0 );
	TPooledString8::SetupFreeList( 0, 0 );

	ALocaleManager::Interface()->SetLanguage( ALocaleManager::Lang_EnglishUK );
	ALocaleUnknownData_SetUnk1( 0 );

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
