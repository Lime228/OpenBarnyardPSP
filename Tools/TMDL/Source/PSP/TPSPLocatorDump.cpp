#include "pch.h"
#include "TPSPLocatorDump.h"

#include <Plugins/PTRB.h>

#include <Math/TQuaternion.h>
#include <Math/TVector3.h>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>

TOSHI_NAMESPACE_USING

struct TerrainLocator_PSPDump
{
	TQuaternion quatRotation;
	TVector3    vecPosition;
	TUINT16     uiNameId;
	TUINT8      uiFlags1;
	TINT8       iFlags2;

	TFLOAT GetScale() const { return ( uiFlags1 & 0xF ) * 0.2f; }
};

struct TerrainLocatorHeader_PSPDump
{
	TINT32                   iNumLocators;
	TerrainLocator_PSPDump*  pLocators;
	TUINT32                  uiNameCount;
	const TCHAR**            ppNames;
};

struct TerrainLocatorVISSection_PSPDump
{
	const TCHAR* pName;
	TUINT32      uiFirstLocator;
	TUINT32      uiNumLocators;
};

struct TerrainLocatorVISHeader_PSPDump
{
	TINT32                            iNumSections;
	TerrainLocatorVISSection_PSPDump* pSections;
};

static void WriteJSONString_PSPLocator( std::ostream& a_rOutput, const TCHAR* a_pValue )
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

static TBOOL IsFinite_PSPLocator( const TerrainLocator_PSPDump& a_rLocator )
{
	return std::isfinite( a_rLocator.quatRotation.x ) &&
	       std::isfinite( a_rLocator.quatRotation.y ) &&
	       std::isfinite( a_rLocator.quatRotation.z ) &&
	       std::isfinite( a_rLocator.quatRotation.w ) &&
	       std::isfinite( a_rLocator.vecPosition.x ) &&
	       std::isfinite( a_rLocator.vecPosition.y ) &&
	       std::isfinite( a_rLocator.vecPosition.z );
}

static TBOOL LoadPSPLocatorHeaders(
	PTRB& a_rTRB,
	const TCHAR* a_pInputPath,
	TerrainLocatorHeader_PSPDump*& a_rLocatorHeader,
	TerrainLocatorVISHeader_PSPDump*& a_rVISHeader,
	TBOOL a_bRequireVIS
)
{
	if ( a_pInputPath == TNULL || !a_rTRB.ReadFromFile( a_pInputPath ) )
	{
		TERROR( "PSP locator dump could not read %s\n", a_pInputPath );
		return TFALSE;
	}

	auto oHeader = a_rTRB.GetSymbols()->Find<TerrainLocatorHeader_PSPDump>(
		a_rTRB.GetSections(),
		"Locators"
	);
	if ( !oHeader || oHeader.get()->iNumLocators <= 0 || oHeader.get()->iNumLocators > 65535 ||
	     oHeader.get()->pLocators == TNULL || oHeader.get()->uiNameCount == 0 ||
	     oHeader.get()->uiNameCount > 65535 || oHeader.get()->ppNames == TNULL )
	{
		TERROR( "PSP locator dump rejected the locator table\n" );
		return TFALSE;
	}

	a_rVISHeader = TNULL;
	if ( a_bRequireVIS )
	{
		auto oVISHeader = a_rTRB.GetSymbols()->Find<TerrainLocatorVISHeader_PSPDump>(
			a_rTRB.GetSections(),
			"LocatorVIS"
		);
		if ( !oVISHeader || oVISHeader.get()->iNumSections <= 0 ||
		     oVISHeader.get()->iNumSections > 4096 || oVISHeader.get()->pSections == TNULL )
		{
			TERROR( "PSP locator dump rejected the locator VIS table\n" );
			return TFALSE;
		}

		a_rVISHeader = oVISHeader.get();
	}

	a_rLocatorHeader = oHeader.get();
	return TTRUE;
}

