#include "AWorld_PSP.h"

#include <Toshi/TDebug.h>
#include <File/TFile.h>
#include <Math/TMathInline.h>
#include <Platform/PSP/T2Texture_PSP.h>
#include <Platform/PSP/TModel_PSP.h>
#include <Platform/PSP/TRenderInterface_PSP.h>
#include <Platform/PSP/TRenderResourceCache_PSP.h>

#include <cstring>

static constexpr TSIZE s_uiMaximumWorldFileSize_PSP =
    sizeof( AWorldDataHeader_PSP ) +
    AWorld_PSP::MAX_INSTANCES * sizeof( AWorldInstance_PSP ) +
    AWorld_PSP::MAX_MODEL_RESOURCES * sizeof( AWorldResourceData_PSP );
static constexpr TFLOAT s_fWorldNearPlane_PSP = 0.1f;
static constexpr TFLOAT s_fWorldFarPlane_PSP = 100.0f;
static constexpr TFLOAT s_fWorldHalfVerticalTangent_PSP = 0.41421356f;
static constexpr TFLOAT s_fWorldAspectRatio_PSP =
    TFLOAT( Toshi::TRenderInterface_PSP::SCREEN_WIDTH ) /
    TFLOAT( Toshi::TRenderInterface_PSP::SCREEN_HEIGHT );

AWorld_PSP::AWorld_PSP()
    : m_aInstances{}
    , m_aResourceData{}
    , m_aModelResources{}
    , m_uiInstanceCount( 0 )
    , m_uiResourceCount( 0 )
    , m_bLoaded( TFALSE )
{
}

TBOOL AWorld_PSP::IsValidResourceName( const TCHAR* a_pName ) const
{
	if ( a_pName == TNULL || a_pName[ 0 ] == '\0' )
		return TFALSE;
	for ( TUINT i = 0; i < 24; i++ )
	{
		const TCHAR cCharacter = a_pName[ i ];
		if ( cCharacter == '\0' )
			return TTRUE;
		const TBOOL bValid =
		    ( cCharacter >= 'a' && cCharacter <= 'z' ) ||
		    ( cCharacter >= 'A' && cCharacter <= 'Z' ) ||
		    ( cCharacter >= '0' && cCharacter <= '9' ) ||
		    cCharacter == '.' || cCharacter == '_' || cCharacter == '-';
		if ( !bValid )
			return TFALSE;
	}
	return TFALSE;
}

TBOOL AWorld_PSP::IsValidInstance( const AWorldInstance_PSP& a_rInstance ) const
{
	for ( TUINT i = 0; i < 3; i++ )
	{
		if ( !Toshi::TMath::IsFinite( a_rInstance.aPosition[ i ] ) ||
		     !Toshi::TMath::IsFinite( a_rInstance.aRotation[ i ] ) ||
		     Toshi::TMath::Abs( a_rInstance.aPosition[ i ] ) > 1000.0f )
		{
			return TFALSE;
		}
	}

	return Toshi::TMath::IsFinite( a_rInstance.fScale ) &&
	       a_rInstance.fScale >= 0.001f &&
	       a_rInstance.fScale <= 100.0f &&
	       a_rInstance.uiModelIndex < MAX_MODEL_RESOURCES;
}

TBOOL AWorld_PSP::LoadFromFile( const TCHAR* a_pPath )
{
	Unload();
	if ( a_pPath == TNULL )
		return TFALSE;

	Toshi::TFile* pFile = Toshi::TFile::Create(
	    a_pPath,
	    Toshi::TFILEMODE_READ | Toshi::TFILEMODE_NOBUFFER
	);
	if ( pFile == TNULL )
		return TFALSE;

	alignas( 16 ) TUINT8 aData[ s_uiMaximumWorldFileSize_PSP ];
	const TSIZE uiFileSize = pFile->GetSize();
	const TBOOL bRead = uiFileSize >= sizeof( AWorldDataHeader_PSP ) &&
	                    uiFileSize <= sizeof( aData ) &&
	                    pFile->Read( aData, uiFileSize ) == uiFileSize;
	pFile->Destroy();
	return bRead && LoadFromMemory( aData, uiFileSize );
}

