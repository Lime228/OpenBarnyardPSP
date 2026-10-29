#include "ToshiPCH.h"
#include "TModel_PSP.h"

#include <cstring>

TOSHI_NAMESPACE_START

TModel_PSP::TModel_PSP()
    : m_pData( TNULL )
    , m_uiDataSize( 0 )
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
	if ( pHeader->uiMagic != TModelDataHeader_PSP::MAGIC ||
	     pHeader->uiVersion != TModelDataHeader_PSP::VERSION ||
	     pHeader->uiFormat != TMODELFORMAT_PSP_COLOUR_XYZ ||
	     pHeader->uiVertexSize != sizeof( TModelVertex_PSP ) ||
	     pHeader->uiVertexCount == 0 ||
	     pHeader->uiVertexCount > 0xffff ||
	     pHeader->uiIndexCount == 0 ||
	     pHeader->uiIndexCount > 0xffff ||
	     pHeader->uiIndexCount % 3 != 0 )
	{
		return TFALSE;
	}

	const TSIZE uiVertexDataSize = TSIZE( pHeader->uiVertexCount ) * sizeof( TModelVertex_PSP );
	const TSIZE uiIndexDataSize  = TSIZE( pHeader->uiIndexCount ) * sizeof( TIndexType );
	const TSIZE uiExpectedSize   = sizeof( TModelDataHeader_PSP ) + uiVertexDataSize + uiIndexDataSize;
	if ( TSIZE( m_uiDataSize ) != uiExpectedSize )
		return TFALSE;

	const auto* pSourceBytes   = static_cast<const TUINT8*>( m_pData );
	const auto* pSourceIndices = reinterpret_cast<const TIndexType*>( pSourceBytes + sizeof( TModelDataHeader_PSP ) + uiVertexDataSize );
	for ( TUINT32 i = 0; i < pHeader->uiIndexCount; i++ )
	{
		if ( pSourceIndices[ i ] >= pHeader->uiVertexCount )
			return TFALSE;
	}

	const TUINT16 uiVertexCount = TUINT16( pHeader->uiVertexCount );
	const TUINT16 uiIndexCount  = TUINT16( pHeader->uiIndexCount );
	if ( !m_Vertices.Create( TUINT16( sizeof( TModelVertex_PSP ) ), uiVertexCount ) ||
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
	return TTRUE;
}

void TModel_PSP::Unload()
{
	m_Indices.Destroy();
	m_Vertices.Destroy();
}

TOSHI_NAMESPACE_END
