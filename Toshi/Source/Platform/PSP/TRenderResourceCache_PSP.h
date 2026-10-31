#pragma once

#include "Toshi/Defines.h"

TOSHI_NAMESPACE_START

enum TRenderResourceType_PSP : TUINT32
{
	TRENDERRESOURCETYPE_PSP_TEXTURE,
	TRENDERRESOURCETYPE_PSP_MODEL,
	TRENDERRESOURCETYPE_PSP_FONT,
};

class TRenderResourceCache_PSP
{
public:
	static constexpr TUINT MAX_RESOURCES    = 32;
	static constexpr TUINT MAX_RESOURCE_NAME = 50;

public:
	TRenderResourceCache_PSP();

	TBOOL Register( const TCHAR* a_pName, TRenderResourceType_PSP a_eType, void* a_pResource );
	void* Find( const TCHAR* a_pName, TRenderResourceType_PSP a_eType ) const;
	TBOOL Deregister( const TCHAR* a_pName, TRenderResourceType_PSP a_eType );
	void  Clear();

	TUINT GetCount() const { return m_uiCount; }
	TUINT GetCapacity() const { return MAX_RESOURCES; }

private:
	struct Entry
	{
		TCHAR                   szName[ MAX_RESOURCE_NAME ];
		TRenderResourceType_PSP eType;
		void*                   pResource;
	};

private:
	Entry m_aEntries[ MAX_RESOURCES ];
	TUINT m_uiCount;
};

TOSHI_NAMESPACE_END
