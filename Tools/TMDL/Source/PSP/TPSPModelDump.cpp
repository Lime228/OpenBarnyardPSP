#include "pch.h"
#include "TPSPModelDump.h"

#include <File/TTRB.h>
#include <Plugins/PTRB.h>
#include <Render/TTMDBase.h>
#include <Render/TTMDWin.h>
#include <Render/TSkeleton.h>

#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>

TOSHI_NAMESPACE_USING

static void WriteJSONString_PSP( std::ostream& a_rOutput, const TCHAR* a_pValue, TSIZE a_uiLength )
{
	a_rOutput << '"';
	for ( TSIZE i = 0; a_pValue != TNULL && i < a_uiLength; i++ )
	{
		const TCHAR cCharacter = a_pValue[ i ];
		switch ( cCharacter )
		{
			case '"': a_rOutput << "\\\""; break;
			case '\\': a_rOutput << "\\\\"; break;
			case '\n': a_rOutput << "\\n"; break;
			case '\r': a_rOutput << "\\r"; break;
			case '\t': a_rOutput << "\\t"; break;
			default:
				if ( TUINT8( cCharacter ) < 0x20 )
					a_rOutput << '?';
				else
					a_rOutput << cCharacter;
				break;
		}
	}
	a_rOutput << '"';
}

static TBOOL ValidateSkeleton_PSP( TSkeleton& a_rSkeleton )
{
	const TINT iBoneCount = a_rSkeleton.GetBoneCount();
	if ( iBoneCount <= 0 || iBoneCount > 64 )
		return TFALSE;

	for ( TINT i = 0; i < iBoneCount; i++ )
	{
		const TSkeletonBone* pBone = a_rSkeleton.GetBone( i );
		const TUINT8 uiNameLength = pBone->GetNameLength();
		const TINT iParentBone = pBone->GetParentBone();
		if ( uiNameLength == 0 || uiNameLength > 31 ||
		     ( i == 0 && iParentBone != -1 ) ||
		     ( i != 0 && ( iParentBone < 0 || iParentBone >= i ) ) ||
		     a_rSkeleton.GetBoneID( pBone->GetName(), uiNameLength ) != i )
		{
			return TFALSE;
		}
	}
	return TTRUE;
}

static TBOOL ValidateSubMesh_PSP( const TTMDWin::SubMesh& a_rSubMesh, TINT a_iBoneCount )
{
	if ( a_rSubMesh.m_uiNumVertices1 == 0 ||
	     a_rSubMesh.m_uiNumVertices1 != a_rSubMesh.m_uiNumVertices2 ||
	     a_rSubMesh.m_uiNumVertices1 > 0xffff ||
	     a_rSubMesh.m_uiNumIndices < 3 ||
	     a_rSubMesh.m_uiNumIndices > 0xffff ||
	     a_rSubMesh.m_uiNumBones == 0 ||
	     a_rSubMesh.m_uiNumBones > TUINT32( a_iBoneCount ) ||
	     a_rSubMesh.m_pBones == TNULL ||
	     a_rSubMesh.m_pVertices == TNULL ||
	     a_rSubMesh.m_pIndices == TNULL )
	{
		return TFALSE;
	}

	for ( TUINT32 i = 0; i < a_rSubMesh.m_uiNumBones; i++ )
	{
		if ( a_rSubMesh.m_pBones[ i ] >= TUINT32( a_iBoneCount ) )
			return TFALSE;
	}
	for ( TUINT32 i = 0; i < a_rSubMesh.m_uiNumIndices; i++ )
	{
		if ( a_rSubMesh.m_pIndices[ i ] >= a_rSubMesh.m_uiNumVertices1 )
			return TFALSE;
	}
	for ( TUINT32 i = 0; i < a_rSubMesh.m_uiNumVertices1; i++ )
	{
		for ( TUINT32 influence = 0; influence < 4; influence++ )
		{
			if ( a_rSubMesh.m_pVertices[ i ].Weights[ influence ] != 0 &&
			     a_rSubMesh.m_pVertices[ i ].Bones[ influence ] >= a_rSubMesh.m_uiNumBones )
			{
				return TFALSE;
			}
		}
	}
	return TTRUE;
}

