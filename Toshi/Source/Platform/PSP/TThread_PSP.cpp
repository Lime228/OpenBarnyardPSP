#include "ToshiPCH.h"
#include "TThread_PSP.h"

#include <pspthreadman.h>

TOSHI_NAMESPACE_START

static TINT TThreadPriorityToPSP( TThread::PRIORITY a_ePriority )
{
	TINT iPriority = 0x20 - a_ePriority;
	if ( iPriority < 0x08 ) iPriority = 0x08;
	if ( iPriority > 0x77 ) iPriority = 0x77;
	return iPriority;
}

static TThread::PRIORITY TThreadPriorityFromPSP( TINT a_iPriority )
{
	return 0x20 - a_iPriority;
}

static TINT TThreadEntry_PSP( SceSize, void* a_pUserData )
{
	TThread* pThread = static_cast<TThread*>( a_pUserData );
	pThread->Main();
	TThread::Exit( pThread );
	return 0;
}

static TThreadManager s_oThreadManager_PSP;

TBOOL TThread::Create( TSIZE a_uiStackSize, PRIORITY a_ePriority, FLAGS a_eFlags )
{
	if ( m_iThreadID >= 0 )
		return TFALSE;

	const TSIZE uiStackSize = a_uiStackSize != 0 ? a_uiStackSize : 0x4000;
	m_iThreadID = sceKernelCreateThread(
	    "ToshiThread",
	    TThreadEntry_PSP,
	    TThreadPriorityToPSP( a_ePriority ),
	    uiStackSize,
	    PSP_THREAD_ATTR_USER,
	    TNULL
	);

	if ( m_iThreadID < 0 )
		return TFALSE;

	m_hThreadHnd = reinterpret_cast<void*>( TUINTPTR( m_iThreadID ) );
	TThreadManager::GetSingletonSafe()->InsertThread( this );

	if ( ( a_eFlags & FLAGS_SUSPENDED ) == 0 )
	{
		const TINT iResult = sceKernelStartThread( m_iThreadID, 0, TNULL );
		if ( iResult < 0 )
		{
			Destroy();
			return TFALSE;
		}
	}

	return TTRUE;
}

TBOOL TThread::Destroy()
{
	if ( m_iThreadID < 0 || m_iThreadID == sceKernelGetThreadId() )
		return TFALSE;

	TINT iResult = sceKernelTerminateDeleteThread( m_iThreadID );
	if ( iResult < 0 )
		iResult = sceKernelDeleteThread( m_iThreadID );

	if ( iResult < 0 )
		return TFALSE;

	TThreadManager::GetSingletonSafe()->RemoveThread( this );
	m_hThreadHnd = TNULL;
	m_iThreadID  = -1;
	return TTRUE;
}

TBOOL TThread::GetPriority( void* a_pThreadHandle, PRIORITY& a_ePriority )
{
	if ( a_pThreadHandle == TNULL )
		return TFALSE;

	SceKernelThreadInfo oInfo = {};
	oInfo.size = sizeof( oInfo );
	const TINT iThreadID = TINT( reinterpret_cast<TUINTPTR>( a_pThreadHandle ) );
	if ( sceKernelReferThreadStatus( iThreadID, &oInfo ) < 0 )
		return TFALSE;

	a_ePriority = TThreadPriorityFromPSP( oInfo.currentPriority );
	return TTRUE;
}

TBOOL TThread::SetPriority( void* a_pThreadHandle, PRIORITY a_ePriority )
{
	if ( a_pThreadHandle == TNULL )
		return TFALSE;

	const TINT iThreadID = TINT( reinterpret_cast<TUINTPTR>( a_pThreadHandle ) );
	return sceKernelChangeThreadPriority( iThreadID, TThreadPriorityToPSP( a_ePriority ) ) >= 0;
}

void TThread::Exit( TThread* a_pThread )
{
	TThreadManager::GetSingletonSafe()->RemoveThread( a_pThread );
	a_pThread->m_hThreadHnd = TNULL;
	a_pThread->m_iThreadID  = -1;
	sceKernelExitDeleteThread( 0 );
}

TThreadManager::TThreadManager()
{
	Create();
}

TThreadManager::~TThreadManager()
{
	Delete();
}

void TThreadManager::Create()
{
	m_Mutex.Create();
}

void TThreadManager::Delete()
{
	m_Mutex.Lock();
	m_Threads.Clear();
	m_Mutex.Unlock();
	m_Mutex.Destroy();
}

void TThreadManager::RemoveThread( TThread* a_pThread )
{
	m_Mutex.Lock();
	if ( a_pThread->IsLinked() )
		a_pThread->Remove();
	m_Mutex.Unlock();
}

void TThreadManager::InsertThread( TThread* a_pThread )
{
	m_Mutex.Lock();
	m_Threads.PushFront( a_pThread );
	m_Mutex.Unlock();
}

TOSHI_NAMESPACE_END
