#include "ToshiPCH.h"
#include "TModel_PSP.h"

#include <Math/TMathInline.h>

#include <cstring>

TOSHI_NAMESPACE_START

static TUINT32 TModelGetVertexSize_PSP( TUINT32 a_uiFormat )
{
	switch ( a_uiFormat )
	{
		case TMODELFORMAT_PSP_COLOUR_XYZ:
			return sizeof( TModelVertex_PSP );
		case TMODELFORMAT_PSP_TEXTURE_COLOUR_XYZ:
			return sizeof( TModelTextureVertex_PSP );
		case TMODELFORMAT_PSP_SKIN_TEXTURE_COLOUR_XYZ:
			return sizeof( TModelSkinTextureVertex_PSP );
		default:
			return 0;
	}
}

static void TModelGetVertexPosition_PSP(
    const TUINT8* a_pVertex,
    TUINT32 a_uiFormat,
    TFLOAT& a_rfX,
    TFLOAT& a_rfY,
    TFLOAT& a_rfZ
)
{
	if ( a_uiFormat == TMODELFORMAT_PSP_COLOUR_XYZ )
	{
		const auto* pVertex = reinterpret_cast<const TModelVertex_PSP*>( a_pVertex );
		a_rfX = pVertex->fX;
		a_rfY = pVertex->fY;
		a_rfZ = pVertex->fZ;
	}
	else if ( a_uiFormat == TMODELFORMAT_PSP_TEXTURE_COLOUR_XYZ )
	{
		const auto* pVertex = reinterpret_cast<const TModelTextureVertex_PSP*>( a_pVertex );
		a_rfX = pVertex->fX;
		a_rfY = pVertex->fY;
		a_rfZ = pVertex->fZ;
	}
	else
	{
		const auto* pVertex = reinterpret_cast<const TModelSkinTextureVertex_PSP*>( a_pVertex );
		a_rfX = pVertex->fX;
		a_rfY = pVertex->fY;
		a_rfZ = pVertex->fZ;
	}
}

TModel_PSP::TModel_PSP()
    : m_pData( TNULL )
    , m_uiDataSize( 0 )
    , m_uiFormat( TMODELFORMAT_PSP_COLOUR_XYZ )
    , m_Bounds{}
{
}

TModel_PSP::~TModel_PSP()
{
	Unload();
}

void TModel_PSP::SetData( const void* a_pData, TUINT a_uiDataSize )
{
	m_pData      = a_pData;
	m_uiDataSize = a_uiDataSize;
}

void TModel_PSP::GetData( const void*& a_rData, TUINT& a_rDataSize ) const
{
	a_rData     = m_pData;
	a_rDataSize = m_uiDataSize;
}

