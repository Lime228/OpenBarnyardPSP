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
#include <string>

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

static Cell* FindCell_PSPTerrain( PTRB& a_rTRB, TINT a_iWorld, TINT a_iCell )
{
	if ( a_iWorld < 0 || a_iCell < 0 )
		return TNULL;
	auto oDatabase = a_rTRB.GetSymbols()->Find<WorldDatabase>( a_rTRB.GetSections(), "Database" );
	if ( !oDatabase || oDatabase.get()->m_uiNumWorlds == 0 || oDatabase.get()->m_uiNumWorlds > 64 ||
	     oDatabase.get()->m_ppWorlds == TNULL || TUINT( a_iWorld ) >= oDatabase.get()->m_uiNumWorlds )
		return TNULL;
	World* pWorld = oDatabase.get()->m_ppWorlds[ a_iWorld ];
	if ( pWorld == TNULL || pWorld->m_iNumCells <= 0 || pWorld->m_iNumCells > 4096 ||
	     pWorld->m_ppCells == TNULL || a_iCell >= pWorld->m_iNumCells )
		return TNULL;
	return pWorld->m_ppCells[ a_iCell ];
}

static TBOOL WriteMesh_PSPTerrain(
    std::ostream&              a_rOutput,
    CellMeshSphere*            a_pMeshSphere,
    TTMDBase::MaterialsHeader* a_pMaterials,
    const TCHAR*               a_pIndent,
    TBOOL                      a_bWriteObject
)
{
	CellMesh* pMesh = a_pMeshSphere != TNULL ? a_pMeshSphere->m_pCellMesh : TNULL;
	if ( pMesh == TNULL || pMesh->uiNumVertices1 == 0 || pMesh->uiNumVertices1 != pMesh->uiNumVertices2 ||
	     pMesh->uiNumVertices1 > 0xffff || pMesh->uiNumIndices < 3 || pMesh->uiNumIndices > 0xffff ||
	     pMesh->pVertices == TNULL || pMesh->pIndices == TNULL || pMesh->szMaterialName == TNULL )
		return TFALSE;
	for ( TUINT32 i = 0; i < pMesh->uiNumIndices; i++ )
	{
		if ( pMesh->pIndices[ i ] >= pMesh->uiNumVertices1 )
			return TFALSE;
	}

	std::string strPropertyIndent = a_pIndent;
	if ( a_bWriteObject )
	{
		a_rOutput << a_pIndent << "{\n";
		strPropertyIndent += "  ";
	}
	const TCHAR* pPropertyIndent = strPropertyIndent.c_str();
	a_rOutput << pPropertyIndent << "\"material\": ";
	WriteJSONString_PSPTerrain( a_rOutput, pMesh->szMaterialName );
	a_rOutput << ",\n" << pPropertyIndent << "\"texture\": ";
	WriteJSONString_PSPTerrain( a_rOutput, FindTexture_PSPTerrain( a_pMaterials, pMesh->szMaterialName ) );
	const TVector4& rSphere = a_pMeshSphere->m_BoundingSphere.AsVector4();
	a_rOutput << ",\n" << pPropertyIndent << "\"sphere\": [" << rSphere.x << ", " << rSphere.y << ", " << rSphere.z << ", " << rSphere.w
	          << "],\n" << pPropertyIndent << "\"vertices\": [\n";
	for ( TUINT32 i = 0; i < pMesh->uiNumVertices1; i++ )
	{
		const WorldVertex& rVertex = pMesh->pVertices[ i ];
		if ( !std::isfinite( rVertex.Position.x ) || !std::isfinite( rVertex.Position.y ) ||
		     !std::isfinite( rVertex.Position.z ) || !std::isfinite( rVertex.UV.x ) ||
		     !std::isfinite( rVertex.UV.y ) )
			return TFALSE;
		a_rOutput << pPropertyIndent << "  { \"position\": [" << rVertex.Position.x << ", " << rVertex.Position.y << ", " << rVertex.Position.z
		          << "], \"uv\": [" << rVertex.UV.x << ", " << rVertex.UV.y << "], \"colour\": ["
		          << rVertex.Color.x << ", " << rVertex.Color.y << ", " << rVertex.Color.z << "] }";
		if ( i + 1 != pMesh->uiNumVertices1 ) a_rOutput << ',';
		a_rOutput << '\n';
	}
	a_rOutput << pPropertyIndent << "],\n" << pPropertyIndent << "\"triangle_strip\": [";
	for ( TUINT32 i = 0; i < pMesh->uiNumIndices; i++ )
	{
		if ( i != 0 ) a_rOutput << ", ";
		a_rOutput << pMesh->pIndices[ i ];
	}
	a_rOutput << ']';
	if ( a_bWriteObject )
		a_rOutput << '\n' << a_pIndent << '}';
	return TTRUE;
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
	auto oMaterials = oTRB.GetSymbols()->Find<TTMDBase::MaterialsHeader>( oTRB.GetSections(), "Materials" );
	Cell* pCell = FindCell_PSPTerrain( oTRB, a_iWorld, a_iCell );
	if ( pCell == TNULL || pCell->uiNumMeshes == 0 || pCell->uiNumMeshes > 1024 ||
	     pCell->ppCellMeshSpheres == TNULL || TUINT( a_iMesh ) >= pCell->uiNumMeshes )
	{
		TERROR( "PSP terrain dump rejected mesh %d\n", a_iMesh );
		return TFALSE;
	}
	std::ostringstream oOutput;
	oOutput.imbue( std::locale::classic() );
	oOutput << std::setprecision( 9 );
	oOutput << "{\n  \"version\": 1,\n";
	if ( !WriteMesh_PSPTerrain( oOutput, pCell->ppCellMeshSpheres[ a_iMesh ], oMaterials.get(), "  ", TFALSE ) )
		return TFALSE;
	oOutput << "\n}\n";

	std::ofstream oFile( a_pOutputPath, std::ios::out | std::ios::trunc );
	if ( !oFile )
		return TFALSE;
	oFile << oOutput.str();
	return oFile.good();
}

