#include "ToshiPCH.h"
#include "TInputInterface_PSP.h"

TOSHI_NAMESPACE_START

TDEFINE_CLASS_NORUNTIME( TInputInterface_PSP );

TInputInterface_PSP::TInputInterface_PSP()
    : m_bInitialised( TFALSE )
{
}

TInputInterface_PSP::~TInputInterface_PSP()
{
	Deinitialise();
}

TBOOL TInputInterface_PSP::Initialise()
{
	if ( m_bInitialised )
		return TTRUE;

	AddDevice( &m_oController );
	if ( !m_oController.Acquire() )
	{
		RemoveDevice( &m_oController );
		return TFALSE;
	}

	m_bInitialised = TTRUE;
	return TTRUE;
}

TBOOL TInputInterface_PSP::Deinitialise()
{
	if ( !m_bInitialised )
		return TTRUE;

	m_oController.Unacquire();
	m_oController.Release();
	RemoveDevice( &m_oController );
	m_bInitialised = TFALSE;
	return TTRUE;
}

TOSHI_NAMESPACE_END
