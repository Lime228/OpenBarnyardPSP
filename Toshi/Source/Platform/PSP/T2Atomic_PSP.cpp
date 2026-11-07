#include "ToshiPCH.h"
#include "T2Atomic_PSP.h"

#include <pspthreadman.h>

TOSHI_NAMESPACE_START

TINT32 T2Atomic32::Load( MemoryOrders a_eOrder ) const
{
	return __atomic_load_n( &m_iValue, a_eOrder == MEMORY_ORDER_STRONG ? __ATOMIC_SEQ_CST : __ATOMIC_RELAXED );
}

TINT32 T2Atomic32::Add( TINT32 a_iValue )
{
	return __atomic_fetch_add( &m_iValue, a_iValue, __ATOMIC_SEQ_CST );
}

TINT32 T2Atomic32::Or( TINT32 a_iMask )
{
	return __atomic_fetch_or( &m_iValue, a_iMask, __ATOMIC_SEQ_CST );
}

TINT32 T2Atomic32::And( TINT32 a_iMask )
{
	return __atomic_fetch_and( &m_iValue, a_iMask, __ATOMIC_SEQ_CST );
}

TINT32 T2Atomic32::Exchange( TINT32 a_iValue )
{
	return __atomic_exchange_n( &m_iValue, a_iValue, __ATOMIC_SEQ_CST );
}

TINT32 T2Atomic32::CompareExchange( TINT32 a_iValue, TINT32 a_iCompare )
{
	__atomic_compare_exchange_n( &m_iValue, &a_iCompare, a_iValue, TFALSE, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST );
	return a_iCompare;
}

void T2Atomic32::WaitForChange( TINT32 a_iValue )
{
	while ( Load( MEMORY_ORDER_WEAK ) == a_iValue )
		sceKernelDelayThread( 100 );
}

void T2Atomic32::WaitForChange()
{
	WaitForChange( Load( MEMORY_ORDER_WEAK ) );
}

void T2Atomic32::Signal()
{
	__atomic_thread_fence( __ATOMIC_SEQ_CST );
}

TOSHI_NAMESPACE_END
