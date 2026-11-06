#include "pch.h"
#include "TPSPTerrainDump.h"

#include "World/AWorld.h"

#include <Plugins/PTRB.h>
#include <Render/TTMDBase.h>

#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>

TOSHI_NAMESPACE_USING

static void WriteJSONString_PSPTerrain( std::ostream& a_rOutput, const TCHAR* a_pValue )
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

static const TCHAR* FindTexture_PSPTerrain(
	TTMDBase::MaterialsHeader* a_pMaterials,
	const TCHAR*                a_pMaterialName
)
{
	if ( a_pMaterials == TNULL || a_pMaterialName == TNULL ||
	     a_pMaterials->iNumMaterials < 0 || a_pMaterials->iNumMaterials > 4096 )
		return TNULL;

	for ( TINT i = 0; i < a_pMaterials->iNumMaterials; i++ )
	{
		TTMDBase::Material* pMaterial = a_pMaterials->GetMaterial( i );
		if ( TStringManager::String8CompareNoCase( pMaterial->szMatName, a_pMaterialName ) == 0 )
			return pMaterial->szTextureFile;
	}

	return TNULL;
}

TBOOL ExportPSPTerrainDump(
    const TCHAR* a_pInputPath,
    TINT         a_iWorld,
    TINT         a_iCell,
    TINT         a_iMesh,
    const TCHAR* a_pOutputPath
)
{
	if ( a_pInputPath == TNULL || a_pOutputPath == TNULL || a_iWorld < 0 || a_iCell < 0 || a_iMesh < 0 )
		return TFALSE;

	PTRB oTRB;
	if ( !oTRB.ReadFromFile( a_pInputPath ) )
		return TFALSE;
	auto oDatabase = oTRB.GetSymbols()->Find<WorldDatabase>( oTRB.GetSections(), "Database" );
	auto oMaterials = oTRB.GetSymbols()->Find<TTMDBase::MaterialsHeader>( oTRB.GetSections(), "Materials" );
	if ( !oDatabase || oDatabase.get()->m_uiNumWorlds == 0 || oDatabase.get()->m_uiNumWorlds > 64 ||
	     oDatabase.get()->m_ppWorlds == TNULL || TUINT( a_iWorld ) >= oDatabase.get()->m_uiNumWorlds )
	{
		TERROR( "PSP terrain dump rejected world %d\n", a_iWorld );
		return TFALSE;
	}
	World* pWorld = oDatabase.get()->m_ppWorlds[ a_iWorld ];
	if ( pWorld == TNULL || pWorld->m_iNumCells <= 0 || pWorld->m_iNumCells > 4096 ||
	     pWorld->m_ppCells == TNULL || a_iCell >= pWorld->m_iNumCells )
	{
		TERROR( "PSP terrain dump rejected cell %d\n", a_iCell );
		return TFALSE;
	}
	Cell* pCell = pWorld->m_ppCells[ a_iCell ];
	if ( pCell == TNULL || pCell->uiNumMeshes == 0 || pCell->uiNumMeshes > 1024 ||
	     pCell->ppCellMeshSpheres == TNULL || TUINT( a_iMesh ) >= pCell->uiNumMeshes )
	{
		TERROR( "PSP terrain dump rejected mesh %d\n", a_iMesh );
		return TFALSE;
	}
	CellMeshSphere* pMeshSphere = pCell->ppCellMeshSpheres[ a_iMesh ];
	CellMesh* pMesh = pMeshSphere != TNULL ? pMeshSphere->m_pCellMesh : TNULL;
	if ( pMesh == TNULL || pMesh->uiNumVertices1 == 0 || pMesh->uiNumVertices1 != pMesh->uiNumVertices2 ||
	     pMesh->uiNumVertices1 > 0xffff || pMesh->uiNumIndices < 3 || pMesh->uiNumIndices > 0xffff ||
	     pMesh->pVertices == TNULL || pMesh->pIndices == TNULL || pMesh->szMaterialName == TNULL )
	{
		TERROR( "PSP terrain dump rejected mesh layout\n" );
		return TFALSE;
	}
	for ( TUINT32 i = 0; i < pMesh->uiNumIndices; i++ )
	{
		if ( pMesh->pIndices[ i ] >= pMesh->uiNumVertices1 )
			return TFALSE;
	}

	std::ostringstream oOutput;
	oOutput.imbue( std::locale::classic() );
	oOutput << std::setprecision( 9 );
	oOutput << "{\n  \"version\": 1,\n  \"material\": ";
	WriteJSONString_PSPTerrain( oOutput, pMesh->szMaterialName );
	oOutput << ",\n  \"texture\": ";
	WriteJSONString_PSPTerrain( oOutput, FindTexture_PSPTerrain( oMaterials.get(), pMesh->szMaterialName ) );
	const TVector4& rSphere = pMeshSphere->m_BoundingSphere.AsVector4();
	oOutput << ",\n  \"sphere\": [" << rSphere.x << ", " << rSphere.y << ", " << rSphere.z << ", " << rSphere.w
	        << "],\n  \"vertices\": [\n";
	for ( TUINT32 i = 0; i < pMesh->uiNumVertices1; i++ )
	{
		const WorldVertex& rVertex = pMesh->pVertices[ i ];
		if ( !std::isfinite( rVertex.Position.x ) || !std::isfinite( rVertex.Position.y ) ||
		     !std::isfinite( rVertex.Position.z ) || !std::isfinite( rVertex.UV.x ) ||
		     !std::isfinite( rVertex.UV.y ) )
			return TFALSE;
		oOutput << "    { \"position\": [" << rVertex.Position.x << ", " << rVertex.Position.y << ", " << rVertex.Position.z
		        << "], \"uv\": [" << rVertex.UV.x << ", " << rVertex.UV.y << "], \"colour\": ["
		        << rVertex.Color.x << ", " << rVertex.Color.y << ", " << rVertex.Color.z << "] }";
		if ( i + 1 != pMesh->uiNumVertices1 ) oOutput << ',';
		oOutput << '\n';
	}
	oOutput << "  ],\n  \"triangle_strip\": [";
	for ( TUINT32 i = 0; i < pMesh->uiNumIndices; i++ )
	{
		if ( i != 0 ) oOutput << ", ";
		oOutput << pMesh->pIndices[ i ];
	}
	oOutput << "]\n}\n";

	std::ofstream oFile( a_pOutputPath, std::ios::out | std::ios::trunc );
	if ( !oFile )
		return TFALSE;
	oFile << oOutput.str();
	return oFile.good();
}
