#include "ToshiPCH.h"
#include "Thread/TSemaphore.h"

#include <pspthreadman.h>

TOSHI_NAMESPACE_START

TSemaphore::~TSemaphore()
{
	Destroy();
}

TBOOL TSemaphore::Create( TINT a_iInitCount, TINT a_iMaxCount )
{
	TASSERT( a_iInitCount >= 0, "Init count should be greater than or equal to zero" );
	TASSERT( a_iMaxCount >= a_iInitCount, "Init count cannot exceed max count" );

	if ( m_iSemaphore >= 0 )
		return TFALSE;

	m_iSemaphore = sceKernelCreateSema(
	    "ToshiSemaphore",
	    0,
	    a_iInitCount,
	    a_iMaxCount,
	    TNULL
	);

	return m_iSemaphore >= 0;
}

TBOOL TSemaphore::Destroy()
{
	if ( m_iSemaphore < 0 )
		return TTRUE;

	const TINT iResult = sceKernelDeleteSema( m_iSemaphore );
	if ( iResult >= 0 )
		m_iSemaphore = -1;

	return iResult >= 0;
}

TBOOL TSemaphore::Signal()
{
	return m_iSemaphore >= 0 && sceKernelSignalSema( m_iSemaphore, 1 ) >= 0;
}

TBOOL TSemaphore::Wait()
{
	return m_iSemaphore >= 0 && sceKernelWaitSema( m_iSemaphore, 1, TNULL ) >= 0;
}

TBOOL TSemaphore::Poll()
{
	return m_iSemaphore >= 0 && sceKernelPollSema( m_iSemaphore, 1 ) >= 0;
}

TOSHI_NAMESPACE_END
