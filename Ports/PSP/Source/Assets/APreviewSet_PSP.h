#pragma once

#include <Toshi/Typedefs.h>

struct APreviewAssetSignature_PSP
{
	TUINT32 uiSize;
	TUINT32 uiCRC32;
};

struct APreviewSetHeader_PSP
{
	enum : TUINT32
	{
		MAGIC = 0x54455350,
		VERSION = 1,
		FLAG_TEXTURE = 1,
	};

	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TUINT32 uiFlags;
	TUINT32 uiHeaderSize;
	APreviewAssetSignature_PSP oMesh;
	APreviewAssetSignature_PSP oSkeleton;
	APreviewAssetSignature_PSP oAnimation;
	APreviewAssetSignature_PSP oTexture;
};

class APreviewSet_PSP
{
public:
	static TBOOL Validate( TBOOL& a_rbHasTexture );

private:
	static TBOOL ValidateAsset(
	    const TCHAR* a_pPath,
	    const APreviewAssetSignature_PSP& a_rSignature
	);
	static TUINT32 UpdateCRC32( TUINT32 a_uiCRC32, const TUINT8* a_pData, TSIZE a_uiSize );
};

static_assert( sizeof( APreviewSetHeader_PSP ) == 48, "PSET header must stay fixed-width" );
