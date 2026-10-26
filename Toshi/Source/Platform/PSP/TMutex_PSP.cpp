#include "ToshiPCH.h"
#include "Thread/TMutex.h"

#include <pspthreadman.h>

TOSHI_NAMESPACE_START

TMutex::TMutex()
    : m_iSemaphore( -1 )
    , m_iOwnerThread( -1 )
    , m_uiLockDepth( 0 )
{
}

TMutex::~TMutex()
{
	Destroy();
}

TBOOL TMutex::Create()
{
	if ( m_iSemaphore >= 0 )
		return TTRUE;

	m_iSemaphore = sceKernelCreateSema( "ToshiMutex", 0, 1, 1, TNULL );
	return m_iSemaphore >= 0;
}

TBOOL TMutex::Destroy()
{
	if ( m_iSemaphore < 0 )
		return TTRUE;

	const TINT iResult = sceKernelDeleteSema( m_iSemaphore );
	if ( iResult >= 0 )
	{
		m_iSemaphore  = -1;
		m_iOwnerThread = -1;
		m_uiLockDepth  = 0;
	}

	return iResult >= 0;
}

TBOOL TMutex::Lock( FLAG a_eFlags )
{
	if ( m_iSemaphore < 0 )
		return TFALSE;

	const TINT iCurrentThread = sceKernelGetThreadId();
	if ( m_iOwnerThread == iCurrentThread )
	{
		m_uiLockDepth += 1;
		return TTRUE;
	}

	const TINT iResult = ( a_eFlags & FLAG_NO_WAIT ) ?
	                         sceKernelPollSema( m_iSemaphore, 1 ) :
	                         sceKernelWaitSema( m_iSemaphore, 1, TNULL );

	if ( iResult < 0 )
		return TFALSE;

	m_iOwnerThread = iCurrentThread;
	m_uiLockDepth  = 1;
	return TTRUE;
}

TBOOL TMutex::Unlock()
{
	if ( m_iSemaphore < 0 ||
	     m_iOwnerThread != sceKernelGetThreadId() ||
	     m_uiLockDepth == 0 )
	{
		return TFALSE;
	}

	m_uiLockDepth -= 1;
	if ( m_uiLockDepth != 0 )
		return TTRUE;

	m_iOwnerThread = -1;
	return sceKernelSignalSema( m_iSemaphore, 1 ) >= 0;
}

TMutex& TMutex::operator=( const TMutex& a_rMutex )
{
	m_iSemaphore   = a_rMutex.m_iSemaphore;
	m_iOwnerThread = a_rMutex.m_iOwnerThread;
	m_uiLockDepth  = a_rMutex.m_uiLockDepth;
	return *this;
}

TMutex& TMutex::operator=( void* a_pHandle )
{
	m_iSemaphore = TINT( reinterpret_cast<TUINTPTR>( a_pHandle ) );
	return *this;
}

TOSHI_NAMESPACE_END
