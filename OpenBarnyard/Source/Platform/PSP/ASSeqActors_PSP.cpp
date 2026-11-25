#include "pch.h"
#include "ASSeqActors_PSP.h"

#include <File/TFile.h>
#include <Math/TMatrix44.h>
#include <Math/TQuaternion.h>
#include <Platform/PSP/TAnimationClipFactory_PSP.h>
#include <Platform/PSP/TModelFactory_PSP.h>
#include <Platform/PSP/TSkeletonFactory_PSP.h>
#include <Platform/PSP/TTextureFactory_PSP.h>

TOSHI_NAMESPACE_USING

static constexpr TCHAR s_szActorsRoot_PSP[] = "local:Data/ScriptedSequence/SS01/Actors";

static TBOOL ASetSSeqActorPose_PSP( TSkeletonInstance_PSP& a_rPose, const TAnimationClip_PSP& a_rClip,
                                  TFLOAT a_fProgress )
{
	const TFLOAT fTime = TFLOAT( a_rClip.GetFrameCount() - 1 ) * 1000.0f / TFLOAT( a_rClip.GetSampleRate() );
	return a_rPose.SetAnimationPose( a_rClip, TUINT( a_fProgress * fTime + 0.5f ) );
}

TBOOL ASSeqActors_PSP::Load()
{
	Unload();
	TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/actors.psac", s_szActorsRoot_PSP );
	TFile* pFile = TFile::Create( m_szPath, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TTRUE;

	struct Header
	{
		TUINT32 uiMagic;
		TUINT32 uiVersion;
		TUINT32 uiActors;
		TUINT32 uiShots;
	};
	static_assert( sizeof( Header ) == 16 && sizeof( ActorConfig ) == 20 );
	Header oHeader;
	const TBOOL bRead = pFile->Read( &oHeader, sizeof( oHeader ) ) == sizeof( oHeader ) &&
	                    oHeader.uiMagic == 0x43415350 && oHeader.uiVersion == 1 &&
	                    oHeader.uiActors != 0 && oHeader.uiActors <= MAX_ACTORS && oHeader.uiShots == SHOTS &&
	                    pFile->GetSize() == sizeof( oHeader ) + oHeader.uiActors * sizeof( ActorConfig ) &&
	                    pFile->Read( m_aConfig, oHeader.uiActors * sizeof( ActorConfig ) ) == oHeader.uiActors * sizeof( ActorConfig );
	pFile->Destroy();
	if ( !bRead )
		return TFALSE;

	m_uiActorCount = oHeader.uiActors;
	if ( !LoadActors() )
	{
		Unload();
		return TFALSE;
	}

	return TTRUE;
}

TBOOL ASSeqActors_PSP::LoadActors()
{
	for ( TUINT i = 0; i < m_uiActorCount; i++ )
	{
		const ActorConfig& rConfig = m_aConfig[ i ];
		if ( rConfig.uiShots == 0 || rConfig.uiShots >= ( 1U << SHOTS ) ||
		     rConfig.uiMeshes == 0 || rConfig.uiMeshes > MAX_MESHES )
			return TFALSE;
		for ( TUINT j = 0; j < rConfig.uiMeshes; j++ )
		{
			if ( rConfig.auiTextures[ j ] >= MAX_TEXTURES )
				return TFALSE;
		}
	}

	if ( !LoadClips( m_Rig, "actor-root", ( 1U << SHOTS ) - 1 ) )
		return TFALSE;

	const TModelFactory_PSP oModelFactory;
	const TTextureFactory_PSP oTextureFactory;
	for ( TUINT i = 0; i < m_uiActorCount; i++ )
	{
		Actor& rActor = m_aActors[ i ];
		const ActorConfig& rConfig = m_aConfig[ i ];
		TCHAR szPrefix[ 16 ];
		TStringManager::String8Format( szPrefix, TARRAYSIZE( szPrefix ), "actor-%u", i );
		if ( rConfig.uiSocket >= m_Rig.oSkeleton.GetBoneCount() || !LoadClips( rActor.oClips, szPrefix, rConfig.uiShots ) )
			return TFALSE;

		for ( TUINT j = 0; j < rConfig.uiMeshes; j++ )
		{
			TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/%s-%u.pmes", s_szActorsRoot_PSP, szPrefix, j );
			if ( !oModelFactory.CreateModelFromFile( rActor.aModels[ j ], m_szPath ) ||
			     !rActor.aSkins[ j ].Create( &rActor.aModels[ j ], &rActor.oClips.oSkeleton ) )
				return TFALSE;

			const TUINT uiTexture = rConfig.auiTextures[ j ];
			T2Texture& rTexture = m_aTextures[ uiTexture ];
			if ( rTexture.GetImageData() == TNULL )
			{
				TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/texture-%u.ptex", s_szActorsRoot_PSP, uiTexture );
				if ( !oTextureFactory.CreateTextureFromFile( rTexture, m_szPath ) )
					return TFALSE;
			}
		}
	}
	return TTRUE;
}

TBOOL ASSeqActors_PSP::LoadClips( ClipSet& a_rClips, const TCHAR* a_szPrefix, TUINT32 a_uiShots )
{
	const TSkeletonFactory_PSP oSkeletonFactory;
	const TAnimationClipFactory_PSP oAnimationFactory;
	TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/%s.pskl", s_szActorsRoot_PSP, a_szPrefix );
	if ( !oSkeletonFactory.CreateSkeletonFromFile( a_rClips.oSkeleton, m_szPath ) ||
	     !a_rClips.oPose.Create( &a_rClips.oSkeleton ) )
		return TFALSE;

	for ( TUINT i = 0; i < SHOTS; i++ )
	{
		if ( ( a_uiShots & ( 1U << i ) ) == 0 )
			continue;
		TAnimationClip_PSP& rClip = a_rClips.aClips[ i ];
		TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/%s-%u.panm", s_szActorsRoot_PSP, a_szPrefix, i );
		if ( !oAnimationFactory.CreateAnimationFromFile( rClip, &a_rClips.oSkeleton, m_szPath ) || rClip.IsLooping() )
			return TFALSE;
	}
	return TTRUE;
}

void ASSeqActors_PSP::UnloadClips( ClipSet& a_rClips )
{
	for ( TAnimationClip_PSP& rClip : a_rClips.aClips )
		rClip.Unload();
	a_rClips.oPose.Destroy();
	a_rClips.oSkeleton.Unload();
}

void ASSeqActors_PSP::Unload()
{
	for ( TUINT i = 0; i < m_uiActorCount; i++ )
	{
		Actor& rActor = m_aActors[ i ];
		for ( TUINT j = 0; j < MAX_MESHES; j++ )
		{
			rActor.aSkins[ j ].Destroy();
			rActor.aModels[ j ].Unload();
		}
		UnloadClips( rActor.oClips );
		rActor.bPoseReady = TFALSE;
	}
	for ( T2Texture& rTexture : m_aTextures )
		rTexture.Unload();
	UnloadClips( m_Rig );
	m_uiActorCount = 0;
	m_uiLastShot = SHOTS;
	m_bRigReady = TFALSE;
}

TUINT ASSeqActors_PSP::Render( TRenderInterface_PSP& a_rRenderer, const TRenderCamera_PSP& a_rCamera,
                             TUINT a_uiShot, TFLOAT a_fShotProgress )
{
	if ( m_uiActorCount == 0 || a_uiShot >= SHOTS )
		return 0;

	const TUINT uiTime = TUINT( a_fShotProgress * 65535.0f );
	const TBOOL bUpdate = a_uiShot != m_uiLastShot || uiTime != m_uiLastTime;
	if ( bUpdate )
	{
		m_uiLastShot = a_uiShot;
		m_uiLastTime = uiTime;
		m_bRigReady = ASetSSeqActorPose_PSP( m_Rig.oPose, m_Rig.aClips[ a_uiShot ], a_fShotProgress );
	}
	if ( !m_bRigReady )
		return 0;

	static const TRenderMaterial_PSP s_Material = {
		TTRUE, TRENDERTEXTUREFUNCTION_PSP_MODULATE, TRENDERCULLMODE_PSP_NONE, TRENDERALPHAMODE_PSP_OPAQUE,
		TRENDERTEXTUREADDRESS_PSP_WRAP, TRENDERTEXTUREADDRESS_PSP_WRAP,
	};
	TMatrix44 oBase;
	oBase.Identity();
	oBase.RotateX( TMath::HALF_PI ); // ASSeqAttachmentManager's transform for SS01.
	TUINT uiDraws = 0;
	for ( TUINT i = 0; i < m_uiActorCount; i++ )
	{
		const ActorConfig& rConfig = m_aConfig[ i ];
		if ( ( rConfig.uiShots & ( 1U << a_uiShot ) ) == 0 )
			continue;

		Actor& rActor = m_aActors[ i ];
		if ( bUpdate )
		{
			rActor.bPoseReady = ASetSSeqActorPose_PSP( rActor.oClips.oPose, rActor.oClips.aClips[ a_uiShot ], a_fShotProgress );
			for ( TUINT j = 0; j < rConfig.uiMeshes && rActor.bPoseReady; j++ )
				rActor.bPoseReady = rActor.aSkins[ j ].Skin( rActor.oClips.oPose );
		}
		if ( !rActor.bPoseReady )
			continue;

		const TSkeletonTransform_PSP& rSocket = *m_Rig.oPose.GetBoneTransform( rConfig.uiSocket );
		const TFLOAT* pRotation = rSocket.aRotation;
		const TFLOAT* pPosition = rSocket.aTranslation;
		TMatrix44 oLocal;
		oLocal.SetFromQuaternion( TQuaternion( pRotation[ 0 ], pRotation[ 1 ], pRotation[ 2 ], pRotation[ 3 ] ) );
		oLocal.SetTranslation( TVector4( pPosition[ 0 ], pPosition[ 1 ], pPosition[ 2 ], 1.0f ) );
		TMatrix44 oWorld;
		oWorld.Multiply( oBase, oLocal );

		for ( TUINT j = 0; j < rConfig.uiMeshes; j++ )
		{
			const auto* pVertices = rActor.aSkins[ j ].GetVertices();
			if ( pVertices != TNULL && a_rRenderer.DrawSkinnedModel(
			         rActor.aModels[ j ], *pVertices, oWorld, s_Material, &m_aTextures[ rConfig.auiTextures[ j ] ], a_rCamera ) )
				uiDraws++;
		}
	}
	return uiDraws;
}
