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
	if ( a_pWave == TNULL || !a_pWave->IsCreated() || !CanRegister( a_pName ) )
	{
		return TFALSE;
	}

	Entry& rEntry = m_aEntries[ m_uiCount++ ];
	Toshi::TStringManager::String8CopySafe( rEntry.szName, a_pName, MAX_WAVE_NAME );
	rEntry.eType = AWaveBankEntryType_PSP_Resident;
	rEntry.pWave = a_pWave;
	return TTRUE;
}

TBOOL AWaveBank_PSP::RegisterStream( const TCHAR* a_pName, AStreamWave_PSP* a_pStream )
{
	if ( a_pStream == TNULL || !a_pStream->IsCreated() || !CanRegister( a_pName ) )
		return TFALSE;

	Entry& rEntry = m_aEntries[ m_uiCount++ ];
	Toshi::TStringManager::String8CopySafe( rEntry.szName, a_pName, MAX_WAVE_NAME );
	rEntry.eType = AWaveBankEntryType_PSP_Stream;
	rEntry.pStream = a_pStream;
	return TTRUE;
}

AWave_PSP* AWaveBank_PSP::FindWave( const TCHAR* a_pName ) const
{
	if ( a_pName == TNULL )
		return TNULL;

	for ( TUINT i = 0; i < m_uiCount; i++ )
	{
		if ( m_aEntries[ i ].eType == AWaveBankEntryType_PSP_Resident &&
		     Toshi::TStringManager::String8CompareNoCase( m_aEntries[ i ].szName, a_pName ) == 0 )
			return m_aEntries[ i ].pWave;
	}

	return TNULL;
}

AStreamWave_PSP* AWaveBank_PSP::FindStream( const TCHAR* a_pName ) const
{
	if ( a_pName == TNULL )
		return TNULL;

	for ( TUINT i = 0; i < m_uiCount; i++ )
	{
		if ( m_aEntries[ i ].eType == AWaveBankEntryType_PSP_Stream &&
		     Toshi::TStringManager::String8CompareNoCase( m_aEntries[ i ].szName, a_pName ) == 0 )
		{
			return m_aEntries[ i ].pStream;
		}
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

TBOOL AWaveBank_PSP::CanRegister( const TCHAR* a_pName ) const
{
	if ( a_pName == TNULL || a_pName[ 0 ] == '\0' ||
	     Toshi::TStringManager::String8Length( a_pName ) >= MAX_WAVE_NAME ||
	     m_uiCount >= MAX_WAVES )
	{
		return TFALSE;
	}

	for ( TUINT i = 0; i < m_uiCount; i++ )
	{
		if ( Toshi::TStringManager::String8CompareNoCase( m_aEntries[ i ].szName, a_pName ) == 0 )
			return TFALSE;
	}
	return TTRUE;
}
