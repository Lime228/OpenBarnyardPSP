#include "pch.h"

#include "AFrontEndTerrainPack_PSP.h"

#include <File/TFile.h>
#include <Toshi/TStringManager.h>

TOSHI_NAMESPACE_USING

static const TRenderMaterial_PSP s_FrontEndTerrainPackMaterial_PSP = {
	TTRUE,
	TRENDERTEXTUREFUNCTION_PSP_MODULATE,
	TRENDERCULLMODE_PSP_NONE,
	TRENDERALPHAMODE_PSP_MASK,
	TRENDERTEXTUREADDRESS_PSP_WRAP,
	TRENDERTEXTUREADDRESS_PSP_WRAP,
};

static const TRenderSceneTransform_PSP s_FrontEndTerrainPackTransform_PSP = {
	{ 0.0f, 0.0f, 0.0f },
	{ 1.570796f, 0.0f, 0.0f },
	1.0f,
};

static const TRenderModelTransform_PSP s_FrontEndTerrainPackModelTransform_PSP = {
	1.0f,
	{ 0.0f, 0.0f, 0.0f },
};

struct AFrontEndTerrainPackHeader_PSP
{
	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TUINT32 uiModelCount;
	TUINT32 uiTextureCount;
	TUINT32 uiEntrySize;
	TUINT32 uiReserved;
};

struct AFrontEndTerrainPackEntry_PSP
{
	TUINT32 uiModelIndex;
	TUINT32 uiTextureIndex;
	TUINT32 uiAlphaMode;
	TUINT32 uiAddressU;
	TUINT32 uiAddressV;
};

static constexpr TUINT32 s_uiFrontEndTerrainPackMagic_PSP = 0x50444650;
static constexpr TUINT32 s_uiFrontEndTerrainPackVersion_PSP = 1;

static_assert( sizeof( AFrontEndTerrainPackHeader_PSP ) == 24, "Unexpected PSP frontend terrain pack header" );
static_assert( sizeof( AFrontEndTerrainPackEntry_PSP ) == 20, "Unexpected PSP frontend terrain pack entry" );
static_assert( TRENDERTEXTUREADDRESS_PSP_WRAP == 0, "Unexpected PSP wrap material value" );
static_assert( TRENDERTEXTUREADDRESS_PSP_CLAMP == 1, "Unexpected PSP clamp material value" );

static TBOOL AFrontEndTerrainPackValidatePrefix_PSP( const TCHAR* a_szPrefix )
{
	if ( a_szPrefix == TNULL || a_szPrefix[ 0 ] == '\0' )
		return TFALSE;

	for ( TSIZE i = 0; a_szPrefix[ i ] != '\0'; i++ )
	{
		if ( i + 1 >= 32 )
			return TFALSE;

		const TCHAR c = a_szPrefix[ i ];
		if ( !( c >= 'a' && c <= 'z' ) &&
		     !( c >= 'A' && c <= 'Z' ) &&
		     !( c >= '0' && c <= '9' ) &&
		     c != '_' )
		{
			return TFALSE;
		}
	}

	return TTRUE;
}

AFrontEndTerrainPack_PSP::AFrontEndTerrainPack_PSP()
    : m_uiModelCount_PSP( 0 )
    , m_uiTextureCount_PSP( 0 )
	, m_szPrefix_PSP{ '\0' }
    , m_szFailedPath_PSP{ '\0' }
{
}

AFrontEndTerrainPack_PSP::~AFrontEndTerrainPack_PSP()
{
	Unload();
}

