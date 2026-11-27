#pragma once

#include <Math/TMatrix44.h>
#include <Platform/PSP/TAnimationClip_PSP.h>
#include <Platform/PSP/TRenderInterface_PSP.h>
#include <Platform/PSP/TSkinningBuffer_PSP.h>

class ASimModel_PSP
{
public:
	static constexpr TUINT NUM_MESHES = 2;

	TBOOL LoadRig( const TCHAR* a_szRoot, TCHAR* a_szPath, TSIZE a_uiPathCapacity );
	TBOOL LoadMeshes( const TCHAR* a_szRoot, TCHAR* a_szPath, TSIZE a_uiPathCapacity,
	                  TINT a_iCostumeMesh = -1, TUINT a_uiCostume = 0 );
	void Unload();
	void SetTransform( const Toshi::TVector4& a_rPosition, const Toshi::TVector4& a_rFacing );
	TBOOL UpdatePose( const Toshi::TAnimationClip_PSP& a_rClip, TUINT a_uiTimeMilliseconds );
	TUINT Render( Toshi::TRenderInterface_PSP& a_rRenderer, const Toshi::TRenderCamera_PSP& a_rCamera ) const;
	const Toshi::TSkeleton_PSP* GetSkeleton() const { return &m_Skeleton; }

private:
	Toshi::TSkeleton_PSP m_Skeleton;
	Toshi::TSkeletonInstance_PSP m_Pose;
	Toshi::TModel_PSP m_aModels[ NUM_MESHES ];
	Toshi::TSkinningBuffer_PSP m_aSkins[ NUM_MESHES ];
	Toshi::T2Texture m_aTextures[ NUM_MESHES ];
	Toshi::TMatrix44 m_World;
};
