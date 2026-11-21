#include "pch.h"
#include "TPSPMaterialPropertiesDump.h"

#include <Plugins/PTRB.h>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>

TOSHI_NAMESPACE_USING

struct ClampedMaterial_PSP
{
	const TCHAR* szName;
	TUINT16      eAddressUState;
	TUINT16      eAddressVState;
};

struct AlphaRefMaterial_PSP
{
	const TCHAR* szName;
	void*        pUnknown;
};

struct WorldMaterialList_PSP
{
	TINT          iNumMaterials;
	const TCHAR** ppNames;
};

struct WorldUVOffsetsList_PSP
{
	struct Entry
	{
		const TCHAR* szName;
		TFLOAT       fSpeedX;
		TFLOAT       fSpeedY;
	};

	TINT   iNumMaterials;
	Entry* pEntries;
};

static void WriteJSONString_PSPMaterialProperties( std::ostream& a_rOutput, const TCHAR* a_pValue )
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

static TBOOL WriteTerminatedNames_PSPMaterialProperties(
    std::ostream& a_rOutput,
    const TCHAR* const* a_ppNames
)
{
	if ( a_ppNames == TNULL )
		return TFALSE;
	a_rOutput << '[';
	for ( TUINT i = 0; i < 256; i++ )
	{
		if ( a_ppNames[ i ] == TNULL )
		{
			a_rOutput << ']';
			return TTRUE;
		}
		if ( i != 0 )
			a_rOutput << ", ";
		WriteJSONString_PSPMaterialProperties( a_rOutput, a_ppNames[ i ] );
	}
	return TFALSE;
}

static TBOOL WriteCountedNames_PSPMaterialProperties(
    std::ostream& a_rOutput,
    const WorldMaterialList_PSP* a_pList
)
{
	if ( a_pList == TNULL || a_pList->iNumMaterials < 0 || a_pList->iNumMaterials > 4096 ||
	     ( a_pList->iNumMaterials > 0 && a_pList->ppNames == TNULL ) )
		return TFALSE;
	a_rOutput << '[';
	for ( TINT i = 0; i < a_pList->iNumMaterials; i++ )
	{
		if ( a_pList->ppNames[ i ] == TNULL )
			return TFALSE;
		if ( i != 0 )
			a_rOutput << ", ";
		WriteJSONString_PSPMaterialProperties( a_rOutput, a_pList->ppNames[ i ] );
	}
	a_rOutput << ']';
	return TTRUE;
}

TBOOL ExportPSPMaterialPropertiesDump(
    const TCHAR* a_pInputPath,
    const TCHAR* a_pOutputPath
)
{
	if ( a_pInputPath == TNULL || a_pOutputPath == TNULL )
		return TFALSE;

	PTRB oTRB;
	if ( !oTRB.ReadFromFile( a_pInputPath ) )
		return TFALSE;
	auto pSections = oTRB.GetSections();
	auto pSymbols = oTRB.GetSymbols();
	auto oGlow = pSymbols->Find<const TCHAR*>( pSections, "GlowMaterials" );
	auto oDoubleSided = pSymbols->Find<const TCHAR*>( pSections, "DoubleSidedMaterials" );
	auto oAlphaRef = pSymbols->Find<AlphaRefMaterial_PSP>( pSections, "AlphaRefMaterials" );
	auto oClamped = pSymbols->Find<ClampedMaterial_PSP>( pSections, "ClampedMaterials" );
	auto oWorldEnd = pSymbols->Find<WorldMaterialList_PSP>( pSections, "WorldEndRender" );
	auto oWorldStart = pSymbols->Find<WorldMaterialList_PSP>( pSections, "WorldStartRender" );
	auto oWorldUV = pSymbols->Find<WorldUVOffsetsList_PSP>( pSections, "WorldUVOffsetsPerSec" );
	if ( !oGlow || !oDoubleSided || !oAlphaRef || !oClamped || !oWorldEnd || !oWorldStart || !oWorldUV )
		return TFALSE;

	std::ostringstream oOutput;
	oOutput.imbue( std::locale::classic() );
	oOutput << std::setprecision( 9 );
	oOutput << "{\n  \"version\": 1,\n  \"glow\": ";
	if ( !WriteTerminatedNames_PSPMaterialProperties( oOutput, oGlow.get() ) )
		return TFALSE;
	oOutput << ",\n  \"double_sided\": ";
	if ( !WriteTerminatedNames_PSPMaterialProperties( oOutput, oDoubleSided.get() ) )
		return TFALSE;

	oOutput << ",\n  \"alpha_ref\": [";
	TBOOL bAlphaRefTerminated = TFALSE;
	for ( TUINT i = 0; i < 256; i++ )
	{
		const AlphaRefMaterial_PSP& rMaterial = oAlphaRef.get()[ i ];
		if ( rMaterial.szName == TNULL )
		{
			bAlphaRefTerminated = TTRUE;
			break;
		}
		if ( i != 0 )
			oOutput << ", ";
		WriteJSONString_PSPMaterialProperties( oOutput, rMaterial.szName );
	}
	if ( !bAlphaRefTerminated )
		return TFALSE;
	oOutput << "],\n  \"clamped\": [";
	TBOOL bClampedTerminated = TFALSE;
	for ( TUINT i = 0; i < 256; i++ )
	{
		const ClampedMaterial_PSP& rMaterial = oClamped.get()[ i ];
		if ( rMaterial.szName == TNULL )
		{
			bClampedTerminated = TTRUE;
			break;
		}
		if ( i != 0 )
			oOutput << ", ";
		oOutput << "{ \"material\": ";
		WriteJSONString_PSPMaterialProperties( oOutput, rMaterial.szName );
		oOutput << ", \"address_u\": " << rMaterial.eAddressUState
		        << ", \"address_v\": " << rMaterial.eAddressVState << " }";
	}
	if ( !bClampedTerminated )
		return TFALSE;

	oOutput << "],\n  \"world_end_render\": ";
	if ( !WriteCountedNames_PSPMaterialProperties( oOutput, oWorldEnd.get() ) )
		return TFALSE;
	oOutput << ",\n  \"world_start_render\": ";
	if ( !WriteCountedNames_PSPMaterialProperties( oOutput, oWorldStart.get() ) )
		return TFALSE;

	const WorldUVOffsetsList_PSP* pUVList = oWorldUV.get();
	if ( pUVList->iNumMaterials < 0 || pUVList->iNumMaterials > 4096 ||
	     ( pUVList->iNumMaterials > 0 && pUVList->pEntries == TNULL ) )
		return TFALSE;
	oOutput << ",\n  \"uv_offsets\": [";
	for ( TINT i = 0; i < pUVList->iNumMaterials; i++ )
	{
		const WorldUVOffsetsList_PSP::Entry& rEntry = pUVList->pEntries[ i ];
		if ( rEntry.szName == TNULL || !std::isfinite( rEntry.fSpeedX ) || !std::isfinite( rEntry.fSpeedY ) )
			return TFALSE;
		if ( i != 0 )
			oOutput << ", ";
		oOutput << "{ \"material\": ";
		WriteJSONString_PSPMaterialProperties( oOutput, rEntry.szName );
		oOutput << ", \"speed\": [" << rEntry.fSpeedX << ", " << rEntry.fSpeedY << "] }";
	}
	oOutput << "]\n}\n";

	std::ofstream oFile( a_pOutputPath, std::ios::out | std::ios::trunc );
	if ( !oFile )
		return TFALSE;
	oFile << oOutput.str();
	return oFile.good();
}