TBOOL AFrontEndTerrainPack_PSP::Load( const TCHAR* a_szPrefix )
{
	Unload();
	if ( !AFrontEndTerrainPackValidatePrefix_PSP( a_szPrefix ) )
	{
		TStringManager::String8Copy(
			m_szFailedPath_PSP,
			"frontend-terrain-pack-prefix",
			TARRAYSIZE( m_szFailedPath_PSP )
		);
		return TFALSE;
	}

	TStringManager::String8Copy( m_szPrefix_PSP, a_szPrefix, TARRAYSIZE( m_szPrefix_PSP ) );
	TStringManager::String8Format(
		m_szFailedPath_PSP,
		TARRAYSIZE( m_szFailedPath_PSP ),
		"local:%s.pcfg",
		m_szPrefix_PSP
	);
	TFile* pConfigFile = TFile::Create( m_szFailedPath_PSP, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pConfigFile == TNULL )
		return TFALSE;

	AFrontEndTerrainPackHeader_PSP oHeader = {};
	TBOOL bConfigValid =
		pConfigFile->Read( &oHeader, sizeof( oHeader ) ) == sizeof( oHeader ) &&
		oHeader.uiMagic == s_uiFrontEndTerrainPackMagic_PSP &&
		oHeader.uiVersion == s_uiFrontEndTerrainPackVersion_PSP &&
		oHeader.uiModelCount >= 1 &&
		oHeader.uiModelCount <= MAX_MODELS_PSP &&
		oHeader.uiTextureCount >= 1 &&
		oHeader.uiTextureCount <= MAX_TEXTURES_PSP &&
		oHeader.uiEntrySize == sizeof( AFrontEndTerrainPackEntry_PSP ) &&
		oHeader.uiReserved == 0 &&
		pConfigFile->GetSize() == sizeof( oHeader ) + TSIZE( oHeader.uiModelCount ) * oHeader.uiEntrySize;

	TBOOL aModelEntries[ MAX_MODELS_PSP ] = {};
	for ( TUINT i = 0; i < oHeader.uiModelCount && bConfigValid; i++ )
	{
		AFrontEndTerrainPackEntry_PSP oSourceEntry = {};
		bConfigValid =
			pConfigFile->Read( &oSourceEntry, sizeof( oSourceEntry ) ) == sizeof( oSourceEntry ) &&
			oSourceEntry.uiModelIndex < oHeader.uiModelCount &&
			oSourceEntry.uiTextureIndex < oHeader.uiTextureCount &&
			oSourceEntry.uiAlphaMode >= TRENDERALPHAMODE_PSP_MASK &&
			oSourceEntry.uiAlphaMode <= TRENDERALPHAMODE_PSP_ADDITIVE &&
			oSourceEntry.uiAddressU <= TRENDERTEXTUREADDRESS_PSP_CLAMP &&
			oSourceEntry.uiAddressV <= TRENDERTEXTUREADDRESS_PSP_CLAMP &&
			!aModelEntries[ oSourceEntry.uiModelIndex ];
		if ( bConfigValid )
		{
			aModelEntries[ oSourceEntry.uiModelIndex ] = TTRUE;
			m_aEntries_PSP[ i ] = {
				oSourceEntry.uiModelIndex,
				oSourceEntry.uiTextureIndex,
				TRenderAlphaMode_PSP( oSourceEntry.uiAlphaMode ),
				TRenderTextureAddress_PSP( oSourceEntry.uiAddressU ),
				TRenderTextureAddress_PSP( oSourceEntry.uiAddressV ),
			};
		}
	}
	pConfigFile->Destroy();
	if ( !bConfigValid )
	{
		TStringManager::String8Copy(
			m_szFailedPath_PSP,
			"frontend-detail-config",
			TARRAYSIZE( m_szFailedPath_PSP )
		);
		return TFALSE;
	}

	for ( TUINT i = 0; i < oHeader.uiModelCount; i++ )
	{
		TStringManager::String8Format(
			m_szFailedPath_PSP,
			TARRAYSIZE( m_szFailedPath_PSP ),
			"local:%s_model_%u.pmes",
			m_szPrefix_PSP,
			i
		);
		if ( !m_ModelFactory_PSP.CreateModelFromFile( m_aModels_PSP[ i ], m_szFailedPath_PSP ) )
		{
			Unload();
			return TFALSE;
		}
		m_uiModelCount_PSP++;
	}

	for ( TUINT i = 0; i < oHeader.uiTextureCount; i++ )
	{
		TStringManager::String8Format(
			m_szFailedPath_PSP,
			TARRAYSIZE( m_szFailedPath_PSP ),
			"local:%s_texture_%u.ptex",
			m_szPrefix_PSP,
			i
		);
		if ( !m_TextureFactory_PSP.CreateTextureFromFile( m_aTextures_PSP[ i ], m_szFailedPath_PSP ) )
		{
			Unload();
			return TFALSE;
		}
		m_uiTextureCount_PSP++;
	}

	m_szFailedPath_PSP[ 0 ] = '\0';
	return TTRUE;
}

void AFrontEndTerrainPack_PSP::Unload()
{
	for ( TUINT i = 0; i < MAX_TEXTURES_PSP; i++ )
		m_aTextures_PSP[ i ].Unload();
	for ( TUINT i = 0; i < MAX_MODELS_PSP; i++ )
		m_aModels_PSP[ i ].Unload();
	m_uiModelCount_PSP = 0;
	m_uiTextureCount_PSP = 0;
}

TUINT AFrontEndTerrainPack_PSP::Render(
	TRenderInterface_PSP& a_rRenderInterface,
	const TRenderCamera_PSP* a_pCamera
) const
{
	if ( !IsLoaded() || a_pCamera == TNULL )
		return 0;

	for ( TUINT i = 0; i < m_uiModelCount_PSP; i++ )
	{
		const MaterialEntry_PSP& rEntry = m_aEntries_PSP[ i ];
		TRenderMaterial_PSP oMaterial = s_FrontEndTerrainPackMaterial_PSP;
		oMaterial.eAlphaMode = rEntry.eAlphaMode;
		oMaterial.eAddressU = rEntry.eAddressU;
		oMaterial.eAddressV = rEntry.eAddressV;
		a_rRenderInterface.DrawModel(
			m_aModels_PSP[ rEntry.uiModelIndex ],
			s_FrontEndTerrainPackTransform_PSP,
			oMaterial,
			&m_aTextures_PSP[ rEntry.uiTextureIndex ],
			s_FrontEndTerrainPackModelTransform_PSP,
			a_pCamera
		);
	}

	return m_uiModelCount_PSP;
}
