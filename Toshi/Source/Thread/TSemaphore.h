#pragma once

#include "Toshi/Defines.h"

#if defined( TOSHI_SKU_WINDOWS ) && !defined( USE_ATOMIC )
#  define USE_WIN32_SEMAPHORE
#endif // TOSHI_SKU_WINDOWS && !USE_ATOMIC

#if !defined( USE_WIN32_SEMAPHORE ) && !defined( TOSHI_SKU_PSP )
#  include "T2AtomicMutex.h"
#endif // !USE_WIN32_SEMAPHORE && !TOSHI_SKU_PSP

TOSHI_NAMESPACE_START

class TSemaphore
{
public:
	TSemaphore() = default;
	~TSemaphore();

public:
	TBOOL Create( TINT a_iInitCount, TINT a_iMaxCount );
	TBOOL Destroy();
	TBOOL Signal();
	TBOOL Wait();
	TBOOL Poll();

private:
#if defined( TOSHI_SKU_PSP )
	TINT m_iSemaphore = -1;
#elif defined( USE_WIN32_SEMAPHORE )
	void* m_hSemaHnd;
#else  // USE_WIN32_SEMAPHORE
	T2Atomic32 m_iCounter;
#endif // !USE_WIN32_SEMAPHORE
};

TOSHI_NAMESPACE_END
