#pragma once

#include <Platform/PSP/TModelFactory_PSP.h>
#include <Platform/PSP/TRenderInterface_PSP.h>
#include <Platform/PSP/TTextureFactory_PSP.h>

class ASSeqTerrain_PSP
{
public:
	static constexpr TUINT MAX_MESHES = 512;
	static constexpr TUINT MAX_TEXTURES = 256;

	static TBOOL HasSS01Pack();
	TBOOL LoadSS01();
	void Unload();
	TUINT Render( Toshi::TRenderInterface_PSP& a_rRenderInterface, const Toshi::TRenderCamera_PSP& a_rCamera ) const;

	TUINT GetMeshCount() const { return m_uiMeshCount; }
	TUINT GetTextureCount() const { return m_uiTextureCount; }
	const TCHAR* GetFailedPath() const { return m_szPath; }

private:
	struct MaterialEntry
	{
		TUINT32 uiTexture;
		TUINT32 uiAlpha;
		TUINT32 uiAddressU;
		TUINT32 uiAddressV;
	};

	Toshi::TModel_PSP m_aMeshes[ MAX_MESHES ];
	Toshi::T2Texture m_aTextures[ MAX_TEXTURES ];
	MaterialEntry m_aMaterials[ MAX_MESHES ];
	TUINT m_uiMeshCount = 0;
	TUINT m_uiTextureCount = 0;
	TCHAR m_szPath[ 80 ] = {};
};
