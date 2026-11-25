#pragma once

#include <Platform/PSP/TAnimationClip_PSP.h>
#include <Platform/PSP/TRenderInterface_PSP.h>
#include <Platform/PSP/TSkinningBuffer_PSP.h>

class ASSeqActors_PSP
{
public:
	TBOOL Load();
	void  Unload();
	TUINT Render( Toshi::TRenderInterface_PSP& a_rRenderer, const Toshi::TRenderCamera_PSP& a_rCamera,
	              TUINT a_uiShot, TFLOAT a_fShotProgress );
	const TCHAR* GetFailedPath() const { return m_szPath; }

private:
	static constexpr TUINT MAX_ACTORS   = 4;
	static constexpr TUINT MAX_TEXTURES = 5;
	static constexpr TUINT MAX_MESHES   = 2;
	static constexpr TUINT SHOTS        = 2;

	struct ClipSet
	{
		Toshi::TSkeleton_PSP oSkeleton;
		Toshi::TSkeletonInstance_PSP oPose;
		Toshi::TAnimationClip_PSP aClips[ SHOTS ];
	};

	struct ActorConfig
	{
		TUINT32 uiSocket;
		TUINT32 uiShots;
		TUINT32 uiMeshes;
		TUINT32 auiTextures[ MAX_MESHES ];
	};

	struct Actor
	{
		ClipSet oClips;
		Toshi::TModel_PSP aModels[ MAX_MESHES ];
		Toshi::TSkinningBuffer_PSP aSkins[ MAX_MESHES ];
		TBOOL bPoseReady;
	};

	TBOOL LoadActors();
	TBOOL LoadClips( ClipSet& a_rClips, const TCHAR* a_szPrefix, TUINT32 a_uiShots );
	static void UnloadClips( ClipSet& a_rClips );

	ClipSet m_Rig;
	ActorConfig m_aConfig[ MAX_ACTORS ];
	Actor m_aActors[ MAX_ACTORS ];
	Toshi::T2Texture m_aTextures[ MAX_TEXTURES ];
	TUINT m_uiActorCount = 0;
	TUINT m_uiLastShot = SHOTS;
	TUINT m_uiLastTime = 0;
	TBOOL m_bRigReady = TFALSE;
	TCHAR m_szPath[ 128 ] = {};
};
