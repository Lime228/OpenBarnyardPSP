#include "pch.h"

#include "AFrontEndTerrain_PSP.h"

#include <File/TFile.h>
#include <Toshi/TStringManager.h>

TOSHI_NAMESPACE_USING

static const TRenderMaterial_PSP s_FrontEndTerrainMaterial_PSP = {
	TTRUE,
	TRENDERTEXTUREFUNCTION_PSP_MODULATE,
	TRENDERCULLMODE_PSP_NONE,
	TRENDERALPHAMODE_PSP_MASK,
};

static const TRenderSceneTransform_PSP s_FrontEndTerrainSceneTransform_PSP = {
	{ 0.0f, -0.45f, -4.8f },
	{ 0.0f, 0.0f, 0.0f },
	0.85f,
};

static const TRenderSceneTransform_PSP s_FrontEndTerrainOriginalSceneTransform_PSP = {
	{ 0.0f, 0.0f, 0.0f },
	{ 1.570796f, 0.0f, 0.0f },
	1.0f,
};

static const TRenderModelTransform_PSP s_FrontEndTerrainOriginalModelTransform_PSP = {
	1.0f,
	{ 0.0f, 0.0f, 0.0f },
};

struct AFrontEndTerrainConfigHeader_PSP
{
	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TUINT32 uiMeshCount;
	TUINT32 uiEntrySize;
};

static constexpr TUINT32 s_uiFrontEndTerrainConfigMagic_PSP = 0x52544650;
static constexpr TUINT32 s_uiFrontEndTerrainConfigVersion1_PSP = 1;
static constexpr TUINT32 s_uiFrontEndTerrainConfigVersion2_PSP = 2;
static constexpr TUINT32 s_uiFrontEndTerrainConfigEntrySize_PSP = sizeof( TUINT32 );

static_assert( sizeof( AFrontEndTerrainConfigHeader_PSP ) == 16, "Unexpected PSP frontend terrain config layout" );
static_assert( TRENDERALPHAMODE_PSP_MASK == 1, "Unexpected PSP mask material value" );
static_assert( TRENDERALPHAMODE_PSP_BLEND == 2, "Unexpected PSP blend material value" );
static_assert( TRENDERALPHAMODE_PSP_ADDITIVE == 3, "Unexpected PSP additive material value" );

AFrontEndTerrain_PSP::AFrontEndTerrain_PSP()
    : m_ModelTransform_PSP{ 1.0f, { 0.0f, 0.0f, 0.0f } }
    , m_uiMeshCount_PSP( 0 )
    , m_szFailedPath_PSP{ '\0' }
{
	for ( TUINT i = 0; i < MAX_MESHES_PSP; i++ )
		m_aeAlphaModes_PSP[ i ] = TRENDERALPHAMODE_PSP_MASK;
}

AFrontEndTerrain_PSP::~AFrontEndTerrain_PSP()
{
	Unload();
}

TBOOL AFrontEndTerrain_PSP::Load()
{
	Unload();
	TStringManager::String8Copy(
		m_szFailedPath_PSP,
		"local:barnyard_terrain.pcfg",
		TARRAYSIZE( m_szFailedPath_PSP )
	);
	TFile* pConfigFile = TFile::Create( m_szFailedPath_PSP, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pConfigFile == TNULL )
		return TFALSE;

	AFrontEndTerrainConfigHeader_PSP oConfig = {};
	const TSIZE uiConfigSize = pConfigFile->GetSize();
	TBOOL bConfigValid =
		pConfigFile->Read( &oConfig, sizeof( oConfig ) ) == sizeof( oConfig ) &&
		oConfig.uiMagic == s_uiFrontEndTerrainConfigMagic_PSP &&
		oConfig.uiMeshCount >= 1 &&
		oConfig.uiMeshCount <= MAX_MESHES_PSP;
	if ( bConfigValid && oConfig.uiVersion == s_uiFrontEndTerrainConfigVersion1_PSP )
	{
		bConfigValid = uiConfigSize == sizeof( oConfig ) && oConfig.uiEntrySize == 0;
	}
	else if ( bConfigValid && oConfig.uiVersion == s_uiFrontEndTerrainConfigVersion2_PSP )
	{
		bConfigValid = oConfig.uiEntrySize == s_uiFrontEndTerrainConfigEntrySize_PSP &&
			uiConfigSize == sizeof( oConfig ) + TSIZE( oConfig.uiMeshCount ) * oConfig.uiEntrySize;
		for ( TUINT i = 0; i < oConfig.uiMeshCount && bConfigValid; i++ )
		{
			TUINT32 uiAlphaMode = 0;
			bConfigValid = pConfigFile->Read( &uiAlphaMode, sizeof( uiAlphaMode ) ) == sizeof( uiAlphaMode ) &&
				uiAlphaMode >= TRENDERALPHAMODE_PSP_MASK &&
				uiAlphaMode <= TRENDERALPHAMODE_PSP_ADDITIVE;
			if ( bConfigValid )
				m_aeAlphaModes_PSP[ i ] = TRenderAlphaMode_PSP( uiAlphaMode );
		}
	}
	else
	{
		bConfigValid = TFALSE;
	}
	pConfigFile->Destroy();
	if ( !bConfigValid )
	{
		TStringManager::String8Copy(
			m_szFailedPath_PSP,
			"frontend-terrain-config",
			TARRAYSIZE( m_szFailedPath_PSP )
		);
		return TFALSE;
	}

	for ( TUINT i = 0; i < oConfig.uiMeshCount; i++ )
	{
		TStringManager::String8Format(
			m_szFailedPath_PSP,
			TARRAYSIZE( m_szFailedPath_PSP ),
			"local:barnyard_terrain_%u.pmes",
			i
		);
		if ( !m_ModelFactory_PSP.CreateModelFromFile( m_aModels_PSP[ i ], m_szFailedPath_PSP ) )
		{
			Unload();
			return TFALSE;
		}

		TStringManager::String8Format(
			m_szFailedPath_PSP,
			TARRAYSIZE( m_szFailedPath_PSP ),
			"local:barnyard_terrain_%u.ptex",
			i
		);
		if ( !m_TextureFactory_PSP.CreateTextureFromFile( m_aTextures_PSP[ i ], m_szFailedPath_PSP ) )
		{
			Unload();
			return TFALSE;
		}

		m_uiMeshCount_PSP++;
	}

	if ( !CalculateModelTransform_PSP() )
	{
		TStringManager::String8Copy(
			m_szFailedPath_PSP,
			"frontend-terrain-bounds",
			TARRAYSIZE( m_szFailedPath_PSP )
		);
		Unload();
		return TFALSE;
	}

	m_szFailedPath_PSP[ 0 ] = '\0';
	return TTRUE;
}

