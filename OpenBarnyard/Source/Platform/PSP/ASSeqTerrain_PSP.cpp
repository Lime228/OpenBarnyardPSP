#include "pch.h"
#include "ASSeqTerrain_PSP.h"

#include <File/TFile.h>
#include <Math/TMathInline.h>
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
static constexpr TCHAR s_szSS01BeadyIntDayRoot_PSP[] = "local:Data/Terrain/SS01/BeadyIntDay";
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

TBOOL ASSeqTerrain_PSP::LoadSS01( TBOOL a_bBeadyIntDay )
{
	Unload();
	const TCHAR* szRoot = a_bBeadyIntDay ? s_szSS01BeadyIntDayRoot_PSP : s_szSS01TerrainRoot_PSP;
	TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/terrain.ptld", szRoot );
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
		TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/texture_%u.ptex", szRoot, i );
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
		TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/mesh_%u.pmes", szRoot, i );
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

TUINT ASSeqTerrain_PSP::Render( TRenderInterface_PSP& a_rRenderInterface, const TRenderCamera_PSP& a_rCamera ) const
{
	static const TRenderSceneTransform_PSP s_Transform = { { 0.0f, 0.0f, 0.0f }, { TMath::HALF_PI, 0.0f, 0.0f }, 1.0f };
	static const TRenderModelTransform_PSP s_ModelTransform = { 1.0f, { 0.0f, 0.0f, 0.0f } };
	const TFLOAT fSlopeY = TMath::Tan( TMath::DegToRad( a_rCamera.fFOV * 0.5f ) );
	const TFLOAT fSlopeX = fSlopeY * TFLOAT( TRenderInterface_PSP::SCREEN_WIDTH ) / TRenderInterface_PSP::SCREEN_HEIGHT;
	const TFLOAT fPlaneX = TMath::Sqrt( 1.0f + fSlopeX * fSlopeX );
	const TFLOAT fPlaneY = TMath::Sqrt( 1.0f + fSlopeY * fSlopeY );
	const TVector3 vecForward( a_rCamera.aDirection[ 0 ], a_rCamera.aDirection[ 1 ], a_rCamera.aDirection[ 2 ] );
	const TVector3 vecUp( a_rCamera.aUp[ 0 ], a_rCamera.aUp[ 1 ], a_rCamera.aUp[ 2 ] );
	TVector3 vecRight;
	vecRight.CrossProduct( vecForward, vecUp );

	TUINT uiDraws = 0;
	for ( TUINT i = 0; i < m_uiMeshCount; i++ )
	{
		const TModelBounds_PSP& rBounds = m_aMeshes[ i ].GetBounds();
		const TVector3 vecHalfExtent(
			( rBounds.aMax[ 0 ] - rBounds.aMin[ 0 ] ) * 0.5f,
			( rBounds.aMax[ 1 ] - rBounds.aMin[ 1 ] ) * 0.5f,
			( rBounds.aMax[ 2 ] - rBounds.aMin[ 2 ] ) * 0.5f
		);
		const TFLOAT fRadius = vecHalfExtent.Magnitude();
		const TVector3 vecOffset(
			rBounds.aMin[ 0 ] + vecHalfExtent.x - a_rCamera.aPosition[ 0 ],
			-( rBounds.aMin[ 2 ] + vecHalfExtent.z ) - a_rCamera.aPosition[ 1 ],
			rBounds.aMin[ 1 ] + vecHalfExtent.y - a_rCamera.aPosition[ 2 ]
		);
		const TFLOAT fDepth = TVector3::DotProduct( vecForward, vecOffset );
		if ( fDepth + fRadius < a_rCamera.fNear || fDepth - fRadius > 280.0f ||
		     TMath::Abs( TVector3::DotProduct( vecRight, vecOffset ) ) > fDepth * fSlopeX + fRadius * fPlaneX ||
		     TMath::Abs( TVector3::DotProduct( vecUp, vecOffset ) ) > fDepth * fSlopeY + fRadius * fPlaneY )
			continue;

		const MaterialEntry& rEntry = m_aMaterials[ i ];
		const TRenderMaterial_PSP oMaterial = {
			TTRUE, TRENDERTEXTUREFUNCTION_PSP_MODULATE, TRENDERCULLMODE_PSP_NONE,
			TRenderAlphaMode_PSP( rEntry.uiAlpha ), TRenderTextureAddress_PSP( rEntry.uiAddressU ),
			TRenderTextureAddress_PSP( rEntry.uiAddressV ),
		};
		a_rRenderInterface.DrawModel( m_aMeshes[ i ], s_Transform, oMaterial, &m_aTextures[ rEntry.uiTexture ], s_ModelTransform, &a_rCamera );
		uiDraws++;
	}
	return uiDraws;
}
