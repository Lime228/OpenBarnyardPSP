#include "pch.h"

#include "AFrontEndInstanceManager_PSP.h"

#include <Toshi/TStringManager.h>

TOSHI_NAMESPACE_USING

struct AFrontEndInstancePackDefinition_PSP
{
	const TCHAR* szPrefix;
	TUINT        uiSourceInstances;
};

static const AFrontEndInstancePackDefinition_PSP s_aFrontEndInstancePackDefinitions_PSP[] = {
	{ "frontend_instances_fncw", 126 },
	{ "frontend_instances_fncp", 71 },
	{ "frontend_instances_fncborder", 40 },
};

static_assert(
	TARRAYSIZE( s_aFrontEndInstancePackDefinitions_PSP ) <= AFrontEndInstanceManager_PSP::MAX_PACKS_PSP,
	"Too many PSP frontend instance packs"
);

AFrontEndInstanceManager_PSP::AFrontEndInstanceManager_PSP()
    : m_uiPackCount_PSP( 0 )
    , m_uiSourceInstanceCount_PSP( 0 )
    , m_uiModelCount_PSP( 0 )
    , m_uiTextureCount_PSP( 0 )
	, m_szFailedPath_PSP{ '\0' }
{
}

AFrontEndInstanceManager_PSP::~AFrontEndInstanceManager_PSP()
{
	Unload();
}

TBOOL AFrontEndInstanceManager_PSP::Load()
{
	Unload();
	m_szFailedPath_PSP[ 0 ] = '\0';

	for ( TUINT i = 0; i < TARRAYSIZE( s_aFrontEndInstancePackDefinitions_PSP ); i++ )
	{
		const AFrontEndInstancePackDefinition_PSP& rDefinition = s_aFrontEndInstancePackDefinitions_PSP[ i ];
		if ( !m_aPacks_PSP[ i ].Load( rDefinition.szPrefix ) )
		{
			TStringManager::String8Copy(
				m_szFailedPath_PSP,
				m_aPacks_PSP[ i ].GetFailedPath(),
				TARRAYSIZE( m_szFailedPath_PSP )
			);
			for ( TUINT j = 0; j <= i; j++ )
				m_aPacks_PSP[ j ].Unload();
			m_uiPackCount_PSP = 0;
			m_uiSourceInstanceCount_PSP = 0;
			m_uiModelCount_PSP = 0;
			m_uiTextureCount_PSP = 0;
			return TFALSE;
		}

		m_uiPackCount_PSP++;
		m_uiSourceInstanceCount_PSP += rDefinition.uiSourceInstances;
		m_uiModelCount_PSP += m_aPacks_PSP[ i ].GetModelCount();
		m_uiTextureCount_PSP += m_aPacks_PSP[ i ].GetTextureCount();
	}

	return TTRUE;
}

void AFrontEndInstanceManager_PSP::Unload()
{
	for ( TUINT i = 0; i < MAX_PACKS_PSP; i++ )
		m_aPacks_PSP[ i ].Unload();
	m_uiPackCount_PSP = 0;
	m_uiSourceInstanceCount_PSP = 0;
	m_uiModelCount_PSP = 0;
	m_uiTextureCount_PSP = 0;
}

TUINT AFrontEndInstanceManager_PSP::Render(
	TRenderInterface_PSP& a_rRenderInterface,
	const TRenderCamera_PSP* a_pCamera,
	TFLOAT a_fHour
) const
{
	TUINT uiDraws = 0;
	for ( TUINT i = 0; i < m_uiPackCount_PSP; i++ )
		uiDraws += m_aPacks_PSP[ i ].Render( a_rRenderInterface, a_pCamera, a_fHour );
	return uiDraws;
}