TBOOL AWorld_PSP::LoadFromMemory( const void* a_pData, TSIZE a_uiDataSize )
{
	Unload();
	if ( a_pData == TNULL || a_uiDataSize < sizeof( AWorldDataHeader_PSP ) )
		return TFALSE;

	AWorldDataHeader_PSP oHeader;
	std::memcpy( &oHeader, a_pData, sizeof( oHeader ) );
	if ( oHeader.uiMagic != AWorldDataHeader_PSP::MAGIC ||
	     oHeader.uiVersion != AWorldDataHeader_PSP::VERSION ||
	     oHeader.uiInstanceCount == 0 ||
	     oHeader.uiInstanceCount > MAX_INSTANCES ||
	     oHeader.uiInstanceSize != sizeof( AWorldInstance_PSP ) ||
	     oHeader.uiResourceCount == 0 ||
	     oHeader.uiResourceCount > MAX_MODEL_RESOURCES ||
	     oHeader.uiResourceSize != sizeof( AWorldResourceData_PSP ) ||
	     a_uiDataSize != sizeof( AWorldDataHeader_PSP ) +
	                         oHeader.uiInstanceCount * sizeof( AWorldInstance_PSP ) +
	                         oHeader.uiResourceCount * sizeof( AWorldResourceData_PSP ) )
	{
		return TFALSE;
	}

	const TUINT8* pInstances = static_cast<const TUINT8*>( a_pData ) + sizeof( oHeader );
	for ( TUINT i = 0; i < oHeader.uiInstanceCount; i++ )
	{
		AWorldInstance_PSP oInstance;
		std::memcpy(
		    &oInstance,
		    pInstances + i * sizeof( AWorldInstance_PSP ),
		    sizeof( oInstance )
		);
		if ( !IsValidInstance( oInstance ) || oInstance.uiModelIndex >= oHeader.uiResourceCount )
		{
			Unload();
			return TFALSE;
		}
		m_aInstances[ i ] = oInstance;
	}
	const TUINT8* pResources = pInstances + oHeader.uiInstanceCount * sizeof( AWorldInstance_PSP );
	for ( TUINT i = 0; i < oHeader.uiResourceCount; i++ )
	{
		AWorldResourceData_PSP oResource;
		std::memcpy(
		    &oResource,
		    pResources + i * sizeof( AWorldResourceData_PSP ),
		    sizeof( oResource )
		);
		if ( !IsValidResourceName( oResource.szModel ) ||
		     !IsValidResourceName( oResource.szTexture ) )
		{
			Unload();
			return TFALSE;
		}
		m_aResourceData[ i ] = oResource;
	}

	m_uiInstanceCount = oHeader.uiInstanceCount;
	m_uiResourceCount = oHeader.uiResourceCount;
	m_bLoaded = TTRUE;
	return TTRUE;
}

void AWorld_PSP::Unload()
{
	for ( TUINT i = 0; i < MAX_MODEL_RESOURCES; i++ )
	{
		m_aResourceData[ i ] = {};
		m_aModelResources[ i ] = {};
	}
	m_uiInstanceCount = 0;
	m_uiResourceCount = 0;
	m_bLoaded = TFALSE;
}

const AWorldResourceData_PSP* AWorld_PSP::GetResourceData( TUINT a_uiIndex ) const
{
	return m_bLoaded && a_uiIndex < m_uiResourceCount
	    ? &m_aResourceData[ a_uiIndex ]
	    : TNULL;
}

TBOOL AWorld_PSP::BindModelResource(
    TUINT a_uiIndex,
    const Toshi::TModel_PSP* a_pModel,
    const Toshi::T2Texture* a_pTexture,
    const Toshi::TRenderMaterial_PSP* a_pMaterial
)
{
	if ( a_uiIndex >= MAX_MODEL_RESOURCES ||
	     a_pModel == TNULL ||
	     a_pTexture == TNULL ||
	     a_pMaterial == TNULL )
	{
		return TFALSE;
	}

	m_aModelResources[ a_uiIndex ] = { a_pModel, a_pTexture, a_pMaterial };
	return TTRUE;
}

const AWorldModelResource_PSP* AWorld_PSP::GetModelResource( TUINT a_uiIndex ) const
{
	if ( a_uiIndex >= MAX_MODEL_RESOURCES )
		return TNULL;
	const AWorldModelResource_PSP& rResource = m_aModelResources[ a_uiIndex ];
	return rResource.pModel != TNULL &&
	               rResource.pTexture != TNULL &&
	               rResource.pMaterial != TNULL
	    ? &rResource
	    : TNULL;
}

