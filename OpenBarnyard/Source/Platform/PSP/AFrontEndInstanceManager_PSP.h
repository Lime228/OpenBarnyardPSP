#pragma once

#include "AFrontEndTerrainPack_PSP.h"

class AFrontEndInstanceManager_PSP
{
public:
	static constexpr TUINT MAX_PACKS_PSP = 8;

public:
	AFrontEndInstanceManager_PSP();
	~AFrontEndInstanceManager_PSP();

	TBOOL Load();
	void  Unload();
	TUINT Render(
		Toshi::TRenderInterface_PSP& a_rRenderInterface,
		const Toshi::TRenderCamera_PSP* a_pCamera,
		TFLOAT a_fHour
	) const;

	TBOOL IsLoaded() const { return m_uiPackCount_PSP != 0; }
	TUINT GetPackCount() const { return m_uiPackCount_PSP; }
	TUINT GetSourceInstanceCount() const { return m_uiSourceInstanceCount_PSP; }
	TUINT GetModelCount() const { return m_uiModelCount_PSP; }
	TUINT GetTextureCount() const { return m_uiTextureCount_PSP; }
	const TCHAR* GetFailedPath() const { return m_szFailedPath_PSP; }

private:
	AFrontEndTerrainPack_PSP m_aPacks_PSP[ MAX_PACKS_PSP ];
	TUINT                    m_uiPackCount_PSP;
	TUINT                    m_uiSourceInstanceCount_PSP;
	TUINT                    m_uiModelCount_PSP;
	TUINT                    m_uiTextureCount_PSP;
	TCHAR                    m_szFailedPath_PSP[ 80 ];
};
