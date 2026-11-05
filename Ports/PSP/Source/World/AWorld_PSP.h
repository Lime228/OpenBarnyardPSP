#pragma once

#include <Toshi/Typedefs.h>

namespace Toshi
{
class T2Texture;
class TModel_PSP;
class TRenderResourceCache_PSP;
struct TRenderMaterial_PSP;
}

struct AWorldDataHeader_PSP
{
	enum : TUINT32
	{
		MAGIC = 0x444C5750,
		VERSION = 2,
	};

	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TUINT32 uiInstanceCount;
	TUINT32 uiInstanceSize;
	TUINT32 uiResourceCount;
	TUINT32 uiResourceSize;
};

struct AWorldResourceData_PSP
{
	TCHAR szModel[ 24 ];
	TCHAR szTexture[ 24 ];
};

struct AWorldInstance_PSP
{
	TFLOAT  aPosition[ 3 ];
	TFLOAT  aRotation[ 3 ];
	TFLOAT  fScale;
	TUINT32 uiModelIndex;
};

struct AWorldModelResource_PSP
{
	const Toshi::TModel_PSP*          pModel;
	const Toshi::T2Texture*           pTexture;
	const Toshi::TRenderMaterial_PSP* pMaterial;
};

class AWorld_PSP
{
public:
	static constexpr TUINT MAX_INSTANCES = 16;
	static constexpr TUINT MAX_MODEL_RESOURCES = 4;

public:
	AWorld_PSP();

	TBOOL LoadFromFile( const TCHAR* a_pPath );
	TBOOL LoadFromMemory( const void* a_pData, TSIZE a_uiDataSize );
	void  Unload();

	TBOOL IsLoaded() const { return m_bLoaded; }
	TUINT GetInstanceCount() const { return m_uiInstanceCount; }
	TUINT GetResourceCount() const { return m_uiResourceCount; }
	const AWorldInstance_PSP* GetInstance( TUINT a_uiIndex ) const;
	const AWorldResourceData_PSP* GetResourceData( TUINT a_uiIndex ) const;
	TBOOL BindModelResource(
	    TUINT a_uiIndex,
	    const Toshi::TModel_PSP* a_pModel,
	    const Toshi::T2Texture* a_pTexture,
	    const Toshi::TRenderMaterial_PSP* a_pMaterial
	);
	const AWorldModelResource_PSP* GetModelResource( TUINT a_uiIndex ) const;
	TBOOL ResolveModelResources(
	    const Toshi::TRenderResourceCache_PSP& a_rResourceCache,
	    const Toshi::TRenderMaterial_PSP* a_pMaterial
	);
	TBOOL ValidateResources() const;
	TUINT CollectVisibleInstances(
	    TFLOAT a_fViewX,
	    TFLOAT a_fViewZ,
	    TUINT16* a_pOutIndices,
	    TUINT a_uiCapacity
	) const;

private:
	TBOOL IsValidInstance( const AWorldInstance_PSP& a_rInstance ) const;
	TBOOL IsValidResourceName( const TCHAR* a_pName ) const;
	TBOOL IsInstanceVisible(
	    const AWorldInstance_PSP& a_rInstance,
	    TFLOAT a_fViewX,
	    TFLOAT a_fViewZ
	) const;

private:
	AWorldInstance_PSP      m_aInstances[ MAX_INSTANCES ];
	AWorldResourceData_PSP  m_aResourceData[ MAX_MODEL_RESOURCES ];
	AWorldModelResource_PSP m_aModelResources[ MAX_MODEL_RESOURCES ];
	TUINT                   m_uiInstanceCount;
	TUINT                   m_uiResourceCount;
	TBOOL                   m_bLoaded;
};

static_assert( sizeof( AWorldDataHeader_PSP ) == 24, "PWLD header must stay fixed-width" );
static_assert( sizeof( AWorldResourceData_PSP ) == 48, "PWLD resource must stay fixed-width" );
static_assert( sizeof( AWorldInstance_PSP ) == 32, "PWLD instance must stay fixed-width" );
