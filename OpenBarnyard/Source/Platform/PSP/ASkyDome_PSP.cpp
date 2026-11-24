#include "pch.h"
#include "ASkyDome_PSP.h"

#include <File/TFile.h>
#include <Math/TMathInline.h>
#include <Platform/PSP/TModelFactory_PSP.h>
#include <Platform/PSP/TTextureFactory_PSP.h>

TOSHI_NAMESPACE_USING

static constexpr TCHAR s_szSkyConfig_PSP[] = "local:Data/ScriptedSequence/SS01/sky.psky";
static constexpr TCHAR s_szSkyBase_PSP[] = "local:Data/ScriptedSequence/SS01/sky-base.pmes";
static constexpr TCHAR s_szSkyClouds_PSP[] = "local:Data/ScriptedSequence/SS01/sky-clouds.pmes";
static constexpr TCHAR s_szSkyTexture_PSP[] = "local:Data/ScriptedSequence/SS01/sky-clouds.ptex";

struct ASkyDomeConfig_PSP
{
	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TFLOAT fRotationSpeed;
};

TBOOL ASkyDome_PSP::Load()
{
	Unload();
	m_szFailedPath = TNULL;
	TFile* pFile = TFile::Create( s_szSkyConfig_PSP, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TTRUE;

	static_assert( sizeof( ASkyDomeConfig_PSP ) == 12 );
	ASkyDomeConfig_PSP oConfig = {};
	const TBOOL bRead = pFile->GetSize() == sizeof( oConfig ) && pFile->Read( &oConfig, sizeof( oConfig ) ) == sizeof( oConfig );
	pFile->Destroy();
	m_szFailedPath = s_szSkyConfig_PSP;
	if ( !bRead || oConfig.uiMagic != 0x594b5350 || oConfig.uiVersion != 1 ||
	     !( oConfig.fRotationSpeed >= 0.0f && oConfig.fRotationSpeed <= 10.0f ) )
		return TFALSE;

	const TModelFactory_PSP oModelFactory;
	const TTextureFactory_PSP oTextureFactory;
	m_szFailedPath = s_szSkyBase_PSP;
	if ( !oModelFactory.CreateModelFromFile( m_Base, s_szSkyBase_PSP ) || m_Base.GetFormat() != TMODELFORMAT_PSP_COLOUR_XYZ )
		return TFALSE;
	m_szFailedPath = s_szSkyClouds_PSP;
	if ( !oModelFactory.CreateModelFromFile( m_Clouds, s_szSkyClouds_PSP ) || m_Clouds.GetFormat() != TMODELFORMAT_PSP_TEXTURE_COLOUR_XYZ )
		return TFALSE;
	m_szFailedPath = s_szSkyTexture_PSP;
	if ( !oTextureFactory.CreateTextureFromFile( m_CloudTexture, s_szSkyTexture_PSP ) )
		return TFALSE;

	m_fRotation = 1.0f; // ASkyDome initial cloud rotation.
	m_fRotationSpeed = oConfig.fRotationSpeed;
	m_szFailedPath = TNULL;
	return TTRUE;
}

void ASkyDome_PSP::Unload()
{
	m_Base.Unload();
	m_Clouds.Unload();
	m_CloudTexture.Unload();
}

TUINT ASkyDome_PSP::Render( TRenderInterface_PSP& a_rRenderer, const TRenderCamera_PSP& a_rCamera, TFLOAT a_fDeltaTime )
{
	if ( !m_Base.IsLoaded() )
		return 0;

	TRenderCamera_PSP oSkyCamera = a_rCamera;
	for ( TFLOAT& fPosition : oSkyCamera.aPosition )
		fPosition = 0.0f;

	static const TRenderModelTransform_PSP s_ModelTransform = { 1.0f, { 0.0f, 0.0f, 0.0f } };
	static const TRenderMaterial_PSP s_BaseMaterial = {
		TFALSE, TRENDERTEXTUREFUNCTION_PSP_MODULATE, TRENDERCULLMODE_PSP_NONE, TRENDERALPHAMODE_PSP_BLEND,
		TRENDERTEXTUREADDRESS_PSP_CLAMP, TRENDERTEXTUREADDRESS_PSP_CLAMP,
	};
	static const TRenderMaterial_PSP s_CloudMaterial = {
		TTRUE, TRENDERTEXTUREFUNCTION_PSP_MODULATE, TRENDERCULLMODE_PSP_NONE, TRENDERALPHAMODE_PSP_BLEND,
		TRENDERTEXTUREADDRESS_PSP_WRAP, TRENDERTEXTUREADDRESS_PSP_CLAMP,
	};

	m_fRotation -= a_fDeltaTime * m_fRotationSpeed;
	if ( m_fRotation < -TMath::TWO_PI )
		m_fRotation += TMath::TWO_PI;
	TRenderSceneTransform_PSP oTransform = { { 0.0f, 0.0f, 0.0f }, { TMath::HALF_PI, 0.0f, 0.0f }, 1.0f };
	a_rRenderer.DrawModel( m_Base, oTransform, s_BaseMaterial, TNULL, s_ModelTransform, &oSkyCamera );
	oTransform.aRotation[ 2 ] = m_fRotation;
	a_rRenderer.DrawModel( m_Clouds, oTransform, s_CloudMaterial, &m_CloudTexture, s_ModelTransform, &oSkyCamera );
	return 2;
}