TBOOL ExportPSPModelDump( const TCHAR* a_pInputPath, const TCHAR* a_pOutputPath )
{
	if ( a_pInputPath == TNULL || a_pOutputPath == TNULL )
		return TFALSE;

	PTRB oTRB;
	if ( !oTRB.ReadFromFile( a_pInputPath ) )
		return TFALSE;
	PTRBSections* pSections = oTRB.GetSections();
	PTRBSymbols* pSymbols = oTRB.GetSymbols();
	auto oFileHeader = pSymbols->Find<TTMDBase::FileHeader>( pSections, "FileHeader" );
	auto oHeader = pSymbols->Find<TTMDWin::TRBWinHeader>( pSections, "Header" );
	auto oSkeleton = pSymbols->Find<TSkeleton>( pSections, "Skeleton" );
	if ( !oFileHeader || !oHeader || !oSkeleton ||
	     oFileHeader.get()->m_uiMagic != TFourCC( "TMDL" ) ||
	     oHeader.get()->m_iNumLODs <= 0 ||
	     !ValidateSkeleton_PSP( *oSkeleton.get() ) )
	{
		return TFALSE;
	}

	const TTMDWin::TRBLODHeader* pLOD = oHeader.get()->GetLOD( 0 );
	const TINT iMeshCount = pLOD->m_iMeshCount1 + pLOD->m_iMeshCount2;
	if ( pLOD->m_eShader != TTMDBase::SHADERTYPE_SKIN || iMeshCount <= 0 )
		return TFALSE;

	std::ostringstream oOutput;
	oOutput.imbue( std::locale::classic() );
	oOutput << std::setprecision( 9 );
	oOutput << "{\n  \"version\": 1,\n  \"skeleton\": [\n";
	for ( TINT i = 0; i < oSkeleton.get()->GetBoneCount(); i++ )
	{
		const TSkeletonBone* pBone = oSkeleton.get()->GetBone( i );
		const TQuaternion& rRotation = pBone->GetRotation();
		const TVector3& rPosition = pBone->GetPosition();
		oOutput << "    { \"name\": ";
		WriteJSONString_PSP( oOutput, pBone->GetName(), pBone->GetNameLength() );
		oOutput << ", \"parent\": ";
		if ( pBone->GetParentBone() < 0 ) oOutput << "null";
		else oOutput << pBone->GetParentBone();
		oOutput << ", \"translation\": [" << rPosition.x << ", " << rPosition.y << ", " << rPosition.z
		        << "], \"rotation\": [" << rRotation.x << ", " << rRotation.y << ", " << rRotation.z << ", " << rRotation.w << "] }";
		if ( i + 1 != oSkeleton.get()->GetBoneCount() ) oOutput << ',';
		oOutput << '\n';
	}
	oOutput << "  ],\n  \"lods\": [\n    { \"shader\": \"skin\", \"meshes\": [\n";

	for ( TINT mesh = 0; mesh < iMeshCount; mesh++ )
	{
		TCHAR szSymbolName[ 64 ];
		TStringManager::String8Format( szSymbolName, sizeof( szSymbolName ), "LOD0_Mesh_%d", mesh );
		auto oMesh = pSymbols->Find<TTMDWin::TRBMeshLODHeader>( pSections, szSymbolName );
		if ( !oMesh || oMesh.get()->m_uiNumSubMeshes == 0 || oMesh.get()->m_pSubMeshes == TNULL )
			return TFALSE;

		oOutput << "      { \"material\": ";
		const TCHAR* pMaterialName = oMesh.get()->m_szMaterialName;
		WriteJSONString_PSP( oOutput, pMaterialName, pMaterialName != TNULL ? TStringManager::String8Length( pMaterialName ) : 0 );
		oOutput << ", \"submeshes\": [\n";
		for ( TUINT32 submesh = 0; submesh < oMesh.get()->m_uiNumSubMeshes; submesh++ )
		{
			const TTMDWin::SubMesh& rSubMesh = oMesh.get()->m_pSubMeshes[ submesh ];
			if ( !ValidateSubMesh_PSP( rSubMesh, oSkeleton.get()->GetBoneCount() ) )
				return TFALSE;
			oOutput << "        { \"bone_palette\": [";
			for ( TUINT32 bone = 0; bone < rSubMesh.m_uiNumBones; bone++ )
			{
				if ( bone != 0 ) oOutput << ", ";
				oOutput << rSubMesh.m_pBones[ bone ];
			}
			oOutput << "], \"vertices\": [\n";
			for ( TUINT32 vertex = 0; vertex < rSubMesh.m_uiNumVertices1; vertex++ )
			{
				const TTMDWin::Vertex& rVertex = rSubMesh.m_pVertices[ vertex ];
				oOutput << "          { \"position\": [" << rVertex.Position.x << ", " << rVertex.Position.y << ", " << rVertex.Position.z
				        << "], \"uv\": [" << rVertex.UV.x << ", " << rVertex.UV.y << "], \"bones\": ["
				        << TUINT( rVertex.Bones[ 0 ] ) << ", " << TUINT( rVertex.Bones[ 1 ] ) << ", "
				        << TUINT( rVertex.Bones[ 2 ] ) << ", " << TUINT( rVertex.Bones[ 3 ] ) << "], \"weights\": ["
				        << TUINT( rVertex.Weights[ 0 ] ) << ", " << TUINT( rVertex.Weights[ 1 ] ) << ", "
				        << TUINT( rVertex.Weights[ 2 ] ) << ", " << TUINT( rVertex.Weights[ 3 ] ) << "] }";
				if ( vertex + 1 != rSubMesh.m_uiNumVertices1 ) oOutput << ',';
				oOutput << '\n';
			}
			oOutput << "        ], \"triangle_strip\": [";
			for ( TUINT32 index = 0; index < rSubMesh.m_uiNumIndices; index++ )
			{
				if ( index != 0 ) oOutput << ", ";
				oOutput << rSubMesh.m_pIndices[ index ];
			}
			oOutput << "] }";
			if ( submesh + 1 != oMesh.get()->m_uiNumSubMeshes ) oOutput << ',';
			oOutput << '\n';
		}
		oOutput << "      ] }";
		if ( mesh + 1 != iMeshCount ) oOutput << ',';
		oOutput << '\n';
	}

	oOutput << "    ] }\n  ]\n}\n";
	if ( !oOutput.good() )
		return TFALSE;

	std::ofstream oFile( a_pOutputPath, std::ios::out | std::ios::trunc );
	if ( !oFile )
		return TFALSE;
	oFile << oOutput.rdbuf();
	return oFile.good();
}
