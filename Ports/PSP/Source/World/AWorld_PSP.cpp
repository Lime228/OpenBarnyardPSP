#include "AWorld_PSP.h"

#include <Toshi/TDebug.h>
#include <File/TFile.h>
#include <Math/TMathInline.h>

#include <cstring>

static constexpr TSIZE s_uiMaximumWorldFileSize_PSP =
    sizeof( AWorldDataHeader_PSP ) +
    AWorld_PSP::MAX_INSTANCES * sizeof( AWorldInstance_PSP );

AWorld_PSP::AWorld_PSP()
    : m_aInstances{}
    , m_uiInstanceCount( 0 )
    , m_bLoaded( TFALSE )
{
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
	       a_rInstance.uiModelIndex == 0;
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
	     a_uiDataSize != sizeof( AWorldDataHeader_PSP ) +
	                         oHeader.uiInstanceCount * sizeof( AWorldInstance_PSP ) )
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
		if ( !IsValidInstance( oInstance ) )
		{
			Unload();
			return TFALSE;
		}
		m_aInstances[ i ] = oInstance;
	}

	m_uiInstanceCount = oHeader.uiInstanceCount;
	m_bLoaded = TTRUE;
	return TTRUE;
}

void AWorld_PSP::Unload()
{
	m_uiInstanceCount = 0;
	m_bLoaded = TFALSE;
}

const AWorldInstance_PSP* AWorld_PSP::GetInstance( TUINT a_uiIndex ) const
{
	return m_bLoaded && a_uiIndex < m_uiInstanceCount
	    ? &m_aInstances[ a_uiIndex ]
	    : TNULL;
}