TBOOL TModel_PSP::Load()
{
	Unload();
	if ( m_pData == TNULL || m_uiDataSize < sizeof( TModelDataHeader_PSP ) )
		return TFALSE;

	const auto* pHeader = static_cast<const TModelDataHeader_PSP*>( m_pData );
	const TUINT32 uiExpectedVertexSize = TModelGetVertexSize_PSP( pHeader->uiFormat );
	if ( pHeader->uiMagic != TModelDataHeader_PSP::MAGIC ||
	     pHeader->uiVersion != TModelDataHeader_PSP::VERSION ||
	     uiExpectedVertexSize == 0 ||
	     pHeader->uiVertexSize != uiExpectedVertexSize ||
	     pHeader->uiVertexCount == 0 ||
	     pHeader->uiVertexCount > 0xffff ||
	     pHeader->uiIndexCount == 0 ||
	     pHeader->uiIndexCount > 0xffff ||
	     pHeader->uiIndexCount % 3 != 0 )
	{
		return TFALSE;
	}

	const TSIZE uiVertexDataSize = TSIZE( pHeader->uiVertexCount ) * pHeader->uiVertexSize;
	const TSIZE uiIndexDataSize  = TSIZE( pHeader->uiIndexCount ) * sizeof( TIndexType );
	const TSIZE uiExpectedSize   = sizeof( TModelDataHeader_PSP ) + uiVertexDataSize + uiIndexDataSize;
	if ( TSIZE( m_uiDataSize ) != uiExpectedSize )
		return TFALSE;

	const auto* pSourceBytes = static_cast<const TUINT8*>( m_pData );
	const TUINT8* pSourceVertices = pSourceBytes + sizeof( TModelDataHeader_PSP );
	TModelGetVertexPosition_PSP(
	    pSourceVertices,
	    pHeader->uiFormat,
	    m_Bounds.aMin[ 0 ],
	    m_Bounds.aMin[ 1 ],
	    m_Bounds.aMin[ 2 ]
	);
	if ( !TMath::IsFinite( m_Bounds.aMin[ 0 ] ) ||
	     !TMath::IsFinite( m_Bounds.aMin[ 1 ] ) ||
	     !TMath::IsFinite( m_Bounds.aMin[ 2 ] ) )
	{
		Unload();
		return TFALSE;
	}
	m_Bounds.aMax[ 0 ] = m_Bounds.aMin[ 0 ];
	m_Bounds.aMax[ 1 ] = m_Bounds.aMin[ 1 ];
	m_Bounds.aMax[ 2 ] = m_Bounds.aMin[ 2 ];
	for ( TUINT32 i = 1; i < pHeader->uiVertexCount; i++ )
	{
		TFLOAT fX;
		TFLOAT fY;
		TFLOAT fZ;
		TModelGetVertexPosition_PSP(
		    pSourceVertices + TSIZE( i ) * pHeader->uiVertexSize,
		    pHeader->uiFormat,
		    fX,
		    fY,
		    fZ
		);
		if ( !TMath::IsFinite( fX ) || !TMath::IsFinite( fY ) || !TMath::IsFinite( fZ ) )
		{
			Unload();
			return TFALSE;
		}
		m_Bounds.aMin[ 0 ] = TMath::Min( m_Bounds.aMin[ 0 ], fX );
		m_Bounds.aMin[ 1 ] = TMath::Min( m_Bounds.aMin[ 1 ], fY );
		m_Bounds.aMin[ 2 ] = TMath::Min( m_Bounds.aMin[ 2 ], fZ );
		m_Bounds.aMax[ 0 ] = TMath::Max( m_Bounds.aMax[ 0 ], fX );
		m_Bounds.aMax[ 1 ] = TMath::Max( m_Bounds.aMax[ 1 ], fY );
		m_Bounds.aMax[ 2 ] = TMath::Max( m_Bounds.aMax[ 2 ], fZ );
	}
	if ( pHeader->uiFormat == TMODELFORMAT_PSP_SKIN_TEXTURE_COLOUR_XYZ )
	{
		const auto* pSkinVertices = reinterpret_cast<const TModelSkinTextureVertex_PSP*>(
		    pSourceBytes + sizeof( TModelDataHeader_PSP )
		);
		for ( TUINT32 i = 0; i < pHeader->uiVertexCount; i++ )
		{
			if ( pSkinVertices[ i ].uiReserved != 0 ||
			     TUINT32( pSkinVertices[ i ].aBoneWeights[ 0 ] ) + pSkinVertices[ i ].aBoneWeights[ 1 ] != 255 )
			{
				return TFALSE;
			}
		}
	}
	const auto* pSourceIndices = reinterpret_cast<const TIndexType*>( pSourceBytes + sizeof( TModelDataHeader_PSP ) + uiVertexDataSize );
	for ( TUINT32 i = 0; i < pHeader->uiIndexCount; i++ )
	{
		if ( pSourceIndices[ i ] >= pHeader->uiVertexCount )
			return TFALSE;
	}

	const TUINT16 uiVertexCount = TUINT16( pHeader->uiVertexCount );
	const TUINT16 uiIndexCount  = TUINT16( pHeader->uiIndexCount );
	if ( !m_Vertices.Create( TUINT16( pHeader->uiVertexSize ), uiVertexCount ) ||
	     !m_Indices.Create( uiIndexCount ) )
	{
		Unload();
		return TFALSE;
	}

	TBYTE*      pVertices = m_Vertices.Lock();
	TIndexType* pIndices  = m_Indices.Lock();
	if ( pVertices == TNULL || pIndices == TNULL )
	{
		if ( pVertices != TNULL ) m_Vertices.Unlock( 0 );
		if ( pIndices != TNULL ) m_Indices.Unlock( 0 );
		Unload();
		return TFALSE;
	}

	std::memcpy( pVertices, pSourceBytes + sizeof( TModelDataHeader_PSP ), uiVertexDataSize );
	std::memcpy( pIndices, pSourceIndices, uiIndexDataSize );
	m_Vertices.Unlock( uiVertexCount );
	m_Indices.Unlock( uiIndexCount );
	m_uiFormat = pHeader->uiFormat;
	return TTRUE;
}

void TModel_PSP::Unload()
{
	m_Indices.Destroy();
	m_Vertices.Destroy();
	m_uiFormat = TMODELFORMAT_PSP_COLOUR_XYZ;
	m_Bounds = {};
}

TOSHI_NAMESPACE_END
