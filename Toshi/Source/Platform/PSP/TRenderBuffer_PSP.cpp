#include "ToshiPCH.h"
#include "TRenderBuffer_PSP.h"

#include "TRenderMemory_PSP.h"
#include "Toshi/T2Allocator.h"

#include <pspkernel.h>

TOSHI_NAMESPACE_START

static constexpr TSIZE TRenderBufferAlignment_PSP = 16;

TVertexPoolResource_PSP::TVertexPoolResource_PSP()
    : m_pData( TNULL )
    , m_uiVertexSize( 0 )
    , m_uiMaxVertices( 0 )
    , m_uiNumVertices( 0 )
    , m_bLocked( TFALSE )
    , m_uiBufferSize( 0 )
{
}

TVertexPoolResource_PSP::~TVertexPoolResource_PSP()
{
	Destroy();
}

TBOOL TVertexPoolResource_PSP::Create( TUINT16 a_uiVertexSize, TUINT16 a_uiMaxVertices )
{
	Destroy();
	if ( a_uiVertexSize == 0 || a_uiMaxVertices == 0 )
		return TFALSE;

	const TSIZE uiBufferSize = TSIZE( a_uiVertexSize ) * a_uiMaxVertices;
	if ( !TRenderMemory_PSP::ReserveMain( uiBufferSize ) )
		return TFALSE;
	m_pData = static_cast<TBYTE*>( GetGlobalAllocator()->Malloc( uiBufferSize, TRenderBufferAlignment_PSP ) );
	if ( m_pData == TNULL )
	{
		TRenderMemory_PSP::ReleaseMain( uiBufferSize );
		return TFALSE;
	}

	m_uiVertexSize   = a_uiVertexSize;
	m_uiMaxVertices  = a_uiMaxVertices;
	m_uiNumVertices  = 0;
	m_bLocked        = TFALSE;
	m_uiBufferSize   = uiBufferSize;
	return TTRUE;
}

void TVertexPoolResource_PSP::Destroy()
{
	TASSERT( !m_bLocked );
	if ( m_pData != TNULL )
	{
		GetGlobalAllocator()->Free( m_pData );
		TRenderMemory_PSP::ReleaseMain( m_uiBufferSize );
	}

	m_pData          = TNULL;
	m_uiVertexSize   = 0;
	m_uiMaxVertices  = 0;
	m_uiNumVertices  = 0;
	m_bLocked        = TFALSE;
	m_uiBufferSize   = 0;
}

TBYTE* TVertexPoolResource_PSP::Lock()
{
	if ( m_pData == TNULL || m_bLocked )
		return TNULL;

	m_bLocked = TTRUE;
	return m_pData;
}

void TVertexPoolResource_PSP::Unlock( TUINT16 a_uiNumVertices )
{
	TASSERT( m_bLocked );
	TASSERT( a_uiNumVertices <= m_uiMaxVertices );
	if ( !m_bLocked )
		return;

	if ( a_uiNumVertices > m_uiMaxVertices )
		a_uiNumVertices = m_uiMaxVertices;

	m_uiNumVertices = a_uiNumVertices;
	m_bLocked       = TFALSE;
	sceKernelDcacheWritebackRange( m_pData, TSIZE( m_uiVertexSize ) * m_uiNumVertices );
}

TIndexPoolResource_PSP::TIndexPoolResource_PSP()
    : m_pData( TNULL )
    , m_uiMaxIndices( 0 )
    , m_uiNumIndices( 0 )
    , m_bLocked( TFALSE )
    , m_uiBufferSize( 0 )
{
}

TIndexPoolResource_PSP::~TIndexPoolResource_PSP()
{
	Destroy();
}

TBOOL TIndexPoolResource_PSP::Create( TUINT16 a_uiMaxIndices )
{
	Destroy();
	if ( a_uiMaxIndices == 0 )
		return TFALSE;

	const TSIZE uiBufferSize = TSIZE( a_uiMaxIndices ) * sizeof( TIndexType );
	if ( !TRenderMemory_PSP::ReserveMain( uiBufferSize ) )
		return TFALSE;
	m_pData = static_cast<TIndexType*>( GetGlobalAllocator()->Malloc( uiBufferSize, TRenderBufferAlignment_PSP ) );
	if ( m_pData == TNULL )
	{
		TRenderMemory_PSP::ReleaseMain( uiBufferSize );
		return TFALSE;
	}

	m_uiMaxIndices = a_uiMaxIndices;
	m_uiNumIndices = 0;
	m_bLocked      = TFALSE;
	m_uiBufferSize = uiBufferSize;
	return TTRUE;
}

void TIndexPoolResource_PSP::Destroy()
{
	TASSERT( !m_bLocked );
	if ( m_pData != TNULL )
	{
		GetGlobalAllocator()->Free( m_pData );
		TRenderMemory_PSP::ReleaseMain( m_uiBufferSize );
	}

	m_pData        = TNULL;
	m_uiMaxIndices = 0;
	m_uiNumIndices = 0;
	m_bLocked      = TFALSE;
	m_uiBufferSize = 0;
}

TIndexType* TIndexPoolResource_PSP::Lock()
{
	if ( m_pData == TNULL || m_bLocked )
		return TNULL;

	m_bLocked = TTRUE;
	return m_pData;
}

void TIndexPoolResource_PSP::Unlock( TUINT16 a_uiNumIndices )
{
	TASSERT( m_bLocked );
	TASSERT( a_uiNumIndices <= m_uiMaxIndices );
	if ( !m_bLocked )
		return;

	if ( a_uiNumIndices > m_uiMaxIndices )
		a_uiNumIndices = m_uiMaxIndices;

	m_uiNumIndices = a_uiNumIndices;
	m_bLocked      = TFALSE;
	sceKernelDcacheWritebackRange( m_pData, TSIZE( m_uiNumIndices ) * sizeof( TIndexType ) );
}

TOSHI_NAMESPACE_END
