#include "pch.h"
#include "TPSPGUISectionDump.h"

#include <GUI/T2GUITextureSectionManager.h>
#include <Plugins/PTRB.h>

#include <fstream>

TOSHI_NAMESPACE_USING

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

TBOOL ExportPSPGUISectionDump(
    const TCHAR* a_pTextureSectionsPath,
    const TCHAR* a_pSectionName,
    const TCHAR* a_pOutputPath
)
{
	if ( a_pTextureSectionsPath == TNULL || a_pSectionName == TNULL || a_pOutputPath == TNULL )
		return TFALSE;

	PTRB oTRB;
	if ( !oTRB.ReadFromFile( a_pTextureSectionsPath ) )
	{
		TERROR( "PSP GUI section dump could not read %s\n", a_pTextureSectionsPath );
		return TFALSE;
	}

	auto oHeader = oTRB.GetSymbols()->Find<T2GUITextureSectionManager::TRBHeader>(
	    oTRB.GetSections(), "texturesections"
	);
	if ( !oHeader || oHeader.get()->m_pSections == TNULL ||
	     oHeader.get()->m_uiNumSections == 0 || oHeader.get()->m_uiNumSections > 4096 )
	{
		TERROR( "PSP GUI section dump rejected texture section header\n" );
		return TFALSE;
	}

	const T2GUITextureSection* pSelected = TNULL;
	for ( TUINT i = 0; i < oHeader.get()->m_uiNumSections; i++ )
	{
		const T2GUITextureSection& rSection = oHeader.get()->m_pSections[ i ];
		if ( rSection.GetName() != TNULL &&
		     TStringManager::String8CompareNoCase( rSection.GetName(), a_pSectionName ) == 0 )
		{
			pSelected = &rSection;
			break;
		}
	}
	if ( pSelected == TNULL || pSelected->GetTextureFile() == TNULL ||
	     pSelected->GetPoint1X() < 0 || pSelected->GetPoint1Y() < 0 ||
	     pSelected->GetPoint2X() <= pSelected->GetPoint1X() ||
	     pSelected->GetPoint2Y() <= pSelected->GetPoint1Y() )
	{
		TERROR( "PSP GUI section dump could not resolve bounded section %s\n", a_pSectionName );
		return TFALSE;
	}

	std::ofstream oOutput( a_pOutputPath, std::ios::out | std::ios::trunc );
	if ( !oOutput )
		return TFALSE;
	oOutput << "{\n  \"version\": 1,\n  \"name\": ";
	WriteJSONString_PSP( oOutput, pSelected->GetName() );
	oOutput << ",\n  \"texture\": ";
	WriteJSONString_PSP( oOutput, pSelected->GetTextureFile() );
	oOutput << ",\n  \"point1\": [" << pSelected->GetPoint1X() << ", " << pSelected->GetPoint1Y()
	        << "],\n  \"point2\": [" << pSelected->GetPoint2X() << ", " << pSelected->GetPoint2Y()
	        << "]\n}\n";
	return oOutput.good();
}
