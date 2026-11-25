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
                                  TFLOAT a_fProgress, TFLOAT a_fShotDuration )
{
	const TFLOAT fTime = a_rClip.IsLooping() ? a_fShotDuration * 1000.0f :
	                     TFLOAT( a_rClip.GetFrameCount() - 1 ) * 1000.0f / TFLOAT( a_rClip.GetSampleRate() );
	return a_rPose.SetAnimationPose( a_rClip, TUINT( a_fProgress * fTime + 0.5f ) );
}

TBOOL ASSeqActors_PSP::Load( TUINT a_uiShotCount )
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
	static_assert( sizeof( Header ) == 16 && sizeof( ActorConfig ) == 24 );
	Header oHeader = {};
	TBOOL bRead = pFile->Read( &oHeader, sizeof( oHeader ) ) == sizeof( oHeader ) &&
	              oHeader.uiMagic == 0x43415350 && ( oHeader.uiVersion == 1 || oHeader.uiVersion == 2 ) &&
	              oHeader.uiActors != 0 && oHeader.uiActors <= MAX_ACTORS &&
	              oHeader.uiShots == a_uiShotCount && oHeader.uiShots >= 2 && oHeader.uiShots < 32;
	const TSIZE uiRecordSize = oHeader.uiVersion == 1 ? 20 : sizeof( ActorConfig );
	bRead = bRead && pFile->GetSize() == sizeof( oHeader ) + oHeader.uiActors * uiRecordSize;
	for ( TUINT i = 0; bRead && i < oHeader.uiActors; i++ )
	{
		m_aConfig[ i ] = {};
		bRead = pFile->Read( &m_aConfig[ i ], uiRecordSize ) == uiRecordSize &&
		        ( oHeader.uiVersion == 2 || m_aConfig[ i ].uiMeshes <= 2 );
	}
	pFile->Destroy();
	if ( !bRead )
		return TFALSE;

	m_uiActorCount = oHeader.uiActors;
	m_uiShotCount = oHeader.uiShots;
	if ( !LoadSkeleton( m_Rig, "actor-root" ) )
	{
		Unload();
		return TFALSE;
	}

	for ( TUINT i = 0; i < m_uiActorCount; i++ )
	{
		const ActorConfig& rConfig = m_aConfig[ i ];
		if ( rConfig.uiSocket >= m_Rig.oSkeleton.GetBoneCount() ||
		     rConfig.uiShots == 0 || rConfig.uiShots >= ( 1U << m_uiShotCount ) ||
		     rConfig.uiMeshes == 0 || rConfig.uiMeshes > MAX_MESHES )
		{
			Unload();
			return TFALSE;
		}
		for ( TUINT j = 0; j < rConfig.uiMeshes; j++ )
		{
			if ( rConfig.auiTextures[ j ] >= MAX_TEXTURES )
			{
				Unload();
				return TFALSE;
			}
		}
	}

	if ( !SetShot( 0 ) )
	{
		Unload();
		return TFALSE;
	}
	return TTRUE;
}

TBOOL ASSeqActors_PSP::SetShot( TUINT a_uiShot )
{
	if ( m_uiActorCount == 0 )
		return TTRUE;
	if ( a_uiShot >= m_uiShotCount )
		return TFALSE;
	if ( a_uiShot == m_uiShot && m_Rig.oClip.IsLoaded() )
		return TTRUE;

	const TUINT uiMask = 1U << a_uiShot;
	for ( Actor& rActor : m_aActors )
	{
		if ( rActor.iConfigIndex >= 0 && ( m_aConfig[ rActor.iConfigIndex ].uiShots & uiMask ) == 0 )
			UnloadActor( rActor );
	}

	TBOOL abUsedTextures[ MAX_TEXTURES ] = {};
	for ( TUINT i = 0; i < m_uiActorCount; i++ )
	{
		const ActorConfig& rConfig = m_aConfig[ i ];
		if ( ( rConfig.uiShots & uiMask ) != 0 )
		{
			for ( TUINT j = 0; j < rConfig.uiMeshes; j++ )
				abUsedTextures[ rConfig.auiTextures[ j ] ] = TTRUE;
		}
	}
	for ( TUINT i = 0; i < MAX_TEXTURES; i++ )
	{
		if ( !abUsedTextures[ i ] )
			m_aTextures[ i ].Unload();
	}

	if ( !LoadClip( m_Rig, "actor-root", a_uiShot ) || m_Rig.oClip.IsLooping() )
		return TFALSE;
	for ( TUINT i = 0; i < m_uiActorCount; i++ )
	{
		if ( ( m_aConfig[ i ].uiShots & uiMask ) == 0 )
			continue;

		Actor* pActor = TNULL;
		Actor* pFreeActor = TNULL;
		for ( Actor& rActor : m_aActors )
		{
			if ( rActor.iConfigIndex == TINT( i ) )
			{
				pActor = &rActor;
				break;
			}
			if ( rActor.iConfigIndex < 0 && pFreeActor == TNULL )
				pFreeActor = &rActor;
		}
		if ( pActor == TNULL )
			pActor = pFreeActor;
		if ( pActor == TNULL || !LoadActor( *pActor, i, a_uiShot ) )
			return TFALSE;
	}

	m_uiShot = a_uiShot;
	m_bRigReady = TFALSE;
	m_uiLastTime = ~TUINT( 0 );
	return TTRUE;
}

