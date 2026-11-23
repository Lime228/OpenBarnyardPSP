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

static void WriteJSONString_PSP( std::ostream& a_rOutput, const TCHAR* a_pValue )
{
	a_rOutput << '"';
	for ( const TCHAR* pCharacter = a_pValue; pCharacter != TNULL && *pCharacter != '\0'; pCharacter++ )
	{
		switch ( *pCharacter )
		{
			case '"': a_rOutput << "\\\""; break;
			case '\\': a_rOutput << "\\\\"; break;
			case '\n': a_rOutput << "\\n"; break;
			case '\r': a_rOutput << "\\r"; break;
			case '\t': a_rOutput << "\\t"; break;
			default: a_rOutput << *pCharacter; break;
		}
	}
	a_rOutput << '"';
}

static TPSPTextureLibraryDump* OpenPSPTextureLibrary(
    const TCHAR* a_pLibraryPath,
    const TCHAR* a_pLibrarySymbol,
    PTRB& a_rTRB
)
{
	if ( a_pLibraryPath == TNULL || a_pLibrarySymbol == TNULL || a_pLibrarySymbol[ 0 ] == '\0' ||
	     !a_rTRB.ReadFromFile( a_pLibraryPath ) )
	{
		TERROR( "PSP texture dump could not read %s\n", a_pLibraryPath );
		return TNULL;
	}

	auto oLibrary = a_rTRB.GetSymbols()->Find<TPSPTextureLibraryDump>(
	    a_rTRB.GetSections(), a_pLibrarySymbol
	);
	if ( !oLibrary || oLibrary.get()->iNumTextures <= 0 || oLibrary.get()->iNumTextures > 1024 ||
	     oLibrary.get()->pTextureInfos == TNULL )
	{
		TERROR( "PSP texture dump rejected TTL header %s\n", a_pLibrarySymbol );
		return TNULL;
	}

	return oLibrary.get();
}

TBOOL ExportPSPTextureDump(
    const TCHAR* a_pLibraryPath,
    const TCHAR* a_pLibrarySymbol,
    const TCHAR* a_pTextureName,
    const TCHAR* a_pOutputPath
)
{
	if ( a_pLibraryPath == TNULL || a_pLibrarySymbol == TNULL || a_pLibrarySymbol[ 0 ] == '\0' ||
	     a_pTextureName == TNULL || a_pOutputPath == TNULL )
		return TFALSE;

	PTRB oTRB;
	auto pLibrary = OpenPSPTextureLibrary( a_pLibraryPath, a_pLibrarySymbol, oTRB );
	if ( pLibrary == TNULL )
		return TFALSE;

	const TPSPTextureInfoDump* pSelectedTexture = TNULL;
	for ( TINT i = 0; i < pLibrary->iNumTextures; i++ )
	{
		const TPSPTextureInfoDump& rTexture = pLibrary->pTextureInfos[ i ];
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
		for ( TINT i = 0; i < pLibrary->iNumTextures; i++ )
		{
			const TCHAR* pName = pLibrary->pTextureInfos[ i ].szFileName;
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

TBOOL ExportPSPTextureInventory(
    const TCHAR* a_pLibraryPath,
    const TCHAR* a_pLibrarySymbol,
    const TCHAR* a_pOutputPath
)
{
	if ( a_pOutputPath == TNULL )
		return TFALSE;

	PTRB oTRB;
	auto pLibrary = OpenPSPTextureLibrary( a_pLibraryPath, a_pLibrarySymbol, oTRB );
	if ( pLibrary == TNULL )
		return TFALSE;

	std::ofstream oOutput( a_pOutputPath, std::ios::out | std::ios::trunc );
	if ( !oOutput )
		return TFALSE;

	oOutput << "{\n  \"version\": 1,\n  \"pack_name\": ";
	WriteJSONString_PSP( oOutput, pLibrary->szPackName );
	oOutput << ",\n  \"texture_count\": " << pLibrary->iNumTextures
	        << ",\n  \"textures\": [\n";
	for ( TINT i = 0; i < pLibrary->iNumTextures; i++ )
	{
		const TPSPTextureInfoDump& rTexture = pLibrary->pTextureInfos[ i ];
		if ( rTexture.szFileName == TNULL )
		{
			TERROR( "PSP texture inventory rejected texture %d\n", i );
			return TFALSE;
		}

		oOutput << "    {\"index\": " << i << ", \"name\": ";
		WriteJSONString_PSP( oOutput, rTexture.szFileName );
		oOutput << ", \"format\": " << TUINT( rTexture.eFormat )
		        << ", \"bytes\": " << rTexture.uiDataSize << '}';
		if ( i + 1 != pLibrary->iNumTextures )
			oOutput << ',';
		oOutput << '\n';
	}
	oOutput << "  ]\n}\n";
	return oOutput.good();
}
