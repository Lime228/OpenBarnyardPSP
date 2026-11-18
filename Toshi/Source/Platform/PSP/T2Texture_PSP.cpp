#include "ToshiPCH.h"
#include "T2Texture_PSP.h"

#include "TRenderMemory_PSP.h"
#include "File/TFile.h"
#include "Toshi/T2Allocator.h"

#include <pspkernel.h>

#include <cstring>

TOSHI_NAMESPACE_START

static TBOOL T2TextureIsPowerOfTwo_PSP( TUINT32 a_uiValue )
{
	return a_uiValue != 0 && ( a_uiValue & ( a_uiValue - 1 ) ) == 0;
}

static TBOOL T2TextureValidateHeader_PSP( const T2TextureDataHeader_PSP& a_rHeader, TSIZE a_uiDataSize )
{
	const TBOOL bKnownFormat = a_rHeader.uiFormat == T2TEXTUREFORMAT_PSP_RGBA8888 ||
	                          a_rHeader.uiFormat == T2TEXTUREFORMAT_PSP_RGBA8888_SWIZZLED;
	const TBOOL bValidSwizzle = a_rHeader.uiFormat != T2TEXTUREFORMAT_PSP_RGBA8888_SWIZZLED ||
	                            ( ( a_rHeader.uiBufferWidth * sizeof( TUINT32 ) ) % 16 == 0 &&
	                              a_rHeader.uiHeight % 8 == 0 );
	if ( a_rHeader.uiMagic != T2TextureDataHeader_PSP::MAGIC ||
	     a_rHeader.uiVersion != T2TextureDataHeader_PSP::VERSION ||
	     !bKnownFormat ||
	     !bValidSwizzle ||
	     !T2TextureIsPowerOfTwo_PSP( a_rHeader.uiWidth ) ||
	     !T2TextureIsPowerOfTwo_PSP( a_rHeader.uiHeight ) ||
	     !T2TextureIsPowerOfTwo_PSP( a_rHeader.uiBufferWidth ) ||
	     a_rHeader.uiWidth > 512 ||
	     a_rHeader.uiHeight > 512 ||
	     a_rHeader.uiBufferWidth < a_rHeader.uiWidth )
	{
		return TFALSE;
	}

	const TSIZE uiPixelDataSize = TSIZE( a_rHeader.uiBufferWidth ) * a_rHeader.uiHeight * sizeof( TUINT32 );
	return a_uiDataSize == sizeof( T2TextureDataHeader_PSP ) + uiPixelDataSize;
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
	if ( !PrepareImage_PSP( *pHeader, m_uiDataSize ) )
		return TFALSE;

	const TUINT8* pPixelData = static_cast<const TUINT8*>( m_pData ) + sizeof( T2TextureDataHeader_PSP );
	std::memcpy( m_pImageData, pPixelData, m_uiImageDataSize );
	sceKernelDcacheWritebackRange( m_pImageData, m_uiImageDataSize );
	return TTRUE;
}

TBOOL T2Texture::LoadFromFile( TFile& a_rFile )
{
	Unload();
	T2TextureDataHeader_PSP oHeader;
	const TSIZE uiDataSize = a_rFile.GetSize();
	if ( a_rFile.Read( &oHeader, sizeof( oHeader ) ) != sizeof( oHeader ) ||
	     !PrepareImage_PSP( oHeader, uiDataSize ) )
	{
		return TFALSE;
	}

	if ( a_rFile.Read( m_pImageData, m_uiImageDataSize ) != m_uiImageDataSize )
	{
		Unload();
		return TFALSE;
	}

	sceKernelDcacheWritebackRange( m_pImageData, m_uiImageDataSize );
	return TTRUE;
}

TBOOL T2Texture::PrepareImage_PSP( const T2TextureDataHeader_PSP& a_rHeader, TSIZE a_uiDataSize )
{
	Unload();
	if ( !T2TextureValidateHeader_PSP( a_rHeader, a_uiDataSize ) )
		return TFALSE;

	const TSIZE uiPixelDataSize = TSIZE( a_rHeader.uiBufferWidth ) * a_rHeader.uiHeight * sizeof( TUINT32 );
	if ( !TRenderMemory_PSP::ReserveMain( uiPixelDataSize ) )
		return TFALSE;

	m_pImageData = GetGlobalAllocator()->Malloc( uiPixelDataSize, 16 );
	if ( m_pImageData == TNULL )
	{
		TRenderMemory_PSP::ReleaseMain( uiPixelDataSize );
		return TFALSE;
	}

	m_uiWidth       = a_rHeader.uiWidth;
	m_uiHeight      = a_rHeader.uiHeight;
	m_uiBufferWidth = a_rHeader.uiBufferWidth;
	m_uiFormat      = a_rHeader.uiFormat;
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