TBOOL ASSeqActors_PSP::LoadActor( Actor& a_rActor, TUINT a_uiActor, TUINT a_uiShot )
{
	a_rActor.iConfigIndex = TINT( a_uiActor );
	const ActorConfig& rConfig = m_aConfig[ a_uiActor ];
	TCHAR szPrefix[ 16 ];
	TStringManager::String8Format( szPrefix, TARRAYSIZE( szPrefix ), "actor-%u", a_uiActor );
	if ( !a_rActor.oClips.oSkeleton.IsLoaded() && !LoadSkeleton( a_rActor.oClips, szPrefix ) )
		return TFALSE;
	if ( !LoadClip( a_rActor.oClips, szPrefix, a_uiShot ) )
		return TFALSE;

	const TModelFactory_PSP oModelFactory;
	const TTextureFactory_PSP oTextureFactory;
	for ( TUINT i = 0; i < rConfig.uiMeshes; i++ )
	{
		if ( !a_rActor.aSkins[ i ].IsCreated() )
		{
			TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/%s-%u.pmes", s_szActorsRoot_PSP, szPrefix, i );
			if ( !oModelFactory.CreateModelFromFile( a_rActor.aModels[ i ], m_szPath ) ||
			     !a_rActor.aSkins[ i ].Create( &a_rActor.aModels[ i ], &a_rActor.oClips.oSkeleton ) )
				return TFALSE;
		}

		const TUINT uiTexture = rConfig.auiTextures[ i ];
		T2Texture& rTexture = m_aTextures[ uiTexture ];
		if ( rTexture.GetImageData() == TNULL )
		{
			TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/texture-%u.ptex", s_szActorsRoot_PSP, uiTexture );
			if ( !oTextureFactory.CreateTextureFromFile( rTexture, m_szPath ) )
				return TFALSE;
		}
	}
	return TTRUE;
}

TBOOL ASSeqActors_PSP::LoadSkeleton( ClipSet& a_rClips, const TCHAR* a_szPrefix )
{
	const TSkeletonFactory_PSP oSkeletonFactory;
	TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/%s.pskl", s_szActorsRoot_PSP, a_szPrefix );
	return oSkeletonFactory.CreateSkeletonFromFile( a_rClips.oSkeleton, m_szPath ) &&
	       a_rClips.oPose.Create( &a_rClips.oSkeleton );
}

TBOOL ASSeqActors_PSP::LoadClip( ClipSet& a_rClips, const TCHAR* a_szPrefix, TUINT a_uiShot )
{
	const TAnimationClipFactory_PSP oAnimationFactory;
	TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/%s-%u.panm", s_szActorsRoot_PSP, a_szPrefix, a_uiShot );
	a_rClips.oClip.Unload();
	return oAnimationFactory.CreateAnimationFromFile( a_rClips.oClip, &a_rClips.oSkeleton, m_szPath );
}

void ASSeqActors_PSP::UnloadActor( Actor& a_rActor )
{
	for ( TUINT i = 0; i < MAX_MESHES; i++ )
	{
		a_rActor.aSkins[ i ].Destroy();
		a_rActor.aModels[ i ].Unload();
	}
	UnloadClips( a_rActor.oClips );
	a_rActor.iConfigIndex = -1;
	a_rActor.bPoseReady = TFALSE;
}

void ASSeqActors_PSP::UnloadClips( ClipSet& a_rClips )
{
	a_rClips.oClip.Unload();
	a_rClips.oPose.Destroy();
	a_rClips.oSkeleton.Unload();
}

void ASSeqActors_PSP::Unload()
{
	for ( Actor& rActor : m_aActors )
		UnloadActor( rActor );
	for ( T2Texture& rTexture : m_aTextures )
		rTexture.Unload();
	UnloadClips( m_Rig );
	m_uiActorCount = 0;
	m_uiShotCount = 0;
	m_uiShot = 0;
	m_bRigReady = TFALSE;
}

TUINT ASSeqActors_PSP::Render( TRenderInterface_PSP& a_rRenderer, const TRenderCamera_PSP& a_rCamera,
                             TUINT a_uiShot, TFLOAT a_fShotProgress, TFLOAT a_fShotDuration )
{
	if ( m_uiActorCount == 0 || a_uiShot != m_uiShot )
		return 0;

	const TUINT uiTime = TUINT( a_fShotProgress * 65535.0f );
	const TBOOL bUpdate = uiTime != m_uiLastTime;
	if ( bUpdate )
	{
		m_uiLastTime = uiTime;
		m_bRigReady = ASetSSeqActorPose_PSP( m_Rig.oPose, m_Rig.oClip, a_fShotProgress, a_fShotDuration );
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
	for ( Actor& rActor : m_aActors )
	{
		if ( rActor.iConfigIndex < 0 )
			continue;

		const ActorConfig& rConfig = m_aConfig[ rActor.iConfigIndex ];
		if ( bUpdate )
		{
			rActor.bPoseReady = ASetSSeqActorPose_PSP( rActor.oClips.oPose, rActor.oClips.oClip, a_fShotProgress, a_fShotDuration );
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
