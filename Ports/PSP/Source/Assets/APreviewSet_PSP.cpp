#include "APreviewSet_PSP.h"

#include <File/TFile.h>

static constexpr TSIZE s_uiPreviewCRCBufferSize_PSP = 1024;
static constexpr TSIZE s_uiPreviewMaximumAssetSize_PSP = 4 * 1024 * 1024;

TUINT32 APreviewSet_PSP::UpdateCRC32(
    TUINT32 a_uiCRC32,
    const TUINT8* a_pData,
    TSIZE a_uiSize
)
{
	for ( TSIZE i = 0; i < a_uiSize; i++ )
	{
		a_uiCRC32 ^= a_pData[ i ];
		for ( TUINT32 uiBit = 0; uiBit < 8; uiBit++ )
		{
			const TUINT32 uiMask = 0 - ( a_uiCRC32 & 1 );
			a_uiCRC32 = ( a_uiCRC32 >> 1 ) ^ ( 0xedb88320u & uiMask );
		}
	}
	return a_uiCRC32;
}

TBOOL APreviewSet_PSP::ValidateAsset(
    const TCHAR* a_pPath,
    const APreviewAssetSignature_PSP& a_rSignature
)
{
	if ( a_rSignature.uiSize == 0 || a_rSignature.uiSize > s_uiPreviewMaximumAssetSize_PSP )
		return TFALSE;

	Toshi::TFile* pFile = Toshi::TFile::Create(
	    a_pPath,
	    Toshi::TFILEMODE_READ | Toshi::TFILEMODE_NOBUFFER
	);
	if ( pFile == TNULL )
		return TFALSE;

	TBOOL bValid = pFile->GetSize() == TSIZE( a_rSignature.uiSize );
	TUINT32 uiCRC32 = 0xffffffff;
	TUINT8 aBuffer[ s_uiPreviewCRCBufferSize_PSP ];
	TSIZE uiRemaining = a_rSignature.uiSize;
	while ( bValid && uiRemaining != 0 )
	{
		const TSIZE uiReadSize = uiRemaining < s_uiPreviewCRCBufferSize_PSP
		    ? uiRemaining
		    : s_uiPreviewCRCBufferSize_PSP;
		if ( pFile->Read( aBuffer, uiReadSize ) != uiReadSize )
		{
			bValid = TFALSE;
			break;
		}
		uiCRC32 = UpdateCRC32( uiCRC32, aBuffer, uiReadSize );
		uiRemaining -= uiReadSize;
	}

	pFile->Destroy();
	return bValid && ( uiCRC32 ^ 0xffffffff ) == a_rSignature.uiCRC32;
}

TBOOL APreviewSet_PSP::Validate( TBOOL& a_rbHasTexture )
{
	a_rbHasTexture = TFALSE;
	Toshi::TFile* pFile = Toshi::TFile::Create(
	    "local:barnyard_preview.pset",
	    Toshi::TFILEMODE_READ | Toshi::TFILEMODE_NOBUFFER
	);
	if ( pFile == TNULL )
		return TFALSE;

	APreviewSetHeader_PSP oHeader = {};
	const TBOOL bHeaderRead =
	    pFile->GetSize() == sizeof( oHeader ) &&
	    pFile->Read( &oHeader, sizeof( oHeader ) ) == sizeof( oHeader );
	pFile->Destroy();
	if ( !bHeaderRead ||
	     oHeader.uiMagic != APreviewSetHeader_PSP::MAGIC ||
	     oHeader.uiVersion != APreviewSetHeader_PSP::VERSION ||
	     oHeader.uiHeaderSize != sizeof( oHeader ) ||
	     ( oHeader.uiFlags & ~APreviewSetHeader_PSP::FLAG_TEXTURE ) != 0 )
	{
		return TFALSE;
	}

	const TBOOL bHasTexture = ( oHeader.uiFlags & APreviewSetHeader_PSP::FLAG_TEXTURE ) != 0;
	if ( !bHasTexture && ( oHeader.oTexture.uiSize != 0 || oHeader.oTexture.uiCRC32 != 0 ) )
		return TFALSE;

	if ( !ValidateAsset( "local:barnyard_preview.pmes", oHeader.oMesh ) ||
	     !ValidateAsset( "local:barnyard_preview.pskl", oHeader.oSkeleton ) ||
	     !ValidateAsset( "local:barnyard_preview.panm", oHeader.oAnimation ) ||
	     ( bHasTexture && !ValidateAsset( "local:barnyard_preview.ptex", oHeader.oTexture ) ) )
	{
		return TFALSE;
	}

	a_rbHasTexture = bHasTexture;
	return TTRUE;
}
