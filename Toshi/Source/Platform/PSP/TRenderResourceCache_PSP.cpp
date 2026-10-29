#include "ToshiPCH.h"
#include "TRenderResourceCache_PSP.h"

#include "Toshi/TStringManager.h"

TOSHI_NAMESPACE_START

TRenderResourceCache_PSP::TRenderResourceCache_PSP()
    : m_uiCount( 0 )
{
}

TBOOL TRenderResourceCache_PSP::Register( const TCHAR* a_pName, TRenderResourceType_PSP a_eType, void* a_pResource )
{
	if ( a_pName == TNULL ||
	     a_pResource == TNULL ||
	     a_pName[ 0 ] == '\0' ||
	     TStringManager::String8Length( a_pName ) >= MAX_RESOURCE_NAME ||
	     m_uiCount >= MAX_RESOURCES ||
	     Find( a_pName, a_eType ) != TNULL )
	{
		return TFALSE;
	}

	Entry& rEntry = m_aEntries[ m_uiCount++ ];
	TStringManager::String8CopySafe( rEntry.szName, a_pName, MAX_RESOURCE_NAME );
	rEntry.eType     = a_eType;
	rEntry.pResource = a_pResource;
	return TTRUE;
}

void* TRenderResourceCache_PSP::Find( const TCHAR* a_pName, TRenderResourceType_PSP a_eType ) const
{
	if ( a_pName == TNULL )
		return TNULL;

	for ( TUINT i = 0; i < m_uiCount; i++ )
	{
		const Entry& rEntry = m_aEntries[ i ];
		if ( rEntry.eType == a_eType &&
		     TStringManager::String8CompareNoCase( rEntry.szName, a_pName ) == 0 )
		{
			return rEntry.pResource;
		}
	}

	return TNULL;
}

TBOOL TRenderResourceCache_PSP::Deregister( const TCHAR* a_pName, TRenderResourceType_PSP a_eType )
{
	if ( a_pName == TNULL )
		return TFALSE;

	for ( TUINT i = 0; i < m_uiCount; i++ )
	{
		if ( m_aEntries[ i ].eType == a_eType &&
		     TStringManager::String8CompareNoCase( m_aEntries[ i ].szName, a_pName ) == 0 )
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

void TRenderResourceCache_PSP::Clear()
{
	for ( TUINT i = 0; i < m_uiCount; i++ )
		m_aEntries[ i ] = {};
	m_uiCount = 0;
}

TOSHI_NAMESPACE_END