TBOOL ExportPSPLocatorDump(
    const TCHAR* a_pInputPath,
    const TCHAR* a_pLocatorName,
    const TCHAR* a_pOutputPath
)
{
	if ( a_pInputPath == TNULL || a_pLocatorName == TNULL || a_pLocatorName[ 0 ] == '\0' ||
	     a_pOutputPath == TNULL )
		return TFALSE;

	PTRB                              oTRB;
	TerrainLocatorHeader_PSPDump*    pHeader = TNULL;
	TerrainLocatorVISHeader_PSPDump* pVISHeader = TNULL;
	if ( !LoadPSPLocatorHeaders( oTRB, a_pInputPath, pHeader, pVISHeader, TFALSE ) )
		return TFALSE;

	TerrainLocator_PSPDump* pSelectedLocator = TNULL;
	const TCHAR* pSelectedName = TNULL;
	for ( TINT32 i = 0; i < pHeader->iNumLocators; i++ )
	{
		TerrainLocator_PSPDump& rLocator = pHeader->pLocators[ i ];
		if ( rLocator.uiNameId >= pHeader->uiNameCount )
		{
			TERROR( "PSP locator dump rejected locator %d name index\n", i );
			return TFALSE;
		}

		const TCHAR* pName = pHeader->ppNames[ rLocator.uiNameId ];
		if ( pName == TNULL )
		{
			TERROR( "PSP locator dump rejected locator %d name\n", i );
			return TFALSE;
		}
		if ( TStringManager::String8CompareNoCase( pName, a_pLocatorName ) == 0 )
		{
			pSelectedLocator = &rLocator;
			pSelectedName = pName;
			break;
		}
	}

	if ( pSelectedLocator == TNULL || !IsFinite_PSPLocator( *pSelectedLocator ) ||
	     pSelectedLocator->quatRotation.MagnitudeSq() <= TMath::TFLOAT_EPSILON )
	{
		TERROR( "PSP locator dump could not find a valid locator named %s\n", a_pLocatorName );
		return TFALSE;
	}

	std::ostringstream oOutput;
	oOutput.imbue( std::locale::classic() );
	oOutput << std::setprecision( 9 );
	oOutput << "{\n  \"version\": 1,\n  \"name\": ";
	WriteJSONString_PSPLocator( oOutput, pSelectedName );
	oOutput << ",\n  \"rotation\": ["
	        << pSelectedLocator->quatRotation.x << ", "
	        << pSelectedLocator->quatRotation.y << ", "
	        << pSelectedLocator->quatRotation.z << ", "
	        << pSelectedLocator->quatRotation.w << "],\n"
	        << "  \"position\": ["
	        << pSelectedLocator->vecPosition.x << ", "
	        << pSelectedLocator->vecPosition.y << ", "
	        << pSelectedLocator->vecPosition.z << "],\n"
	        << "  \"scale\": " << pSelectedLocator->GetScale() << ",\n"
	        << "  \"flags1\": " << TUINT( pSelectedLocator->uiFlags1 >> 4 ) << ",\n"
	        << "  \"flags2\": " << TINT( pSelectedLocator->iFlags2 ) << "\n}\n";

	std::ofstream oFile( a_pOutputPath, std::ios::out | std::ios::trunc );
	if ( !oFile )
	{
		TERROR( "PSP locator dump could not create %s\n", a_pOutputPath );
		return TFALSE;
	}
	oFile << oOutput.str();
	if ( !oFile.good() )
	{
		TERROR( "PSP locator dump could not write %s\n", a_pOutputPath );
		return TFALSE;
	}

	TINFO( "PSP locator dump wrote %s to %s\n", pSelectedName, a_pOutputPath );
	return TTRUE;
}

