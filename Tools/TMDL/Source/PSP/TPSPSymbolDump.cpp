#include "pch.h"
#include "TPSPSymbolDump.h"

#include <Plugins/PTRB.h>

#include <fstream>
#include <sstream>

TOSHI_NAMESPACE_USING

static void WriteJSONString_PSPSymbol( std::ostream& a_rOutput, T2StringView a_oValue )
{
	a_rOutput << '"';
	for ( TSIZE i = 0; i < a_oValue.Length(); i++ )
	{
		const TCHAR cCharacter = a_oValue[ i ];
		if ( cCharacter == '"' || cCharacter == '\\' )
			a_rOutput << '\\';
		a_rOutput << cCharacter;
	}
	a_rOutput << '"';
}

TBOOL ExportPSPSymbolDump( const TCHAR* a_pInputPath, const TCHAR* a_pOutputPath )
{
	if ( a_pInputPath == TNULL || a_pOutputPath == TNULL )
		return TFALSE;

	PTRB oTRB;
	if ( !oTRB.ReadFromFile( a_pInputPath ) )
	{
		TERROR( "PSP symbol dump could not read %s\n", a_pInputPath );
		return TFALSE;
	}
	PTRBSections* pSections = oTRB.GetSections();
	PTRBSymbols* pSymbols = oTRB.GetSymbols();
	const TUINT uiSymbolCount = pSymbols->GetCount();
	if ( uiSymbolCount > 4096 )
	{
		TERROR( "PSP symbol dump rejected %u symbols\n", uiSymbolCount );
		return TFALSE;
	}

	std::ostringstream oOutput;
	oOutput << "{\n  \"version\": 1,\n  \"symbols\": [\n";
	for ( TUINT i = 0; i < uiSymbolCount; i++ )
	{
		auto oSymbol = pSymbols->GetByIndex<TCHAR>( pSections, i );
		PTRBSections::MemoryStream* pStack = oSymbol.stack();
		oOutput << "    { \"name\": ";
		WriteJSONString_PSPSymbol( oOutput, pSymbols->GetName( i ) );
		oOutput << ", \"stream\": " << pStack->GetIndex()
		        << ", \"offset\": " << oSymbol.offset()
		        << ", \"stream_bytes\": " << pStack->GetUsedSize() << " }";
		if ( i + 1 != uiSymbolCount )
			oOutput << ',';
		oOutput << '\n';
	}
	oOutput << "  ]\n}\n";

	std::ofstream oFile( a_pOutputPath, std::ios::out | std::ios::trunc );
	if ( !oFile )
	{
		TERROR( "PSP symbol dump could not create %s\n", a_pOutputPath );
		return TFALSE;
	}
	oFile << oOutput.str();
	if ( !oFile.good() )
	{
		TERROR( "PSP symbol dump could not write %s\n", a_pOutputPath );
		return TFALSE;
	}
	TINFO( "PSP symbol dump wrote %u symbols to %s\n", uiSymbolCount, a_pOutputPath );
	return TTRUE;
}
