#pragma once

#include <Toshi/Typedefs.h>

struct AWorldDataHeader_PSP
{
	enum : TUINT32
	{
		MAGIC = 0x444C5750,
		VERSION = 1,
	};

	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TUINT32 uiInstanceCount;
	TUINT32 uiInstanceSize;
};

struct AWorldInstance_PSP
{
	TFLOAT  aPosition[ 3 ];
	TFLOAT  aRotation[ 3 ];
	TFLOAT  fScale;
	TUINT32 uiModelIndex;
};

class AWorld_PSP
{
public:
	static constexpr TUINT MAX_INSTANCES = 16;

public:
	AWorld_PSP();

	TBOOL LoadFromFile( const TCHAR* a_pPath );
	TBOOL LoadFromMemory( const void* a_pData, TSIZE a_uiDataSize );
	void  Unload();

	TBOOL IsLoaded() const { return m_bLoaded; }
	TUINT GetInstanceCount() const { return m_uiInstanceCount; }
	const AWorldInstance_PSP* GetInstance( TUINT a_uiIndex ) const;

private:
	TBOOL IsValidInstance( const AWorldInstance_PSP& a_rInstance ) const;

private:
	AWorldInstance_PSP m_aInstances[ MAX_INSTANCES ];
	TUINT              m_uiInstanceCount;
	TBOOL              m_bLoaded;
};

static_assert( sizeof( AWorldDataHeader_PSP ) == 16, "PWLD header must stay fixed-width" );
static_assert( sizeof( AWorldInstance_PSP ) == 32, "PWLD instance must stay fixed-width" );
