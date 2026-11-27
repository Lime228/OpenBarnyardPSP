#include "pch.h"
#include "AShadowManager_PSP.h"
#include "ATerrainCollision_PSP.h"

#include <File/TFile.h>
#include <Math/TMatrix44.h>
#include <Platform/PSP/TTextureFactory_PSP.h>

TOSHI_NAMESPACE_USING

static constexpr TCHAR s_szShadowConfig_PSP[] = "local:Data/Particles/shadow.pshs";
static constexpr TCHAR s_szShadowTexture_PSP[] = "local:Data/Particles/shadow.ptex";
static constexpr TFLOAT s_fCharacterShadowScale_PSP = 1.25f;
static constexpr TFLOAT s_fCharacterShadowHeight_PSP = -0.1f;
static constexpr TFLOAT s_fShadowMaxCameraDistance_PSP = 30.0f;

TBOOL AShadowManager_PSP::Load()
{
	Unload();
	TFile* pFile = TFile::Create( s_szShadowConfig_PSP, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TTRUE;

	m_szPath = s_szShadowConfig_PSP;
	TUINT32 aHeader[ 2 ];
	TFLOAT fSize;
	const TBOOL bRead = pFile->GetSize() == sizeof( aHeader ) + sizeof( fSize ) &&
	                    pFile->Read( aHeader, sizeof( aHeader ) ) == sizeof( aHeader ) &&
	                    aHeader[ 0 ] == 0x53485350 && aHeader[ 1 ] == 1 &&
	                    pFile->Read( &fSize, sizeof( fSize ) ) == sizeof( fSize ) && fSize > 0.0f && fSize <= 8.0f;
	pFile->Destroy();
	if ( !bRead )
		return TFALSE;

	m_szPath = s_szShadowTexture_PSP;
	const TTextureFactory_PSP oTextureFactory;
	if ( !oTextureFactory.CreateTextureFromFile( m_Texture, s_szShadowTexture_PSP ) )
		return TFALSE;
	m_fHalfSize = 0.5f * fSize * s_fCharacterShadowScale_PSP;
	m_szPath = TNULL;
	return TTRUE;
}

void AShadowManager_PSP::Unload()
{
	m_Texture.Unload();
	m_fHalfSize = 0.0f;
	m_szPath = TNULL;
}

void AShadowManager_PSP::Update( Shadow& a_rShadow, const TVector4& a_rPosition, const TVector4& a_rFacing,
                                const ATerrainCollision_PSP& a_rTerrain ) const
{
	if ( m_fHalfSize == 0.0f )
		return;

	const TUINT uiMask = a_rTerrain.GetActivePackMask();
	if ( a_rShadow.uiMask == uiMask && TVector4::DistanceSqXZ( a_rPosition, a_rShadow.vPosition ) < 0.0001f )
		return;
	a_rShadow.vPosition = a_rPosition;
	a_rShadow.uiMask = uiMask;
	a_rShadow.bReady = TFALSE;

	// ATerrainInterface::GetGroundShadowInfo starts just above the model origin.
	const TFLOAT fRayY = a_rPosition.y - 0.2f;
	TFLOAT fDistance = 10.0f;
	TVector3 vNormal;
	if ( !a_rTerrain.CollideVertical( a_rPosition.x, a_rPosition.z, fRayY, TTRUE, fDistance, &vNormal ) ||
	     vNormal.y >= -0.4f )
		return;

	TMatrix44 oTransform;
	oTransform.Identity();
	oTransform.SetTranslation( TVector4( a_rPosition.x, fRayY + fDistance + s_fCharacterShadowHeight_PSP, a_rPosition.z, 1.0f ) );
	oTransform.LookAtDirection( TVector4( vNormal.x, vNormal.y, vNormal.z, 0.0f ), a_rFacing );
	oTransform.RotateX( -TMath::HALF_PI );

	const TVector3& rCentre = oTransform.AsBasisVector3( BASISVECTOR_TRANSLATION );
	TVector3 vSide = oTransform.AsBasisVector3( BASISVECTOR_RIGHT );
	TVector3 vForward = oTransform.AsBasisVector3( BASISVECTOR_FORWARD );
	vSide.Multiply( m_fHalfSize );
	vForward.Multiply( m_fHalfSize );
	a_rShadow.aCorners[ 0 ] = rCentre - vSide - vForward;
	a_rShadow.aCorners[ 1 ] = rCentre + vSide - vForward;
	a_rShadow.aCorners[ 2 ] = rCentre - vSide + vForward;
	a_rShadow.aCorners[ 3 ] = rCentre + vSide + vForward;
	a_rShadow.bReady = TTRUE;
}

TBOOL AShadowManager_PSP::Render( const Shadow& a_rShadow, TRenderInterface_PSP& a_rRenderer,
                                 const TRenderCamera_PSP& a_rCamera ) const
{
	if ( !a_rShadow.bReady )
		return TFALSE;
	const TVector4 vCamera( a_rCamera.aPosition[ 0 ], a_rCamera.aPosition[ 1 ], a_rCamera.aPosition[ 2 ], 1.0f );
	if ( TVector4::DistanceSq( a_rShadow.vPosition, vCamera ) >= s_fShadowMaxCameraDistance_PSP * s_fShadowMaxCameraDistance_PSP )
		return TFALSE;
	a_rRenderer.DrawWorldTextureQuad( m_Texture, a_rShadow.aCorners, a_rCamera );
	return TTRUE;
}
