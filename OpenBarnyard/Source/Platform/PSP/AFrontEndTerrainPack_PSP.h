#pragma once

#include <Platform/PSP/TModelFactory_PSP.h>
#include <Platform/PSP/TRenderInterface_PSP.h>
#include <Platform/PSP/TTextureFactory_PSP.h>

class AFrontEndTerrainPack_PSP
{
public:
	static constexpr TUINT MAX_MODELS_PSP = 48;
	static constexpr TUINT MAX_TEXTURES_PSP = 48;
	enum LightProfile_PSP : TUINT32
	{
		LIGHTPROFILE_NONE_PSP,
		LIGHTPROFILE_BARN_PSP,
		LIGHTPROFILE_CONE_PSP,
		LIGHTPROFILE_NUMOF_PSP,
	};

public:
	AFrontEndTerrainPack_PSP();
	~AFrontEndTerrainPack_PSP();

	TBOOL Load( const TCHAR* a_szPrefix );
	void  Unload();
	TUINT Render(
		Toshi::TRenderInterface_PSP& a_rRenderInterface,
		const Toshi::TRenderCamera_PSP* a_pCamera,
		TFLOAT a_fHour
	) const;

	TBOOL IsLoaded() const { return m_uiModelCount_PSP != 0; }
	TUINT GetModelCount() const { return m_uiModelCount_PSP; }
	TUINT GetTextureCount() const { return m_uiTextureCount_PSP; }
	const TCHAR* GetFailedPath() const { return m_szFailedPath_PSP; }

private:
	struct MaterialEntry_PSP
	{
		TUINT                            uiModelIndex;
		TUINT                            uiTextureIndex;
		Toshi::TRenderAlphaMode_PSP      eAlphaMode;
		Toshi::TRenderTextureAddress_PSP eAddressU;
		Toshi::TRenderTextureAddress_PSP eAddressV;
		LightProfile_PSP                  eLightProfile;
	};

private:
	Toshi::TModelFactory_PSP   m_ModelFactory_PSP;
	Toshi::TTextureFactory_PSP m_TextureFactory_PSP;
	Toshi::TModel_PSP          m_aModels_PSP[ MAX_MODELS_PSP ];
	Toshi::T2Texture           m_aTextures_PSP[ MAX_TEXTURES_PSP ];
	MaterialEntry_PSP          m_aEntries_PSP[ MAX_MODELS_PSP ];
	TUINT                      m_uiModelCount_PSP;
	TUINT                      m_uiTextureCount_PSP;
	TCHAR                      m_szPrefix_PSP[ 32 ];
	TCHAR                      m_szFailedPath_PSP[ 80 ];
};
