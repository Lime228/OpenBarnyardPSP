#pragma once

#include <Platform/PSP/TModelFactory_PSP.h>
#include <Platform/PSP/TRenderInterface_PSP.h>
#include <Platform/PSP/TTextureFactory_PSP.h>

class AFrontEndTerrain_PSP
{
public:
	static constexpr TUINT MAX_MESHES_PSP = 4;

public:
	AFrontEndTerrain_PSP();
	~AFrontEndTerrain_PSP();

	TBOOL Load();
	void  Unload();
	TUINT Render( Toshi::TRenderInterface_PSP& a_rRenderInterface ) const;

	TBOOL IsLoaded() const { return m_uiMeshCount_PSP == MAX_MESHES_PSP; }
	TUINT GetMeshCount() const { return m_uiMeshCount_PSP; }
	const Toshi::TRenderModelTransform_PSP& GetModelTransform() const { return m_ModelTransform_PSP; }
	const TCHAR* GetFailedPath() const { return m_szFailedPath_PSP; }

private:
	TBOOL CalculateModelTransform_PSP();

private:
	Toshi::TModelFactory_PSP         m_ModelFactory_PSP;
	Toshi::TTextureFactory_PSP       m_TextureFactory_PSP;
	Toshi::TModel_PSP                m_aModels_PSP[ MAX_MESHES_PSP ];
	Toshi::T2Texture                 m_aTextures_PSP[ MAX_MESHES_PSP ];
	Toshi::TRenderModelTransform_PSP m_ModelTransform_PSP;
	TUINT                            m_uiMeshCount_PSP;
	TCHAR                            m_szFailedPath_PSP[ 64 ];
};
