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
#include <string>
#include <vector>

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
	{
		TERROR( "PSP model dump rejected skeleton bone count %d\n", iBoneCount );
		return TFALSE;
	}

	for ( TINT i = 0; i < iBoneCount; i++ )
	{
		const TSkeletonBone* pBone = a_rSkeleton.GetBone( i );
		const TUINT8 uiNameLength = pBone->GetNameLength();
		const TINT iParentBone = pBone->GetParentBone();
		if ( uiNameLength == 0 || uiNameLength > 31 ||
		     iParentBone < -1 || iParentBone >= i ||
		     a_rSkeleton.GetBoneID( pBone->GetName(), uiNameLength ) != i )
		{
			TERROR(
			    "PSP model dump rejected bone %d (name=%u parent=%d lookup=%d)\n",
			    i,
			    TUINT( uiNameLength ),
			    iParentBone,
			    a_rSkeleton.GetBoneID( pBone->GetName(), uiNameLength )
			);
			return TFALSE;
		}
	}
	return TTRUE;
}

static TBOOL RemapSubMesh_PSP(
    const TTMDWin::SubMesh& a_rSubMesh,
    const TTMDWin::SubMesh& a_rVertexUpload,
    TINT a_iBoneCount,
    std::vector<TUINT16>& a_rVertices,
    std::vector<TUINT16>& a_rIndices
)
{
	if ( a_rSubMesh.m_uiNumIndices < 3 ||
	     a_rSubMesh.m_uiNumIndices > 0xffff ||
	     a_rSubMesh.m_uiNumBones == 0 ||
	     a_rSubMesh.m_uiNumBones > TUINT32( a_iBoneCount ) ||
	     a_rSubMesh.m_pBones == TNULL ||
	     a_rSubMesh.m_pIndices == TNULL )
	{
		TERROR(
		    "PSP model dump rejected submesh layout (indices=%u bones=%u)\n",
		    a_rSubMesh.m_uiNumIndices,
		    a_rSubMesh.m_uiNumBones
		);
		return TFALSE;
	}

	for ( TUINT32 i = 0; i < a_rSubMesh.m_uiNumBones; i++ )
	{
		if ( a_rSubMesh.m_pBones[ i ] >= TUINT32( a_iBoneCount ) )
		{
			TERROR( "PSP model dump rejected bone palette entry %u (%u)\n", i, a_rSubMesh.m_pBones[ i ] );
			return TFALSE;
		}
	}
	std::vector<TINT> aRemap( a_rVertexUpload.m_uiNumVertices1, -1 );
	a_rIndices.reserve( a_rSubMesh.m_uiNumIndices );
	for ( TUINT32 i = 0; i < a_rSubMesh.m_uiNumIndices; i++ )
	{
		const TUINT16 uiVertex = a_rSubMesh.m_pIndices[ i ];
		if ( uiVertex >= a_rVertexUpload.m_uiNumVertices1 )
		{
			TERROR( "PSP model dump rejected index %u (%u)\n", i, uiVertex );
			return TFALSE;
		}
		if ( aRemap[ uiVertex ] < 0 )
		{
			aRemap[ uiVertex ] = TINT( a_rVertices.size() );
			a_rVertices.push_back( uiVertex );
		}
		a_rIndices.push_back( TUINT16( aRemap[ uiVertex ] ) );
	}
	for ( TUINT16 uiVertex : a_rVertices )
	{
		const TTMDWin::Vertex& rVertex = a_rVertexUpload.m_pVertices[ uiVertex ];
		for ( TUINT32 influence = 0; influence < 4; influence++ )
		{
			const TUINT8 uiBoneRegister = rVertex.Bones[ influence ];
			if ( rVertex.Weights[ influence ] != 0 &&
			     ( uiBoneRegister % 3 != 0 || uiBoneRegister / 3 >= a_rSubMesh.m_uiNumBones ) )
			{
				TERROR(
				    "PSP model dump rejected vertex %u influence %u (register=%u weight=%u palette=%u)\n",
				    TUINT( uiVertex ),
				    influence,
				    TUINT( uiBoneRegister ),
				    TUINT( rVertex.Weights[ influence ] ),
				    a_rSubMesh.m_uiNumBones
				);
				return TFALSE;
			}
		}
	}
	return TTRUE;
}