void AFrontEndTerrain_PSP::Unload()
{
	for ( TUINT i = 0; i < MAX_MESHES_PSP; i++ )
	{
		m_aTextures_PSP[ i ].Unload();
		m_aModels_PSP[ i ].Unload();
		m_aeAlphaModes_PSP[ i ] = TRENDERALPHAMODE_PSP_MASK;
	}

	m_ModelTransform_PSP = { 1.0f, { 0.0f, 0.0f, 0.0f } };
	m_uiMeshCount_PSP = 0;
}

TUINT AFrontEndTerrain_PSP::Render(
    TRenderInterface_PSP& a_rRenderInterface,
    const TRenderCamera_PSP* a_pCamera
) const
{
	if ( !IsLoaded() )
		return 0;

	const TRenderSceneTransform_PSP& rSceneTransform = a_pCamera != TNULL ?
		s_FrontEndTerrainOriginalSceneTransform_PSP : s_FrontEndTerrainSceneTransform_PSP;
	const TRenderModelTransform_PSP& rModelTransform = a_pCamera != TNULL ?
		s_FrontEndTerrainOriginalModelTransform_PSP : m_ModelTransform_PSP;

	for ( TUINT i = 0; i < m_uiMeshCount_PSP; i++ )
	{
		TRenderMaterial_PSP oMaterial = s_FrontEndTerrainMaterial_PSP;
		oMaterial.eAlphaMode = m_aeAlphaModes_PSP[ i ];
		a_rRenderInterface.DrawModel(
			m_aModels_PSP[ i ],
			rSceneTransform,
			oMaterial,
			&m_aTextures_PSP[ i ],
			rModelTransform,
			a_pCamera
		);
	}

	return m_uiMeshCount_PSP;
}

TBOOL AFrontEndTerrain_PSP::CalculateModelTransform_PSP()
{
	if ( m_uiMeshCount_PSP == 0 )
		return TFALSE;

	TFLOAT aMin[ 3 ];
	TFLOAT aMax[ 3 ];
	for ( TUINT axis = 0; axis < TARRAYSIZE( aMin ); axis++ )
	{
		aMin[ axis ] = m_aModels_PSP[ 0 ].GetBounds().aMin[ axis ];
		aMax[ axis ] = m_aModels_PSP[ 0 ].GetBounds().aMax[ axis ];
	}

	for ( TUINT mesh = 1; mesh < m_uiMeshCount_PSP; mesh++ )
	{
		const TModelBounds_PSP& rBounds = m_aModels_PSP[ mesh ].GetBounds();
		for ( TUINT axis = 0; axis < TARRAYSIZE( aMin ); axis++ )
		{
			aMin[ axis ] = TMath::Min( aMin[ axis ], rBounds.aMin[ axis ] );
			aMax[ axis ] = TMath::Max( aMax[ axis ], rBounds.aMax[ axis ] );
		}
	}

	const TFLOAT fLargestSize = TMath::Max(
		TMath::Max( aMax[ 0 ] - aMin[ 0 ], aMax[ 1 ] - aMin[ 1 ] ),
		aMax[ 2 ] - aMin[ 2 ]
	);
	if ( !TMath::IsFinite( fLargestSize ) || fLargestSize <= TMath::TFLOAT_EPSILON )
		return TFALSE;

	m_ModelTransform_PSP.fScale = 2.0f / fLargestSize;
	for ( TUINT axis = 0; axis < TARRAYSIZE( aMin ); axis++ )
	{
		m_ModelTransform_PSP.aCenter[ axis ] =
			aMin[ axis ] + ( aMax[ axis ] - aMin[ axis ] ) * 0.5f;
	}
	return TTRUE;
}
