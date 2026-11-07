#include "ToshiPCH.h"
#include "TDebugConsole_PSP.h"

TOSHI_NAMESPACE_START

TDebugConsole::TDebugConsole()
    : m_bVisible( TFALSE )
    , m_bCreated( TTRUE )
{}

TDebugConsole::~TDebugConsole() = default;

TBOOL TDebugConsole::Show( TBOOL a_bState )
{
	TBOOL bOldState = m_bVisible;
	m_bVisible      = a_bState;
	return bOldState;
}

TBOOL TDebugConsole::Toggle()
{
	m_bVisible = !m_bVisible;
	return m_bVisible;
}

TOSHI_NAMESPACE_END
