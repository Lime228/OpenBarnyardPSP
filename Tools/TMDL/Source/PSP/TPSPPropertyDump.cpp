#include "pch.h"
#include "TPSPPropertyDump.h"

#include <Plugins/PPropertyParser/PBProperties.h>
#include <Plugins/PTRB.h>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>

TOSHI_NAMESPACE_USING

static constexpr TUINT s_uiPropertyDepthLimit_PSP = 24;
static constexpr TUINT s_uiPropertyValueLimit_PSP = 250000;

static TSIZE GetPropertyBytes_PSP( PTRBSections& a_rSections, const void* a_pAddress )
{
	const TUINTPTR uiAddress = reinterpret_cast<TUINTPTR>( a_pAddress );
	for ( auto pStack : a_rSections )
	{
		const TUINTPTR uiBase = reinterpret_cast<TUINTPTR>( pStack->GetBuffer() );
		if ( uiAddress >= uiBase && uiAddress - uiBase < pStack->GetUsedSize() )
			return pStack->GetUsedSize() - ( uiAddress - uiBase );
	}

	return 0;
}

static void WritePropertyString_PSP( std::ostream& a_rOutput, const TCHAR* a_pString, TSIZE a_uiLength )
{
	constexpr TCHAR s_szHex[] = "0123456789abcdef";
	a_rOutput << '"';
	for ( TSIZE i = 0; i < a_uiLength; i++ )
	{
		const TUINT8 uiChar = static_cast<TUINT8>( a_pString[ i ] );
		if ( uiChar < 32 || uiChar >= 127 )
			a_rOutput << "\\u00" << s_szHex[ uiChar >> 4 ] << s_szHex[ uiChar & 15 ];
		else
		{
			if ( uiChar == '"' || uiChar == '\\' )
				a_rOutput << '\\';
			a_rOutput << a_pString[ i ];
		}
	}
	a_rOutput << '"';
}

static TBOOL WritePropertyString_PSP(
    std::ostream& a_rOutput, PTRBSections& a_rSections, const TCHAR* a_pString
)
{
	const TSIZE uiBytes = GetPropertyBytes_PSP( a_rSections, a_pString );
	for ( TSIZE i = 0; i < uiBytes && i < 8192; i++ )
	{
		if ( a_pString[ i ] == '\0' )
		{
			WritePropertyString_PSP( a_rOutput, a_pString, i );
			return TTRUE;
		}
	}
	return TFALSE;
}

static TBOOL WritePropertyValue_PSP(
    std::ostream& a_rOutput, PTRBSections& a_rSections,
    const PBPropertyValue* a_pValue, TUINT a_uiDepth, TUINT& a_rRemaining
);

static TBOOL WriteProperties_PSP(
    std::ostream& a_rOutput, PTRBSections& a_rSections,
    const PBProperties* a_pProperties, TUINT a_uiDepth, TUINT& a_rRemaining
)
{
	if ( a_uiDepth > s_uiPropertyDepthLimit_PSP ||
	     GetPropertyBytes_PSP( a_rSections, a_pProperties ) < sizeof( PBProperties ) )
		return TFALSE;

	const TUINT uiCount = a_pProperties->GetPropertyCount();
	if ( uiCount > a_rRemaining ||
	     GetPropertyBytes_PSP( a_rSections, a_pProperties->Begin() ) < uiCount * sizeof( PBProperties::PBProperty ) )
		return TFALSE;

	// Repeated keys such as Sound and Wave retain their order in this format.
	a_rOutput << "{\"properties\":[";
	for ( TUINT i = 0; i < uiCount; i++ )
	{
		const auto pProperty = a_pProperties->GetPropertyByIndex( i );
		const auto& rName    = pProperty->GetName();
		if ( GetPropertyBytes_PSP( a_rSections, &rName ) < sizeof( rName ) )
			return TFALSE;

		if ( i != 0 ) a_rOutput << ',';
		a_rOutput << '[';
		if ( !WritePropertyString_PSP( a_rOutput, a_rSections, rName.GetString() ) )
			return TFALSE;
		a_rOutput << ',';
		if ( !WritePropertyValue_PSP( a_rOutput, a_rSections, pProperty->GetValue(), a_uiDepth + 1, a_rRemaining ) )
			return TFALSE;
		a_rOutput << ']';
	}
	a_rOutput << "]}";
	return TTRUE;
}

