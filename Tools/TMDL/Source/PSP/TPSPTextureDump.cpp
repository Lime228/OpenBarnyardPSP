#include "pch.h"
#include "TPSPTextureDump.h"

#include <Plugins/PTRB.h>
#include <Render/TTexture.h>

#include <fstream>

TOSHI_NAMESPACE_USING

struct TPSPTextureInfoDump
{
	TTEXTURE_FORMAT eFormat;
	const TCHAR*    szFileName;
	TUINT           uiDataSize;
	void*           pData;
};

struct TPSPTextureLibraryDump
{
	TINT                  iNumTextures;
	TPSPTextureInfoDump* pTextureInfos;
	const TCHAR*          szPackName;
};

TBOOL ExportPSPTextureDump(
    const TCHAR* a_pLibraryPath,
    const TCHAR* a_pTextureName,
    const TCHAR* a_pOutputPath
)
{
	if ( a_pLibraryPath == TNULL || a_pTextureName == TNULL || a_pOutputPath == TNULL )
		return TFALSE;

	PTRB oTRB;
	if ( !oTRB.ReadFromFile( a_pLibraryPath ) )
	{
		TERROR( "PSP texture dump could not read %s\n", a_pLibraryPath );
		return TFALSE;
	}

	auto oLibrary = oTRB.GetSymbols()->Find<TPSPTextureLibraryDump>( oTRB.GetSections(), "TTL" );
	if ( !oLibrary || oLibrary.get()->iNumTextures <= 0 || oLibrary.get()->iNumTextures > 1024 ||
	     oLibrary.get()->pTextureInfos == TNULL )
	{
		TERROR( "PSP texture dump rejected TTL header\n" );
		return TFALSE;
	}

	const TPSPTextureInfoDump* pSelectedTexture = TNULL;
	for ( TINT i = 0; i < oLibrary.get()->iNumTextures; i++ )
	{
		const TPSPTextureInfoDump& rTexture = oLibrary.get()->pTextureInfos[ i ];
		if ( rTexture.szFileName != TNULL &&
		     TStringManager::String8CompareNoCase( rTexture.szFileName, a_pTextureName ) == 0 )
		{
			pSelectedTexture = &rTexture;
			break;
		}
	}

	if ( pSelectedTexture == TNULL )
	{
		TERROR( "PSP texture dump could not find %s; available textures:\n", a_pTextureName );
		for ( TINT i = 0; i < oLibrary.get()->iNumTextures; i++ )
		{
			const TCHAR* pName = oLibrary.get()->pTextureInfos[ i ].szFileName;
			TERROR( "  %s\n", pName != TNULL ? pName : "<invalid>" );
		}
		return TFALSE;
	}

	if ( pSelectedTexture->eFormat != TTEX_FMT_WIN_DDS || pSelectedTexture->pData == TNULL ||
	     pSelectedTexture->uiDataSize < 128 || pSelectedTexture->uiDataSize > 16 * 1024 * 1024 ||
	     TUtil::MemCompare( pSelectedTexture->pData, "DDS ", 4 ) != 0 )
	{
		TERROR(
		    "PSP texture dump rejected %s (format=%u bytes=%u)\n",
		    a_pTextureName,
		    TUINT( pSelectedTexture->eFormat ),
		    pSelectedTexture->uiDataSize
		);
		return TFALSE;
	}

	std::ofstream oFile( a_pOutputPath, std::ios::binary | std::ios::out | std::ios::trunc );
	if ( !oFile )
	{
		TERROR( "PSP texture dump could not create %s\n", a_pOutputPath );
		return TFALSE;
	}
	oFile.write(
	    static_cast<const char*>( pSelectedTexture->pData ),
	    static_cast<std::streamsize>( pSelectedTexture->uiDataSize )
	);
	if ( !oFile.good() )
	{
		TERROR( "PSP texture dump could not write %s\n", a_pOutputPath );
		return TFALSE;
	}

	TINFO(
	    "PSP texture dump wrote %s (%u bytes) to %s\n",
	    pSelectedTexture->szFileName,
	    pSelectedTexture->uiDataSize,
	    a_pOutputPath
	);
	return TTRUE;
}
