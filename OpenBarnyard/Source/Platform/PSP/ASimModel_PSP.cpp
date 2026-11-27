#include "pch.h"
#include "ASimModel_PSP.h"

#include <Platform/PSP/TModelFactory_PSP.h>
#include <Platform/PSP/TSkeletonFactory_PSP.h>
#include <Platform/PSP/TTextureFactory_PSP.h>
#include <Toshi/TStringManager.h>

TOSHI_NAMESPACE_USING

TBOOL ASimModel_PSP::LoadRig( const TCHAR* a_szRoot, TCHAR* a_szPath, TSIZE a_uiPathCapacity )
{
	const TSkeletonFactory_PSP oFactory;
	TStringManager::String8Format( a_szPath, a_uiPathCapacity, "%s/skeleton.pskl", a_szRoot );
	return oFactory.CreateSkeletonFromFile( m_Skeleton, a_szPath ) && m_Pose.Create( &m_Skeleton );
}

TBOOL ASimModel_PSP::LoadMeshes( const TCHAR* a_szRoot, TCHAR* a_szPath, TSIZE a_uiPathCapacity,
                               TINT a_iCostumeMesh, TUINT a_uiCostume )
{
	const TModelFactory_PSP oModelFactory;
	const TTextureFactory_PSP oTextureFactory;
	for ( TUINT i = 0; i < NUM_MESHES; i++ )
	{
		TStringManager::String8Format( a_szPath, a_uiPathCapacity, "%s/mesh-%03u.pmes", a_szRoot, i );
		if ( !oModelFactory.CreateModelFromFile( m_aModels[ i ], a_szPath ) || !m_aSkins[ i ].Create( &m_aModels[ i ], &m_Skeleton ) )
			return TFALSE;

		if ( TINT( i ) == a_iCostumeMesh )
			TStringManager::String8Format( a_szPath, a_uiPathCapacity, "%s/costume-%u.ptex", a_szRoot, a_uiCostume );
		else
			TStringManager::String8Format( a_szPath, a_uiPathCapacity, "%s/mesh-%03u.ptex", a_szRoot, i );
		if ( !oTextureFactory.CreateTextureFromFile( m_aTextures[ i ], a_szPath ) )
			return TFALSE;
	}
	return TTRUE;
}

void ASimModel_PSP::Unload()
{
	for ( TUINT i = 0; i < NUM_MESHES; i++ )
	{
		m_aSkins[ i ].Destroy();
		m_aModels[ i ].Unload();
		m_aTextures[ i ].Unload();
	}
	m_Pose.Destroy();
	m_Skeleton.Unload();
}

void ASimModel_PSP::SetTransform( const TVector4& a_rPosition, const TVector4& a_rFacing )
{
	// AModelInstance::CreateRotationFromDirection for an upright unit.
	TVector4 vForward;
	vForward.Negate3( a_rFacing );
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
	m_World.SetTranslation( a_rPosition );
}

TBOOL ASimModel_PSP::UpdatePose( const TAnimationClip_PSP& a_rClip, TUINT a_uiTimeMilliseconds )
{
	if ( !m_Pose.SetAnimationPose( a_rClip, a_uiTimeMilliseconds ) )
		return TFALSE;
	for ( TSkinningBuffer_PSP& rSkin : m_aSkins )
	{
		if ( !rSkin.Skin( m_Pose ) )
			return TFALSE;
	}
	return TTRUE;
}

TUINT ASimModel_PSP::Render( TRenderInterface_PSP& a_rRenderer, const TRenderCamera_PSP& a_rCamera ) const
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
