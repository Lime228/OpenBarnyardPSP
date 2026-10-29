#include "ToshiPCH.h"
#include "T2Texture_PSP.h"

#include "TRenderMemory_PSP.h"
#include "Toshi/T2Allocator.h"

#include <pspkernel.h>

#include <cstring>

TOSHI_NAMESPACE_START

static TBOOL T2TextureIsPowerOfTwo_PSP( TUINT32 a_uiValue )
{
	return a_uiValue != 0 && ( a_uiValue & ( a_uiValue - 1 ) ) == 0;
}

T2Texture::T2Texture()
    : m_pData( TNULL )
    , m_uiDataSize( 0 )
    , m_pImageData( TNULL )
    , m_uiWidth( 0 )
    , m_uiHeight( 0 )
    , m_uiBufferWidth( 0 )
    , m_uiFormat( T2TEXTUREFORMAT_PSP_RGBA8888 )
    , m_uiImageDataSize( 0 )
{
}

T2Texture::~T2Texture()
{
	Unload();
}

void T2Texture::SetData( const void* a_pData, TUINT a_uiDataSize )
{
	m_pData      = a_pData;
	m_uiDataSize = a_uiDataSize;
}

void T2Texture::GetData( const void*& a_rData, TUINT& a_rDataSize ) const
{
	a_rData     = m_pData;
	a_rDataSize = m_uiDataSize;
}

TBOOL T2Texture::Load()
{
	Unload();

	if ( m_pData == TNULL || m_uiDataSize < sizeof( T2TextureDataHeader_PSP ) )
		return TFALSE;

	const T2TextureDataHeader_PSP* pHeader = static_cast<const T2TextureDataHeader_PSP*>( m_pData );
	const TBOOL bKnownFormat = pHeader->uiFormat == T2TEXTUREFORMAT_PSP_RGBA8888 ||
	                          pHeader->uiFormat == T2TEXTUREFORMAT_PSP_RGBA8888_SWIZZLED;
	const TBOOL bValidSwizzle = pHeader->uiFormat != T2TEXTUREFORMAT_PSP_RGBA8888_SWIZZLED ||
	                            ( ( pHeader->uiBufferWidth * sizeof( TUINT32 ) ) % 16 == 0 &&
	                              pHeader->uiHeight % 8 == 0 );
	if ( pHeader->uiMagic != T2TextureDataHeader_PSP::MAGIC ||
	     pHeader->uiVersion != T2TextureDataHeader_PSP::VERSION ||
	     !bKnownFormat ||
	     !bValidSwizzle ||
	     !T2TextureIsPowerOfTwo_PSP( pHeader->uiWidth ) ||
	     !T2TextureIsPowerOfTwo_PSP( pHeader->uiHeight ) ||
	     !T2TextureIsPowerOfTwo_PSP( pHeader->uiBufferWidth ) ||
	     pHeader->uiWidth > 512 ||
	     pHeader->uiHeight > 512 ||
	     pHeader->uiBufferWidth < pHeader->uiWidth )
	{
		return TFALSE;
	}

	const TSIZE uiPixelDataSize = TSIZE( pHeader->uiBufferWidth ) * pHeader->uiHeight * sizeof( TUINT32 );
	if ( m_uiDataSize != sizeof( T2TextureDataHeader_PSP ) + uiPixelDataSize )
		return TFALSE;
	if ( !TRenderMemory_PSP::ReserveMain( uiPixelDataSize ) )
		return TFALSE;

	m_pImageData = GetGlobalAllocator()->Malloc( uiPixelDataSize, 16 );
	if ( m_pImageData == TNULL )
	{
		TRenderMemory_PSP::ReleaseMain( uiPixelDataSize );
		return TFALSE;
	}

	const TUINT8* pPixelData = static_cast<const TUINT8*>( m_pData ) + sizeof( T2TextureDataHeader_PSP );
	std::memcpy( m_pImageData, pPixelData, uiPixelDataSize );
	sceKernelDcacheWritebackRange( m_pImageData, uiPixelDataSize );

	m_uiWidth       = pHeader->uiWidth;
	m_uiHeight      = pHeader->uiHeight;
	m_uiBufferWidth = pHeader->uiBufferWidth;
	m_uiFormat      = pHeader->uiFormat;
	m_uiImageDataSize = uiPixelDataSize;
	return TTRUE;
}

void T2Texture::Unload()
{
	if ( m_pImageData != TNULL )
	{
		GetGlobalAllocator()->Free( m_pImageData );
		m_pImageData = TNULL;
		TRenderMemory_PSP::ReleaseMain( m_uiImageDataSize );
	}

	m_uiWidth       = 0;
	m_uiHeight      = 0;
	m_uiBufferWidth = 0;
	m_uiFormat      = T2TEXTUREFORMAT_PSP_RGBA8888;
	m_uiImageDataSize = 0;
}

TOSHI_NAMESPACE_END
