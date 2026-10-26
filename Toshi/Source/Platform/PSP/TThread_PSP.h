#pragma once

#include "Toshi/Defines.h"
#include "Toshi/T2DList.h"
#include "Toshi/TSingleton.h"
#include "Thread/TMutex.h"

TOSHI_NAMESPACE_START

class TThread : public T2DList<TThread>::Node
{
public:
	using PRIORITY = TINT;
	enum PRIORITY_ : PRIORITY
	{
		THREAD_MODE_BACKGROUND_BEGIN  = 0x00010000,
		THREAD_MODE_BACKGROUND_END    = 0x00020000,
		THREAD_PRIORITY_ABOVE_NORMAL  = 1,
		THREAD_PRIORITY_BELOW_NORMAL  = -1,
		THREAD_PRIORITY_HIGHEST       = 2,
		THREAD_PRIORITY_IDLE          = -15,
		THREAD_PRIORITY_LOWEST        = -2,
		THREAD_PRIORITY_NORMAL        = 0,
		THREAD_PRIORITY_TIME_CRITICAL = 15
	};

	using FLAGS = TUINT8;
	enum FLAGS_ : FLAGS
	{
		FLAGS_NONE      = 0,
		FLAGS_SUSPENDED = BITFLAG( 0 ),
	};

public:
	virtual void Main() = 0;
	virtual ~TThread() = default;

	TBOOL Create( TSIZE a_uiStackSize, PRIORITY a_ePriority, FLAGS a_eFlags );
	TBOOL Destroy();

	static TBOOL GetPriority( void* a_pThreadHandle, PRIORITY& a_ePriority );
	static TBOOL SetPriority( void* a_pThreadHandle, PRIORITY a_ePriority );
	static void  Exit( TThread* a_pThread );

public:
	void*         m_hThreadHnd = TNULL;
	volatile TINT m_iThreadID  = -1;
};

class TThreadManager : public TSingleton<TThreadManager>
{
public:
	TThreadManager();
	~TThreadManager();

	void Create();
	void Delete();
	void RemoveThread( TThread* a_pThread );
	void InsertThread( TThread* a_pThread );

	friend class TThread;

private:
	TMutex          m_Mutex;
	T2DList<TThread> m_Threads;
};

TOSHI_NAMESPACE_END
