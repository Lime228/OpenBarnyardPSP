#include "pch.h"
#include "ASSeqTerrain_PSP.h"

#include <File/TFile.h>
#include <Platform/PSP/TRenderInterface_PSP.h>
#include <Toshi/TStringManager.h>

TOSHI_NAMESPACE_USING

struct ASSeqTerrainHeader_PSP
{
	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TUINT32 uiMeshes;
	TUINT32 uiTextures;
};

static constexpr TCHAR s_szSS01TerrainRoot_PSP[] = "local:Data/Terrain/SS01";
static constexpr TCHAR s_szSS01TerrainIndex_PSP[] = "local:Data/Terrain/SS01/terrain.ptld";
static constexpr TUINT32 s_uiSSeqTerrainMagic_PSP = 0x444c5450;

TBOOL ASSeqTerrain_PSP::HasSS01Pack()
{
	TFile* pFile = TFile::Create( s_szSS01TerrainIndex_PSP, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TFALSE;
	pFile->Destroy();
	return TTRUE;
}

TBOOL ASSeqTerrain_PSP::LoadSS01()
{
	Unload();
	TStringManager::String8Copy( m_szPath, s_szSS01TerrainIndex_PSP, TARRAYSIZE( m_szPath ) );
	TFile* pFile = TFile::Create( m_szPath, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TFALSE;

	static_assert( sizeof( ASSeqTerrainHeader_PSP ) == 16 && sizeof( MaterialEntry ) == 16 );
	ASSeqTerrainHeader_PSP oHeader;
	TBOOL bValid = pFile->Read( &oHeader, sizeof( oHeader ) ) == sizeof( oHeader ) &&
		oHeader.uiMagic == s_uiSSeqTerrainMagic_PSP && oHeader.uiVersion == 1 &&
		oHeader.uiMeshes > 0 && oHeader.uiMeshes <= MAX_MESHES &&
		oHeader.uiTextures > 0 && oHeader.uiTextures <= MAX_TEXTURES;
	if ( bValid )
	{
		const TSIZE uiMaterialBytes = TSIZE( oHeader.uiMeshes ) * sizeof( MaterialEntry );
		bValid = pFile->GetSize() == sizeof( oHeader ) + uiMaterialBytes &&
			pFile->Read( m_aMaterials, uiMaterialBytes ) == uiMaterialBytes;
	}
	pFile->Destroy();
	if ( !bValid )
		return TFALSE;

	for ( TUINT i = 0; i < oHeader.uiMeshes; i++ )
	{
		const MaterialEntry& rMaterial = m_aMaterials[ i ];
		if ( rMaterial.uiTexture >= oHeader.uiTextures || rMaterial.uiAlpha < TRENDERALPHAMODE_PSP_MASK ||
		     rMaterial.uiAlpha > TRENDERALPHAMODE_PSP_ADDITIVE ||
		     rMaterial.uiAddressU > TRENDERTEXTUREADDRESS_PSP_CLAMP || rMaterial.uiAddressV > TRENDERTEXTUREADDRESS_PSP_CLAMP )
			return TFALSE;
	}

	const TTextureFactory_PSP oTextureFactory;
	for ( TUINT i = 0; i < oHeader.uiTextures; i++ )
	{
		TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/texture_%u.ptex", s_szSS01TerrainRoot_PSP, i );
		if ( !oTextureFactory.CreateTextureFromFile( m_aTextures[ i ], m_szPath ) )
		{
			Unload();
			return TFALSE;
		}
		m_uiTextureCount++;
	}

	const TModelFactory_PSP oModelFactory;
	for ( TUINT i = 0; i < oHeader.uiMeshes; i++ )
	{
		TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/mesh_%u.pmes", s_szSS01TerrainRoot_PSP, i );
		if ( !oModelFactory.CreateModelFromFile( m_aMeshes[ i ], m_szPath ) )
		{
			Unload();
			return TFALSE;
		}
		m_uiMeshCount++;
	}
	m_szPath[ 0 ] = '\0';
	return TTRUE;
}

void ASSeqTerrain_PSP::Unload()
{
	while ( m_uiMeshCount > 0 )
		m_aMeshes[ --m_uiMeshCount ].Unload();
	while ( m_uiTextureCount > 0 )
		m_aTextures[ --m_uiTextureCount ].Unload();
}
