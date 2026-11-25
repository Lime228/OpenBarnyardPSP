#pragma once

#include <Math/TMatrix44.h>
#include <Platform/PSP/TAnimationClip_PSP.h>
#include <Platform/PSP/TRenderInterface_PSP.h>
#include <Platform/PSP/TSkinningBuffer_PSP.h>

struct ASimStart_PSP;

class ASimPlayer_PSP
{
public:
	TBOOL Load( const ASimStart_PSP& a_rStart, TUINT a_uiGender, TUINT a_uiCostume );
	void Unload();
	TBOOL Update( TFLOAT a_fDeltaTime );
	TUINT Render( Toshi::TRenderInterface_PSP& a_rRenderer, const Toshi::TRenderCamera_PSP& a_rCamera ) const;
	const TCHAR* GetFailedPath() const { return m_szPath; }

private:
	static constexpr TUINT NUM_MESHES = 2;

	TBOOL LoadAssets( const TCHAR* a_szRoot, TUINT a_uiCostume );
	TBOOL UpdatePose();

	Toshi::TSkeleton_PSP m_Skeleton;
	Toshi::TSkeletonInstance_PSP m_Pose;
	Toshi::TAnimationClip_PSP m_Idle;
	Toshi::TModel_PSP m_aModels[ NUM_MESHES ];
	Toshi::TSkinningBuffer_PSP m_aSkins[ NUM_MESHES ];
	Toshi::T2Texture m_aTextures[ NUM_MESHES ];
	Toshi::TMatrix44 m_World;
	TFLOAT m_fAnimationTime = 0.0f;
	TCHAR m_szPath[ 88 ] = {};
};
