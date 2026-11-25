#include "pch.h"
#include "TPSPTerrainVISDump.h"

#include "Terrain/ATerrainVIS.h"

#include <Plugins/PTRB.h>

#include <fstream>
#include <sstream>

TOSHI_NAMESPACE_USING

static void WriteJSONString_PSPTerrainVIS( std::ostream& a_rOutput, const TCHAR* a_pValue )
{
	a_rOutput << '"';
	for ( TSIZE i = 0; a_pValue != TNULL && a_pValue[ i ] != '\0'; i++ )
	{
		const TCHAR cCharacter = a_pValue[ i ];
		if ( cCharacter == '"' || cCharacter == '\\' )
			a_rOutput << '\\';
		a_rOutput << cCharacter;
	}
	a_rOutput << '"';
}

static const TCHAR* GetLODName_PSPTerrainVIS( ATerrainLODType a_eLODType )
{
	switch ( a_eLODType )
	{
		case ATerrainLODType_None: return "none";
		case ATerrainLODType_High: return "high";
		case ATerrainLODType_Low: return "low";
		default: return TNULL;
	}
}

static TBOOL WriteLODArray_PSPTerrainVIS(
	std::ostream& a_rOutput,
	ATerrainSection& a_rSection,
	ATerrainLODType a_eLODType
)
{
	const TINT iLODCount = a_rSection.GetLODCount( a_eLODType );
	if ( iLODCount < 0 || iLODCount > 128 )
		return TFALSE;

	const TCHAR** ppLODNames = TNULL;
	TINT iNameCount = 0;
	a_rSection.GetLODNames( a_eLODType, ppLODNames, iNameCount );
	if ( iNameCount != iLODCount || ( iLODCount > 0 && ppLODNames == TNULL ) )
		return TFALSE;

	a_rOutput << '[';
	for ( TINT i = 0; i < iLODCount; i++ )
	{
		if ( ppLODNames[ i ] == TNULL )
			return TFALSE;

		const ATerrainSection::LODFLAGS eFlags = a_rSection.GetLODFlags( a_eLODType, i );
		if ( i != 0 )
			a_rOutput << ',';
		a_rOutput << "\n        { \"name\": ";
		WriteJSONString_PSPTerrainVIS( a_rOutput, ppLODNames[ i ] );
		a_rOutput << ", \"flags\": " << TUINT( eFlags )
		          << ", \"glow\": "
		          << ( HASANYFLAG( eFlags, ATerrainSection::LODFLAGS_GLOW ) ? "true" : "false" )
		          << " }";
	}
	if ( iLODCount > 0 )
		a_rOutput << '\n' << "      ";
	a_rOutput << ']';
	return TTRUE;
}

TBOOL ExportPSPTerrainVISDump( const TCHAR* a_pInputPath, const TCHAR* a_pOutputPath )
{
	if ( a_pInputPath == TNULL || a_pOutputPath == TNULL )
		return TFALSE;

	PTRB oTRB;
	if ( !oTRB.ReadFromFile( a_pInputPath ) )
	{
		TERROR( "PSP terrain VIS dump could not read %s\n", a_pInputPath );
		return TFALSE;
	}

	auto oVIS = oTRB.GetSymbols()->Find<ATerrainVIS>( oTRB.GetSections(), "terrainvis" );
	if ( !oVIS || oVIS.get()->GetNumSections() <= 0 || oVIS.get()->GetNumSections() > 4096 ||
	     oVIS.get()->GetSections() == TNULL )
	{
		TERROR( "PSP terrain VIS dump rejected terrainvis\n" );
		return TFALSE;
	}

	std::ostringstream oOutput;
	oOutput << "{\n  \"version\": 2,\n  \"sections\": [\n";
	for ( TINT32 i = 0; i < oVIS.get()->GetNumSections(); i++ )
	{
		ATerrainSection& rSection = oVIS.get()->GetSections()[ i ];
		if ( rSection.GetName() == TNULL )
		{
			TERROR( "PSP terrain VIS dump rejected section %d name\n", i );
			return TFALSE;
		}

		oOutput << "    {\n      \"name\": ";
		WriteJSONString_PSPTerrainVIS( oOutput, rSection.GetName() );
		oOutput << ",\n      \"collision\": ";
		WriteJSONString_PSPTerrainVIS( oOutput, rSection.GetCollisionFilename() );
		oOutput << ",\n      \"material_libraries\": { \"high\": ";
		WriteJSONString_PSPTerrainVIS( oOutput, rSection.GetLODMaterialLibraryName( ATerrainLODType_High ) );
		oOutput << ", \"low\": ";
		WriteJSONString_PSPTerrainVIS( oOutput, rSection.GetLODMaterialLibraryName( ATerrainLODType_Low ) );
		oOutput << " }";
		oOutput << ",\n      \"high\": ";
		if ( !WriteLODArray_PSPTerrainVIS( oOutput, rSection, ATerrainLODType_High ) )
		{
			TERROR( "PSP terrain VIS dump rejected section %d high LODs\n", i );
			return TFALSE;
		}
		oOutput << ",\n      \"low\": ";
		if ( !WriteLODArray_PSPTerrainVIS( oOutput, rSection, ATerrainLODType_Low ) )
		{
			TERROR( "PSP terrain VIS dump rejected section %d low LODs\n", i );
			return TFALSE;
		}
		oOutput << ",\n      \"visibility\": [";
		for ( TINT32 k = 0; k < oVIS.get()->GetNumSections(); k++ )
		{
			const TCHAR* pLODName = GetLODName_PSPTerrainVIS( rSection.GetOtherSectionVisibility( k ) );
			if ( pLODName == TNULL )
			{
				TERROR( "PSP terrain VIS dump rejected section %d visibility %d\n", i, k );
				return TFALSE;
			}
			if ( k != 0 )
				oOutput << ", ";
			WriteJSONString_PSPTerrainVIS( oOutput, pLODName );
		}
		oOutput << ']';
		oOutput << "\n    }";
		if ( i + 1 != oVIS.get()->GetNumSections() )
			oOutput << ',';
		oOutput << '\n';
	}
	oOutput << "  ]\n}\n";

	std::ofstream oFile( a_pOutputPath, std::ios::out | std::ios::trunc );
	if ( !oFile )
	{
		TERROR( "PSP terrain VIS dump could not create %s\n", a_pOutputPath );
		return TFALSE;
	}
	oFile << oOutput.str();
	if ( !oFile.good() )
	{
		TERROR( "PSP terrain VIS dump could not write %s\n", a_pOutputPath );
		return TFALSE;
	}

	TINFO(
	    "PSP terrain VIS dump wrote %d sections to %s\n",
	    oVIS.get()->GetNumSections(),
	    a_pOutputPath
	);
	return TTRUE;
}
