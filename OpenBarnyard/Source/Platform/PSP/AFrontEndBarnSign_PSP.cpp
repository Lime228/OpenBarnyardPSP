#include "pch.h"

#include "AFrontEndBarnSign_PSP.h"

#include <Toshi/TStringManager.h>

TOSHI_NAMESPACE_USING

static const TRenderMaterial_PSP s_FrontEndBarnSignMaterial_PSP = {
	TTRUE,
	TRENDERTEXTUREFUNCTION_PSP_MODULATE,
	TRENDERCULLMODE_PSP_NONE,
	TRENDERALPHAMODE_PSP_MASK,
};

static const TRenderSceneTransform_PSP s_FrontEndBarnSignTransform_PSP = {
	{ 0.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f },
	1.0f,
};

static const TRenderModelTransform_PSP s_FrontEndBarnSignModelTransform_PSP = {
	1.0f,
	{ 0.0f, 0.0f, 0.0f },
};

AFrontEndBarnSign_PSP::AFrontEndBarnSign_PSP()
    : m_uiMeshCount_PSP( 0 )
    , m_szFailedPath_PSP{ '\0' }
{
}

AFrontEndBarnSign_PSP::~AFrontEndBarnSign_PSP()
{
	Unload();
}

TBOOL AFrontEndBarnSign_PSP::Load()
{
	Unload();
	for ( TUINT i = 0; i < MAX_MESHES_PSP; i++ )
	{
		TStringManager::String8Format(
			m_szFailedPath_PSP,
			TARRAYSIZE( m_szFailedPath_PSP ),
			"local:frontend_barnsign_%u.pmes",
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
			"local:frontend_barnsign_%u.ptex",
			i
		);
		if ( !m_TextureFactory_PSP.CreateTextureFromFile( m_aTextures_PSP[ i ], m_szFailedPath_PSP ) )
		{
			Unload();
			return TFALSE;
		}

		m_uiMeshCount_PSP++;
	}

	m_szFailedPath_PSP[ 0 ] = '\0';
	return TTRUE;
}

void AFrontEndBarnSign_PSP::Unload()
{
	for ( TUINT i = 0; i < MAX_MESHES_PSP; i++ )
	{
		m_aTextures_PSP[ i ].Unload();
		m_aModels_PSP[ i ].Unload();
	}
	m_uiMeshCount_PSP = 0;
}

TUINT AFrontEndBarnSign_PSP::Render(
    TRenderInterface_PSP& a_rRenderInterface,
    const TRenderCamera_PSP* a_pCamera
) const
{
	if ( !IsLoaded() || a_pCamera == TNULL )
		return 0;

	for ( TUINT i = 0; i < m_uiMeshCount_PSP; i++ )
	{
		a_rRenderInterface.DrawModel(
			m_aModels_PSP[ i ],
			s_FrontEndBarnSignTransform_PSP,
			s_FrontEndBarnSignMaterial_PSP,
			&m_aTextures_PSP[ i ],
			s_FrontEndBarnSignModelTransform_PSP,
			a_pCamera
		);
	}

	return m_uiMeshCount_PSP;
}
