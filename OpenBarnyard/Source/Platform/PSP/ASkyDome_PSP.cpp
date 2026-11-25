#include "pch.h"
#include "ASkyDome_PSP.h"

#include <File/TFile.h>
#include <Math/TMathInline.h>
#include <Platform/PSP/TModelFactory_PSP.h>
#include <Platform/PSP/TTextureFactory_PSP.h>

TOSHI_NAMESPACE_USING

static constexpr TCHAR s_szSSeqSkyRoot_PSP[] = "local:Data/ScriptedSequence/SS01";

struct ASkyDomeConfig_PSP
{
	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TFLOAT fRotationSpeed;
};

TBOOL ASkyDome_PSP::Load( const TCHAR* a_szRoot )
{
	Unload();
	const TCHAR* szRoot = a_szRoot != TNULL ? a_szRoot : s_szSSeqSkyRoot_PSP;
	TStringManager::String8Format( m_szFailedPath, TARRAYSIZE( m_szFailedPath ), "%s/sky.psky", szRoot );
	TFile* pFile = TFile::Create( m_szFailedPath, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return a_szRoot == TNULL;

	static_assert( sizeof( ASkyDomeConfig_PSP ) == 12 );
	ASkyDomeConfig_PSP oConfig = {};
	const TBOOL bRead = pFile->GetSize() == sizeof( oConfig ) && pFile->Read( &oConfig, sizeof( oConfig ) ) == sizeof( oConfig );
	pFile->Destroy();
	if ( !bRead || oConfig.uiMagic != 0x594b5350 || oConfig.uiVersion != 1 ||
	     !( oConfig.fRotationSpeed >= 0.0f && oConfig.fRotationSpeed <= 10.0f ) )
		return TFALSE;

	const TModelFactory_PSP oModelFactory;
	const TTextureFactory_PSP oTextureFactory;
	TStringManager::String8Format( m_szFailedPath, TARRAYSIZE( m_szFailedPath ), "%s/sky-base.pmes", szRoot );
	if ( !oModelFactory.CreateModelFromFile( m_Base, m_szFailedPath ) || m_Base.GetFormat() != TMODELFORMAT_PSP_COLOUR_XYZ )
		return TFALSE;
	TStringManager::String8Format( m_szFailedPath, TARRAYSIZE( m_szFailedPath ), "%s/sky-clouds.pmes", szRoot );
	if ( !oModelFactory.CreateModelFromFile( m_Clouds, m_szFailedPath ) || m_Clouds.GetFormat() != TMODELFORMAT_PSP_TEXTURE_COLOUR_XYZ )
		return TFALSE;
	TStringManager::String8Format( m_szFailedPath, TARRAYSIZE( m_szFailedPath ), "%s/sky-clouds.ptex", szRoot );
	if ( !oTextureFactory.CreateTextureFromFile( m_CloudTexture, m_szFailedPath ) )
		return TFALSE;

	m_fRotation = 1.0f; // ASkyDome initial cloud rotation.
	m_fRotationSpeed = oConfig.fRotationSpeed;
	m_szFailedPath[ 0 ] = '\0';
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