TBOOL ExportPSPLocatorTableDump(
	const TCHAR* a_pInputPath,
	const TCHAR* a_pOutputPath
)
{
	if ( a_pInputPath == TNULL || a_pOutputPath == TNULL )
		return TFALSE;

	PTRB                              oTRB;
	TerrainLocatorHeader_PSPDump*    pHeader = TNULL;
	TerrainLocatorVISHeader_PSPDump* pVISHeader = TNULL;
	if ( !LoadPSPLocatorHeaders( oTRB, a_pInputPath, pHeader, pVISHeader, TTRUE ) )
		return TFALSE;

	for ( TINT32 i = 0; i < pHeader->iNumLocators; i++ )
	{
		const TerrainLocator_PSPDump& rLocator = pHeader->pLocators[ i ];
		if ( rLocator.uiNameId >= pHeader->uiNameCount ||
		     pHeader->ppNames[ rLocator.uiNameId ] == TNULL ||
		     !IsFinite_PSPLocator( rLocator ) ||
		     rLocator.quatRotation.MagnitudeSq() <= TMath::TFLOAT_EPSILON )
		{
			TERROR( "PSP locator table dump rejected locator %d\n", i );
			return TFALSE;
		}
	}

	for ( TINT32 i = 0; i < pVISHeader->iNumSections; i++ )
	{
		const TerrainLocatorVISSection_PSPDump& rSection = pVISHeader->pSections[ i ];
		if ( rSection.pName == TNULL || rSection.uiFirstLocator > TUINT32( pHeader->iNumLocators ) ||
		     rSection.uiNumLocators > TUINT32( pHeader->iNumLocators ) - rSection.uiFirstLocator )
		{
			TERROR( "PSP locator table dump rejected VIS section %d\n", i );
			return TFALSE;
		}
	}

	std::ostringstream oOutput;
	oOutput.imbue( std::locale::classic() );
	oOutput << std::setprecision( 9 );
	oOutput << "{\n  \"version\": 1,\n  \"locator_count\": " << pHeader->iNumLocators
	        << ",\n  \"locators\": [\n";
	for ( TINT32 i = 0; i < pHeader->iNumLocators; i++ )
	{
		const TerrainLocator_PSPDump& rLocator = pHeader->pLocators[ i ];
		oOutput << "    {\"index\": " << i << ", \"name\": ";
		WriteJSONString_PSPLocator( oOutput, pHeader->ppNames[ rLocator.uiNameId ] );
		oOutput << ", \"rotation\": ["
		        << rLocator.quatRotation.x << ", " << rLocator.quatRotation.y << ", "
		        << rLocator.quatRotation.z << ", " << rLocator.quatRotation.w << "], "
		        << "\"position\": [" << rLocator.vecPosition.x << ", "
		        << rLocator.vecPosition.y << ", " << rLocator.vecPosition.z << "], "
		        << "\"scale\": " << rLocator.GetScale() << ", "
		        << "\"scale_index\": " << TUINT( rLocator.uiFlags1 & 0xF ) << ", "
		        << "\"yaw_index\": " << TUINT( rLocator.uiFlags1 >> 4 ) << ", "
		        << "\"raw_flags1\": " << TUINT( rLocator.uiFlags1 ) << ", "
		        << "\"flags2\": " << TINT( rLocator.iFlags2 ) << "}"
		        << ( i + 1 == pHeader->iNumLocators ? "\n" : ",\n" );
	}
	oOutput << "  ],\n  \"vis_section_count\": " << pVISHeader->iNumSections
	        << ",\n  \"vis_sections\": [\n";
	for ( TINT32 i = 0; i < pVISHeader->iNumSections; i++ )
	{
		const TerrainLocatorVISSection_PSPDump& rSection = pVISHeader->pSections[ i ];
		oOutput << "    {\"index\": " << i << ", \"name\": ";
		WriteJSONString_PSPLocator( oOutput, rSection.pName );
		oOutput << ", \"first_locator\": " << rSection.uiFirstLocator
		        << ", \"locator_count\": " << rSection.uiNumLocators << "}"
		        << ( i + 1 == pVISHeader->iNumSections ? "\n" : ",\n" );
	}
	oOutput << "  ]\n}\n";

	std::ofstream oFile( a_pOutputPath, std::ios::out | std::ios::trunc );
	if ( !oFile )
	{
		TERROR( "PSP locator table dump could not create %s\n", a_pOutputPath );
		return TFALSE;
	}
	oFile << oOutput.str();
	if ( !oFile.good() )
	{
		TERROR( "PSP locator table dump could not write %s\n", a_pOutputPath );
		return TFALSE;
	}

	TINFO(
		"PSP locator table dump wrote %d locators and %d VIS sections to %s\n",
		pHeader->iNumLocators,
		pVISHeader->iNumSections,
		a_pOutputPath
	);
	return TTRUE;
}
