#include "pch.h"

#include "AppBoot.h"
#include "CVar/AVarType.h"
#include "Locale/ALocale.h"
#include "Locale/ALocaleManager.h"
#include "Memory/AMemory.h"
#include "Tasks/ARootTask.h"

#include <Core/TMemory.h>
#include <Toshi/TFreeList.h>
#include <Toshi/TPString8.h>
#include <Toshi/TScheduler.h>
#include <Toshi/TSystem.h>
#include <Toshi/TUtil.h>

#include <pspkernel.h>

PSP_MODULE_INFO( "OpenBarnyard PSP", PSP_MODULE_USER, 0, 1 );
PSP_MAIN_THREAD_ATTR( PSP_THREAD_ATTR_USER );
PSP_HEAP_SIZE_KB( -1024 );

TOSHI_NAMESPACE_USING

static constexpr TSIZE s_uiGameHeapSize_PSP = 20 * 1024 * 1024;

static TINT ExitCallback_PSP( TINT, TINT, void* )
{
	if ( g_oTheApp.IsCreated() )
		g_oTheApp.Destroy();

	return 0;
}

static TINT CallbackThread_PSP( SceSize, void* )
{
	const TINT iCallback = sceKernelCreateCallback( "OpenBarnyardExit", ExitCallback_PSP, TNULL );
	if ( iCallback >= 0 )
		sceKernelRegisterExitCallback( iCallback );

	sceKernelSleepThreadCB();
	return 0;
}

static void SetupCallbacks_PSP()
{
	const TINT iThread = sceKernelCreateThread(
	    "OpenBarnyardCallbacks",
	    CallbackThread_PSP,
	    0x11,
	    0x1000,
	    PSP_THREAD_ATTR_USER,
	    TNULL
	);

	if ( iThread >= 0 )
		sceKernelStartThread( iThread, 0, TNULL );
}

static TBOOL CreateEngine_PSP()
{
	if ( !TMemory::Initialise( s_uiGameHeapSize_PSP, 0 ) )
		return TFALSE;

	TUtil::TOSHIParams oToshiParams;
	oToshiParams.szCommandLine = "";
	oToshiParams.szLogFileName = "barnyard_psp";

	if ( !TUtil::ToshiCreate( oToshiParams ) )
	{
		TMemory::Deinitialise();
		return TFALSE;
	}

	AMemory::CreatePool( AMemory::POOL_StringPool );
	return AMemory::GetMemBlock( AMemory::POOL_StringPool ) != TNULL;
}

static void DestroyLocaleManager_PSP()
{
	if ( !T2Locale::IsSingletonCreated() )
		return;

	void* pBuffer = ALocaleManager::Interface()->GetBuffer();
	ALocaleManager::DestroySingleton();
	TFree( pBuffer );
}

static void DestroyEngine_PSP()
{
	if ( ARootTask::IsSingletonCreated() )
		ARootTask::GetSingleton()->Shutdown();

	TScheduler* pScheduler = g_oSystemManager.GetScheduler();
	if ( pScheduler != TNULL )
		pScheduler->DestroyAllTasks();

	g_oLocaleUnknownData.Clear();
	DestroyLocaleManager_PSP();
	AVarType::Destroy();

	if ( TPString8Pool* pStringPool = TUtil::GetTPStringPool() )
		delete pStringPool;

	TUtil::SetTPStringPool( TNULL );
	TFreeList::DeinitialiseAll();
	AMemory::DestroyPool( AMemory::POOL_StringPool );
	TUtil::ToshiDestroy();
	TMemory::Deinitialise();
}

int main( TINT, TCHAR** )
{
	SetupCallbacks_PSP();

	if ( !CreateEngine_PSP() )
	{
		sceKernelExitGame();
		return 1;
	}

	TCHAR  szExecutable[] = "OpenBarnyardPSP";
	TCHAR* apArguments[]  = { szExecutable };

	TINT iResult = 1;
	if ( g_oTheApp.Create( "The Barnyard - PSP", 1, apArguments ) )
	{
		g_oTheApp.Execute();
		iResult = 0;
	}

	DestroyEngine_PSP();
	sceKernelExitGame();
	return iResult;
}