TBOOL ExportPSPTerrainCellDump(
    const TCHAR* a_pInputPath,
    TINT         a_iWorld,
    TINT         a_iCell,
    const TCHAR* a_pOutputPath
)
{
	if ( a_pInputPath == TNULL || a_pOutputPath == TNULL )
		return TFALSE;
	PTRB oTRB;
	if ( !oTRB.ReadFromFile( a_pInputPath ) )
		return TFALSE;
	Cell* pCell = FindCell_PSPTerrain( oTRB, a_iWorld, a_iCell );
	if ( pCell == TNULL || pCell->uiNumMeshes == 0 || pCell->uiNumMeshes > 4 || pCell->ppCellMeshSpheres == TNULL )
	{
		TERROR( "PSP terrain cell requires between 1 and 4 meshes\n" );
		return TFALSE;
	}
	auto oMaterials = oTRB.GetSymbols()->Find<TTMDBase::MaterialsHeader>( oTRB.GetSections(), "Materials" );
	std::ostringstream oOutput;
	oOutput.imbue( std::locale::classic() );
	oOutput << std::setprecision( 9 );
	oOutput << "{\n  \"version\": 2,\n  \"world\": " << a_iWorld << ",\n  \"cell\": " << a_iCell << ",\n  \"meshes\": [\n";
	for ( TUINT32 i = 0; i < pCell->uiNumMeshes; i++ )
	{
		if ( !WriteMesh_PSPTerrain( oOutput, pCell->ppCellMeshSpheres[ i ], oMaterials.get(), "    ", TTRUE ) )
			return TFALSE;
		if ( i + 1 != pCell->uiNumMeshes ) oOutput << ',';
		oOutput << '\n';
	}
	oOutput << "  ]\n}\n";
	std::ofstream oFile( a_pOutputPath, std::ios::out | std::ios::trunc );
	if ( !oFile )
		return TFALSE;
	oFile << oOutput.str();
	return oFile.good();
}
