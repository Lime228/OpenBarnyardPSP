#include <ToshiPCH.h>
#include "AWaveBank_PSP.h"

#include <Toshi/TStringManager.h>

AWaveBank_PSP::AWaveBank_PSP()
    : m_aEntries{}
    , m_uiCount( 0 )
{
}

TBOOL AWaveBank_PSP::RegisterWave( const TCHAR* a_pName, AWave_PSP* a_pWave )
{
	if ( a_pName == TNULL ||
	     a_pWave == TNULL ||
	     !a_pWave->IsCreated() ||
	     a_pName[ 0 ] == '\0' ||
	     Toshi::TStringManager::String8Length( a_pName ) >= MAX_WAVE_NAME ||
	     m_uiCount >= MAX_WAVES ||
	     FindWave( a_pName ) != TNULL )
	{
		return TFALSE;
	}

	Entry& rEntry = m_aEntries[ m_uiCount++ ];
	Toshi::TStringManager::String8CopySafe( rEntry.szName, a_pName, MAX_WAVE_NAME );
	rEntry.pWave = a_pWave;
	return TTRUE;
}

AWave_PSP* AWaveBank_PSP::FindWave( const TCHAR* a_pName ) const
{
	if ( a_pName == TNULL )
		return TNULL;

	for ( TUINT i = 0; i < m_uiCount; i++ )
	{
		if ( Toshi::TStringManager::String8CompareNoCase( m_aEntries[ i ].szName, a_pName ) == 0 )
			return m_aEntries[ i ].pWave;
	}

	return TNULL;
}

TBOOL AWaveBank_PSP::DeregisterWave( const TCHAR* a_pName )
{
	if ( a_pName == TNULL )
		return TFALSE;

	for ( TUINT i = 0; i < m_uiCount; i++ )
	{
		if ( Toshi::TStringManager::String8CompareNoCase( m_aEntries[ i ].szName, a_pName ) == 0 )
		{
			m_uiCount--;
			if ( i != m_uiCount )
				m_aEntries[ i ] = m_aEntries[ m_uiCount ];
			m_aEntries[ m_uiCount ] = {};
			return TTRUE;
		}
	}

	return TFALSE;
}

void AWaveBank_PSP::Clear()
{
	for ( TUINT i = 0; i < m_uiCount; i++ )
		m_aEntries[ i ] = {};
	m_uiCount = 0;
}