static const TCHAR* FindTexture_PSPModel(
	TTMDBase::MaterialsHeader* a_pMaterials,
	const TCHAR* a_pMaterialName
)
{
	if ( a_pMaterials == TNULL || a_pMaterialName == TNULL ||
	     a_pMaterials->iNumMaterials <= 0 || a_pMaterials->iNumMaterials > 4096 )
		return TNULL;

	for ( TINT i = 0; i < a_pMaterials->iNumMaterials; i++ )
	{
		TTMDBase::Material* pMaterial = a_pMaterials->GetMaterial( i );
		if ( TStringManager::String8CompareNoCase( pMaterial->szMatName, a_pMaterialName ) == 0 )
			return pMaterial->szTextureFile;
	}

	return TNULL;
}

TBOOL ExportPSPModelDump( const TCHAR* a_pInputPath, const TCHAR* a_pOutputPath, const TCHAR* a_pModelName )
{
	if ( a_pInputPath == TNULL || a_pOutputPath == TNULL )
		return TFALSE;

	PTRB oTRB;
	if ( !oTRB.ReadFromFile( a_pInputPath ) )
	{
		TERROR( "PSP model dump could not read %s\n", a_pInputPath );
		return TFALSE;
	}
	PTRBSections* pSections = oTRB.GetSections();
	PTRBSymbols* pSymbols = oTRB.GetSymbols();
	const std::string strPrefix = a_pModelName != TNULL && a_pModelName[ 0 ] != '\0' ?
	                                 std::string( a_pModelName ) + "_" : "";
	auto oFileHeader = pSymbols->Find<TTMDBase::FileHeader>( pSections, ( strPrefix + "FileHeader" ).c_str() );
	auto oHeader = pSymbols->Find<TTMDWin::TRBWinHeader>( pSections, ( strPrefix + "Header" ).c_str() );
	auto oSkeleton = pSymbols->Find<TSkeleton>( pSections, ( strPrefix + "Skeleton" ).c_str() );
	auto oMaterials = pSymbols->Find<TTMDBase::MaterialsHeader>( pSections, ( strPrefix + "Materials" ).c_str() );
	if ( !oFileHeader || !oHeader || !oSkeleton || !oMaterials ||
	     oFileHeader.get()->m_uiMagic != TFourCC( "TMDL" ) ||
	     oHeader.get()->m_iNumLODs <= 0 ||
	     oMaterials.get()->iNumMaterials <= 0 || oMaterials.get()->iNumMaterials > 4096 ||
	     !ValidateSkeleton_PSP( *oSkeleton.get() ) )
	{
		TERROR( "PSP model dump rejected required TMD symbols, magic, LODs or skeleton\n" );
		return TFALSE;
	}

	const TTMDWin::TRBLODHeader* pLOD = oHeader.get()->GetLOD( 0 );
	const TINT iMeshCount = pLOD->m_iMeshCount1 + pLOD->m_iMeshCount2;
	if ( pLOD->m_eShader != TTMDBase::SHADERTYPE_SKIN || iMeshCount <= 0 )
	{
		TERROR( "PSP model dump rejected LOD0 (shader=%u meshes=%d)\n", TUINT( pLOD->m_eShader ), iMeshCount );
		return TFALSE;
	}

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
		const std::string strSymbolName = strPrefix + "LOD0_Mesh_" + std::to_string( mesh );
		auto oMesh = pSymbols->Find<TTMDWin::TRBMeshLODHeader>( pSections, strSymbolName.c_str() );
		if ( !oMesh || oMesh.get()->m_uiNumSubMeshes == 0 || oMesh.get()->m_pSubMeshes == TNULL )
		{
			TERROR( "PSP model dump rejected mesh symbol %s\n", strSymbolName.c_str() );
			return TFALSE;
		}

		// LoadSkinLOD uploads into one vertex pool per material mesh. Each draw
		// uses its own strip and bones over that pool.
		const TTMDWin::SubMesh* pVertexUpload = TNULL;
		for ( TUINT32 i = 0; i < oMesh.get()->m_uiNumSubMeshes; i++ )
		{
			const TTMDWin::SubMesh& rSubMesh = oMesh.get()->m_pSubMeshes[ i ];
			if ( rSubMesh.m_uiNumVertices1 != 0 )
				pVertexUpload = &rSubMesh;
		}
		if ( pVertexUpload == TNULL || pVertexUpload->m_pVertices == TNULL ||
		     pVertexUpload->m_uiNumVertices1 > 0xffff || pVertexUpload->m_uiNumVertices1 > oMesh.get()->m_uiNumVertices )
		{
			TERROR( "PSP model dump rejected vertex pool for %s\n", strSymbolName.c_str() );
			return TFALSE;
		}

		oOutput << "      { \"material\": ";
		const TCHAR* pMaterialName = oMesh.get()->m_szMaterialName;
		const TCHAR* pTextureName = FindTexture_PSPModel( oMaterials.get(), pMaterialName );
		if ( pMaterialName == TNULL || pTextureName == TNULL || pTextureName[ 0 ] == '\0' )
		{
			TERROR( "PSP model dump could not resolve mesh %d material texture\n", mesh );
			return TFALSE;
		}
		WriteJSONString_PSP( oOutput, pMaterialName, pMaterialName != TNULL ? TStringManager::String8Length( pMaterialName ) : 0 );
		oOutput << ", \"texture\": ";
		WriteJSONString_PSP( oOutput, pTextureName, TStringManager::String8Length( pTextureName ) );
		oOutput << ", \"submeshes\": [\n";
		for ( TUINT32 submesh = 0; submesh < oMesh.get()->m_uiNumSubMeshes; submesh++ )
		{
			const TTMDWin::SubMesh& rSubMesh = oMesh.get()->m_pSubMeshes[ submesh ];
			std::vector<TUINT16> aVertices;
			std::vector<TUINT16> aIndices;
			if ( !RemapSubMesh_PSP( rSubMesh, *pVertexUpload, oSkeleton.get()->GetBoneCount(), aVertices, aIndices ) )
				return TFALSE;
			oOutput << "        { \"bone_palette\": [";
			for ( TUINT32 bone = 0; bone < rSubMesh.m_uiNumBones; bone++ )
			{
				if ( bone != 0 ) oOutput << ", ";
				oOutput << rSubMesh.m_pBones[ bone ];
			}
			oOutput << "], \"vertices\": [\n";
			for ( TSIZE vertex = 0; vertex < aVertices.size(); vertex++ )
			{
				const TTMDWin::Vertex& rVertex = pVertexUpload->m_pVertices[ aVertices[ vertex ] ];
			oOutput << "          { \"position\": [" << rVertex.Position.x << ", " << rVertex.Position.y << ", " << rVertex.Position.z
				        << "], \"uv\": [" << rVertex.UV.x << ", " << rVertex.UV.y << "], \"bones\": ["
			        << TUINT( rVertex.Bones[ 0 ] / 3 ) << ", " << TUINT( rVertex.Bones[ 1 ] / 3 ) << ", "
			        << TUINT( rVertex.Bones[ 2 ] / 3 ) << ", " << TUINT( rVertex.Bones[ 3 ] / 3 ) << "], \"weights\": ["
				        << TUINT( rVertex.Weights[ 0 ] ) << ", " << TUINT( rVertex.Weights[ 1 ] ) << ", "
				        << TUINT( rVertex.Weights[ 2 ] ) << ", " << TUINT( rVertex.Weights[ 3 ] ) << "] }";
				if ( vertex + 1 != aVertices.size() ) oOutput << ',';
				oOutput << '\n';
			}
			oOutput << "        ], \"triangle_strip\": [";
			for ( TSIZE index = 0; index < aIndices.size(); index++ )
			{
				if ( index != 0 ) oOutput << ", ";
				oOutput << aIndices[ index ];
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
	{
		TERROR( "PSP model dump could not create %s\n", a_pOutputPath );
		return TFALSE;
	}
	oFile << oOutput.str();
	if ( !oFile.good() )
	{
		TERROR( "PSP model dump could not write %s\n", a_pOutputPath );
		return TFALSE;
	}
	return TTRUE;
}