static TBOOL WritePropertyValue_PSP(
    std::ostream& a_rOutput, PTRBSections& a_rSections,
    const PBPropertyValue* a_pValue, TUINT a_uiDepth, TUINT& a_rRemaining
)
{
	if ( a_uiDepth > s_uiPropertyDepthLimit_PSP || a_rRemaining == 0 ||
	     GetPropertyBytes_PSP( a_rSections, a_pValue ) < sizeof( PBPropertyValue ) )
		return TFALSE;
	a_rRemaining--;

	using Type = PBPropertyValue::Type;
	switch ( a_pValue->GetType() )
	{
		case Type::Int: a_rOutput << a_pValue->GetInteger(); return TTRUE;
		case Type::UInt32: a_rOutput << a_pValue->GetUINT32(); return TTRUE;
		case Type::Bool: a_rOutput << ( a_pValue->GetBoolean() ? "true" : "false" ); return TTRUE;
		case Type::String: return WritePropertyString_PSP( a_rOutput, a_rSections, a_pValue->GetString() );
		case Type::LocaleString:
			a_rOutput << "{\"locale_index\":" << a_pValue->GetLocaleString().uiIndex << '}';
			return TTRUE;
		case Type::Float:
			if ( !std::isfinite( a_pValue->GetFloat() ) ) return TFALSE;
			a_rOutput << a_pValue->GetFloat();
			return TTRUE;
		case Type::Properties:
			return WriteProperties_PSP( a_rOutput, a_rSections, a_pValue->GetProperties(), a_uiDepth, a_rRemaining );
		case Type::Array:
		{
			const auto pArray = a_pValue->GetArray();
			if ( GetPropertyBytes_PSP( a_rSections, pArray ) < sizeof( PBPropertyValueArray ) ||
			     pArray->GetSize() > a_rRemaining )
				return TFALSE;
			if ( pArray->GetSize() &&
			     GetPropertyBytes_PSP( a_rSections, pArray->GetValue( 0 ) ) < pArray->GetSize() * sizeof( PBPropertyValue ) )
				return TFALSE;
			a_rOutput << '[';
			for ( TUINT i = 0; i < pArray->GetSize(); i++ )
			{
				if ( i != 0 ) a_rOutput << ',';
				if ( !WritePropertyValue_PSP( a_rOutput, a_rSections, pArray->GetValue( i ), a_uiDepth + 1, a_rRemaining ) )
					return TFALSE;
			}
			a_rOutput << ']';
			return TTRUE;
		}
		default: return TFALSE;
	}
}

TBOOL ExportPSPPropertyDump( const TCHAR* a_pInputPath, const TCHAR* a_pOutputPath )
{
	if ( a_pInputPath == TNULL || a_pOutputPath == TNULL )
		return TFALSE;
	PTRB oTRB;
	if ( !oTRB.ReadFromFile( a_pInputPath ) )
		return TFALSE;

	auto pSections = oTRB.GetSections();
	auto pSymbols = oTRB.GetSymbols();
	std::ostringstream oOutput;
	oOutput.imbue( std::locale::classic() );
	oOutput << std::setprecision( 9 ) << "{\"version\":1,\"symbols\":[";
	TUINT uiWritten   = 0;
	TUINT uiRemaining = s_uiPropertyValueLimit_PSP;
	for ( TUINT i = 0; i < pSymbols->GetCount(); i++ )
	{
		const auto oName         = pSymbols->GetName( i );
		const TSIZE uiNameLength = oName.Length();
		if ( TStringManager::String8Compare( oName.Get(), "Main" ) != 0 &&
		     ( uiNameLength < 5 || TStringManager::String8Compare( oName.Get() + uiNameLength - 5, "_Main" ) != 0 ) )
			continue;
		if ( uiWritten++ != 0 ) oOutput << ',';
		oOutput << "\n{\"symbol\":";
		WritePropertyString_PSP( oOutput, oName.Get(), uiNameLength );
		oOutput << ",\"root\":";
		auto oRoot = pSymbols->GetByIndex<PBProperties>( pSections, i );
		if ( !WriteProperties_PSP( oOutput, *pSections, oRoot.get(), 0, uiRemaining ) )
		{
			TERROR( "PSP property dump could not read %s in %s\n", oName.Get(), a_pInputPath );
			return TFALSE;
		}
		oOutput << '}';
	}
	if ( uiWritten == 0 )
		return TFALSE;
	oOutput << "\n]}\n";
	std::ofstream oFile( a_pOutputPath, std::ios::out | std::ios::trunc );
	oFile << oOutput.str();
	oFile.close();
	return oFile.good();
}
