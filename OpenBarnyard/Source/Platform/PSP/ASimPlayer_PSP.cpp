#include "pch.h"
#include "ASimPlayer_PSP.h"
#include "ASimStart_PSP.h"

#include <File/TFile.h>
#include <Math/TMathInline.h>
#include <Platform/PSP/TAnimationClipFactory_PSP.h>
#include <Platform/PSP/TModelFactory_PSP.h>
#include <Platform/PSP/TSkeletonFactory_PSP.h>
#include <Platform/PSP/TTextureFactory_PSP.h>

TOSHI_NAMESPACE_USING

TBOOL ASimPlayer_PSP::Load( const ASimStart_PSP& a_rStart, TUINT a_uiGender, TUINT a_uiCostume )
{
	Unload();
	static const TCHAR* const s_aszRoots[] = { "local:Data/Player/male", "local:Data/Player/female" };
	if ( a_uiGender >= TARRAYSIZE( s_aszRoots ) || a_uiCostume >= 7 )
	{
		TStringManager::String8Copy( m_szPath, "envmain-player-profile", TARRAYSIZE( m_szPath ) );
		return TFALSE;
	}

	// Match AModelInstance::CreateRotationFromDirection for an upright unit.
	TVector4 vForward;
	vForward.Negate3( a_rStart.vFacing );
	vForward.NormaliseXZ();
	TVector4 vRight;
	vRight.CrossProduct( vForward, TVector4::VEC_NEGY );
	vRight.Normalise();
	TVector4 vUp;
	vUp.CrossProduct( vRight, vForward );
	vUp.Normalise();
	m_World.Identity();
	m_World.SetBasisVector4( BASISVECTOR_RIGHT, TVector4( vRight.x, vRight.y, vRight.z, 0.0f ) );
	m_World.SetBasisVector4( BASISVECTOR_UP, TVector4( vForward.x, vForward.y, vForward.z, 0.0f ) );
	m_World.SetBasisVector4( BASISVECTOR_FORWARD, TVector4( vUp.x, vUp.y, vUp.z, 0.0f ) );
	m_World.SetTranslation( a_rStart.vPosition );

	if ( !LoadAssets( s_aszRoots[ a_uiGender ], a_uiCostume ) )
	{
		Unload();
		return TFALSE;
	}
	m_szPath[ 0 ] = '\0';
	return TTRUE;
}

TBOOL ASimPlayer_PSP::LoadAssets( const TCHAR* a_szRoot, TUINT a_uiCostume )
{
	TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/player.pspl", a_szRoot );
	TFile* pFile = TFile::Create( m_szPath, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TFALSE;
	TUINT32 aConfig[ 3 ];
	const TBOOL bRead = pFile->GetSize() == sizeof( aConfig ) && pFile->Read( aConfig, sizeof( aConfig ) ) == sizeof( aConfig );
	pFile->Destroy();
	if ( !bRead || aConfig[ 0 ] != 0x4c505350 || aConfig[ 1 ] != 1 || aConfig[ 2 ] >= NUM_MESHES )
		return TFALSE;

	const TSkeletonFactory_PSP oSkeletonFactory;
	TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/skeleton.pskl", a_szRoot );
	if ( !oSkeletonFactory.CreateSkeletonFromFile( m_Skeleton, m_szPath ) || !m_Pose.Create( &m_Skeleton ) )
		return TFALSE;
	const TAnimationClipFactory_PSP oAnimationFactory;
	TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/idle.panm", a_szRoot );
	if ( !oAnimationFactory.CreateAnimationFromFile( m_Idle, &m_Skeleton, m_szPath ) || !m_Idle.IsLooping() )
		return TFALSE;

	const TModelFactory_PSP oModelFactory;
	const TTextureFactory_PSP oTextureFactory;
	for ( TUINT i = 0; i < NUM_MESHES; i++ )
	{
		TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/mesh-%03u.pmes", a_szRoot, i );
		if ( !oModelFactory.CreateModelFromFile( m_aModels[ i ], m_szPath ) || !m_aSkins[ i ].Create( &m_aModels[ i ], &m_Skeleton ) )
			return TFALSE;
		if ( i == aConfig[ 2 ] )
			TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/costume-%u.ptex", a_szRoot, a_uiCostume );
		else
			TStringManager::String8Format( m_szPath, TARRAYSIZE( m_szPath ), "%s/mesh-%03u.ptex", a_szRoot, i );
		if ( !oTextureFactory.CreateTextureFromFile( m_aTextures[ i ], m_szPath ) )
			return TFALSE;
	}
	return UpdatePose();
}

void ASimPlayer_PSP::Unload()
{
	for ( TUINT i = 0; i < NUM_MESHES; i++ )
	{
		m_aSkins[ i ].Destroy();
		m_aModels[ i ].Unload();
		m_aTextures[ i ].Unload();
	}
	m_Idle.Unload();
	m_Pose.Destroy();
	m_Skeleton.Unload();
	m_fAnimationTime = 0.0f;
}

TBOOL ASimPlayer_PSP::Update( TFLOAT a_fDeltaTime )
{
	if ( !TMath::IsFinite( a_fDeltaTime ) || a_fDeltaTime <= 0.0f )
		return TTRUE;
	const TFLOAT fDuration = TFLOAT( m_Idle.GetFrameCount() ) / m_Idle.GetSampleRate();
	m_fAnimationTime += a_fDeltaTime;
	m_fAnimationTime -= TMath::FloorToInt( m_fAnimationTime / fDuration ) * fDuration;
	return UpdatePose();
}

TBOOL ASimPlayer_PSP::UpdatePose()
{
	if ( !m_Pose.SetAnimationPose( m_Idle, TUINT( m_fAnimationTime * 1000.0f ) ) )
		return TFALSE;
	for ( TSkinningBuffer_PSP& rSkin : m_aSkins )
	{
		if ( !rSkin.Skin( m_Pose ) )
			return TFALSE;
	}
	return TTRUE;
}

TUINT ASimPlayer_PSP::Render( TRenderInterface_PSP& a_rRenderer, const TRenderCamera_PSP& a_rCamera ) const
{
	static const TRenderMaterial_PSP s_Material = {
		TTRUE, TRENDERTEXTUREFUNCTION_PSP_MODULATE, TRENDERCULLMODE_PSP_NONE, TRENDERALPHAMODE_PSP_OPAQUE,
		TRENDERTEXTUREADDRESS_PSP_WRAP, TRENDERTEXTUREADDRESS_PSP_WRAP,
	};
	TUINT uiDraws = 0;
	for ( TUINT i = 0; i < NUM_MESHES; i++ )
	{
		const auto* pVertices = m_aSkins[ i ].GetVertices();
		if ( pVertices != TNULL && a_rRenderer.DrawSkinnedModel( m_aModels[ i ], *pVertices, m_World, s_Material, &m_aTextures[ i ], a_rCamera ) )
			uiDraws++;
	}
	return uiDraws;
}