TBOOL AWorld_PSP::ResolveModelResources(
    const Toshi::TRenderResourceCache_PSP& a_rResourceCache,
    const Toshi::TRenderMaterial_PSP* a_pMaterial
)
{
	if ( !m_bLoaded || a_pMaterial == TNULL )
		return TFALSE;
	for ( TUINT i = 0; i < MAX_MODEL_RESOURCES; i++ )
		m_aModelResources[ i ] = {};
	for ( TUINT i = 0; i < m_uiResourceCount; i++ )
	{
		const AWorldResourceData_PSP& rData = m_aResourceData[ i ];
		auto* pModel = static_cast<Toshi::TModel_PSP*>( a_rResourceCache.Find(
		    rData.szModel,
		    Toshi::TRENDERRESOURCETYPE_PSP_MODEL
		) );
		auto* pTexture = static_cast<Toshi::T2Texture*>( a_rResourceCache.Find(
		    rData.szTexture,
		    Toshi::TRENDERRESOURCETYPE_PSP_TEXTURE
		) );
		if ( !BindModelResource( i, pModel, pTexture, a_pMaterial ) )
		{
			for ( TUINT j = 0; j < MAX_MODEL_RESOURCES; j++ )
				m_aModelResources[ j ] = {};
			return TFALSE;
		}
	}
	return ValidateResources();
}

TBOOL AWorld_PSP::ValidateResources() const
{
	if ( !m_bLoaded )
		return TFALSE;
	for ( TUINT i = 0; i < m_uiInstanceCount; i++ )
	{
		const AWorldModelResource_PSP* pResource = GetModelResource( m_aInstances[ i ].uiModelIndex );
		if ( pResource == TNULL ||
		     !pResource->pModel->IsLoaded() ||
		     pResource->pTexture->GetImageData() == TNULL )
		{
			return TFALSE;
		}
	}
	return TTRUE;
}

TBOOL AWorld_PSP::IsInstanceVisible(
    const AWorldInstance_PSP& a_rInstance,
    TFLOAT a_fViewX,
    TFLOAT a_fViewZ
) const
{
	const TFLOAT fX = a_rInstance.aPosition[ 0 ] + a_fViewX;
	const TFLOAT fY = a_rInstance.aPosition[ 1 ];
	const TFLOAT fDepth = -( a_rInstance.aPosition[ 2 ] + a_fViewZ );
	const TFLOAT fRadius = a_rInstance.fScale * 1.75f;
	if ( fDepth + fRadius < s_fWorldNearPlane_PSP ||
	     fDepth - fRadius > s_fWorldFarPlane_PSP )
	{
		return TFALSE;
	}

	const TFLOAT fHalfHeight = fDepth * s_fWorldHalfVerticalTangent_PSP;
	const TFLOAT fHalfWidth = fHalfHeight * s_fWorldAspectRatio_PSP;
	return fHalfHeight >= 0.0f &&
	       Toshi::TMath::Abs( fX ) <= fHalfWidth + fRadius &&
	       Toshi::TMath::Abs( fY ) <= fHalfHeight + fRadius;
}

TUINT AWorld_PSP::CollectVisibleInstances(
    TFLOAT a_fViewX,
    TFLOAT a_fViewZ,
    TUINT16* a_pOutIndices,
    TUINT a_uiCapacity
) const
{
	if ( !m_bLoaded || a_pOutIndices == TNULL || a_uiCapacity == 0 ||
	     !Toshi::TMath::IsFinite( a_fViewX ) || !Toshi::TMath::IsFinite( a_fViewZ ) )
	{
		return 0;
	}

	TUINT uiVisibleCount = 0;
	for ( TUINT i = 0; i < m_uiInstanceCount && uiVisibleCount < a_uiCapacity; i++ )
	{
		if ( IsInstanceVisible( m_aInstances[ i ], a_fViewX, a_fViewZ ) )
			a_pOutIndices[ uiVisibleCount++ ] = TUINT16( i );
	}
	return uiVisibleCount;
}

const AWorldInstance_PSP* AWorld_PSP::GetInstance( TUINT a_uiIndex ) const
{
	return m_bLoaded && a_uiIndex < m_uiInstanceCount
	    ? &m_aInstances[ a_uiIndex ]
	    : TNULL;
}
