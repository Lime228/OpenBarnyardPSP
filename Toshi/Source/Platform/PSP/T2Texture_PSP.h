#pragma once

#include "Toshi/Defines.h"

TOSHI_NAMESPACE_START

enum T2TextureFormat_PSP : TUINT32
{
	T2TEXTUREFORMAT_PSP_RGBA8888          = 0,
	T2TEXTUREFORMAT_PSP_RGBA8888_SWIZZLED = 1,
};

struct T2TextureDataHeader_PSP
{
	static constexpr TUINT32 MAGIC   = 0x58455450;
	static constexpr TUINT32 VERSION = 1;

	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TUINT32 uiWidth;
	TUINT32 uiHeight;
	TUINT32 uiBufferWidth;
	TUINT32 uiFormat;
};

static_assert( sizeof( T2TextureDataHeader_PSP ) == 24, "Unexpected PSP texture header layout" );

class T2Texture
{
public:
	T2Texture();
	~T2Texture();

	void SetData( const void* a_pData, TUINT a_uiDataSize );
	void GetData( const void*& a_rData, TUINT& a_rDataSize ) const;

	TBOOL Load();
	void  Unload();

	TUINT GetWidth() const { return m_uiWidth; }
	TUINT GetHeight() const { return m_uiHeight; }
	TUINT GetBufferWidth() const { return m_uiBufferWidth; }
	TUINT GetMipLevels() const { return 1; }
	TUINT GetFormat() const { return m_uiFormat; }
	TBOOL IsSwizzled() const { return m_uiFormat == T2TEXTUREFORMAT_PSP_RGBA8888_SWIZZLED; }
	void* GetImageData() const { return m_pImageData; }

private:
	const void* m_pData;
	TUINT       m_uiDataSize;
	void*       m_pImageData;
	TUINT       m_uiWidth;
	TUINT       m_uiHeight;
	TUINT       m_uiBufferWidth;
	TUINT       m_uiFormat;
};

TOSHI_NAMESPACE_END
