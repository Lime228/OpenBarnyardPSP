#include "pch.h"

#include "AFrontEndCamera_PSP.h"

#include <File/TFile.h>
#include <Math/TMathInline.h>
#include <Toshi/TStringManager.h>

TOSHI_NAMESPACE_USING

struct AFrontEndCameraHeader_PSP
{
	TUINT32 uiMagic;
	TUINT32 uiVersion;
	TUINT32 uiFrameCount;
	TUINT32 uiSampleRate;
	TUINT32 uiFrameSize;
	TUINT32 uiFlags;
};

static constexpr TUINT32 s_uiFrontEndCameraMagic_PSP = 0x4d414350;
static constexpr TUINT32 s_uiFrontEndCameraVersion_PSP = 1;
static constexpr TUINT32 s_uiFrontEndCameraLoopFlag_PSP = 1;
static constexpr TUINT32 s_uiFrontEndCameraMaxSampleRate_PSP = 60;

static_assert( sizeof( AFrontEndCameraHeader_PSP ) == 24, "Unexpected PSP camera header layout" );
static_assert( sizeof( AFrontEndCameraFrame_PSP ) == 36, "Unexpected PSP camera frame layout" );

static TBOOL AFrontEndCameraNormalise_PSP( TFLOAT* a_pVector )
{
	const TFLOAT fLengthSquared =
		a_pVector[ 0 ] * a_pVector[ 0 ] +
		a_pVector[ 1 ] * a_pVector[ 1 ] +
		a_pVector[ 2 ] * a_pVector[ 2 ];
	if ( !TMath::IsFinite( fLengthSquared ) || fLengthSquared <= TMath::TFLOAT_EPSILON )
		return TFALSE;

	const TFLOAT fInverseLength = TMath::OneOverSqrt( fLengthSquared );
	for ( TUINT axis = 0; axis < 3; axis++ )
		a_pVector[ axis ] *= fInverseLength;
	return TTRUE;
}

AFrontEndCamera_PSP::AFrontEndCamera_PSP()
    : m_RenderCamera_PSP{
		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, -1.0f },
		{ 0.0f, 1.0f, 0.0f },
		60.0f,
	}
    , m_uiFrameCount_PSP( 0 )
    , m_uiSampleRate_PSP( 0 )
    , m_uiFlags_PSP( 0 )
    , m_fTime_PSP( 0.0f )
    , m_szFailedPath_PSP{ '\0' }
{
}

TBOOL AFrontEndCamera_PSP::Load( const TCHAR* a_szPath, TFLOAT a_fFOV, TFLOAT a_fNear )
{
	Unload();
	TStringManager::String8Copy(
		m_szFailedPath_PSP,
		a_szPath,
		TARRAYSIZE( m_szFailedPath_PSP )
	);

	TFile* pFile = TFile::Create( m_szFailedPath_PSP, TFILEMODE_READ | TFILEMODE_NOBUFFER );
	if ( pFile == TNULL )
		return TFALSE;

	AFrontEndCameraHeader_PSP oHeader = {};
	TBOOL bLoaded = pFile->Read( &oHeader, sizeof( oHeader ) ) == sizeof( oHeader );
	bLoaded = bLoaded &&
		oHeader.uiMagic == s_uiFrontEndCameraMagic_PSP &&
		oHeader.uiVersion == s_uiFrontEndCameraVersion_PSP &&
		oHeader.uiFrameCount >= 1 &&
		oHeader.uiFrameCount <= MAX_FRAMES_PSP &&
		oHeader.uiSampleRate >= 1 &&
		oHeader.uiSampleRate <= s_uiFrontEndCameraMaxSampleRate_PSP &&
		oHeader.uiFrameSize == sizeof( AFrontEndCameraFrame_PSP ) &&
		( oHeader.uiFlags & ~s_uiFrontEndCameraLoopFlag_PSP ) == 0;
	if ( bLoaded )
	{
		const TSIZE uiExpectedSize = sizeof( oHeader ) +
			TSIZE( oHeader.uiFrameCount ) * sizeof( AFrontEndCameraFrame_PSP );
		bLoaded = pFile->GetSize() == uiExpectedSize;
	}
	if ( bLoaded )
	{
		bLoaded = pFile->Read(
			m_aFrames_PSP,
			oHeader.uiFrameCount * sizeof( AFrontEndCameraFrame_PSP )
		) == oHeader.uiFrameCount * sizeof( AFrontEndCameraFrame_PSP );
	}
	pFile->Destroy();

	if ( bLoaded )
	{
		for ( TUINT frame = 0; frame < oHeader.uiFrameCount && bLoaded; frame++ )
		{
			AFrontEndCameraFrame_PSP& rFrame = m_aFrames_PSP[ frame ];
			for ( TUINT axis = 0; axis < 3; axis++ )
				bLoaded = bLoaded && TMath::IsFinite( rFrame.aPosition[ axis ] );
			bLoaded = bLoaded &&
				AFrontEndCameraNormalise_PSP( rFrame.aDirection ) &&
				AFrontEndCameraNormalise_PSP( rFrame.aUp );
			const TFLOAT fDot =
				rFrame.aDirection[ 0 ] * rFrame.aUp[ 0 ] +
				rFrame.aDirection[ 1 ] * rFrame.aUp[ 1 ] +
				rFrame.aDirection[ 2 ] * rFrame.aUp[ 2 ];
			bLoaded = bLoaded && TMath::IsFinite( fDot ) && TMath::Abs( fDot ) < 0.999f;
		}
	}

	if ( !bLoaded )
	{
		TStringManager::String8Copy(
			m_szFailedPath_PSP,
			"frontend-camera-data",
			TARRAYSIZE( m_szFailedPath_PSP )
		);
		Unload();
		return TFALSE;
	}

	m_uiFrameCount_PSP = oHeader.uiFrameCount;
	m_uiSampleRate_PSP = oHeader.uiSampleRate;
	m_uiFlags_PSP = oHeader.uiFlags;
	m_fTime_PSP = 0.0f;
	m_RenderCamera_PSP.fFOV = a_fFOV;
	m_RenderCamera_PSP.fNear = a_fNear;
	ApplyFrame_PSP( 0, m_uiFrameCount_PSP > 1 ? 1 : 0, 0.0f );
	m_szFailedPath_PSP[ 0 ] = '\0';
	return TTRUE;
}

void AFrontEndCamera_PSP::Unload()
{
	m_uiFrameCount_PSP = 0;
	m_uiSampleRate_PSP = 0;
	m_uiFlags_PSP = 0;
	m_fTime_PSP = 0.0f;
	m_RenderCamera_PSP = {
		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, -1.0f },
		{ 0.0f, 1.0f, 0.0f },
		60.0f,
	};
}

void AFrontEndCamera_PSP::Update( TFLOAT a_fDeltaTime )
{
	if ( !IsLoaded() || !TMath::IsFinite( a_fDeltaTime ) || a_fDeltaTime <= 0.0f )
		return;

	m_fTime_PSP += a_fDeltaTime;
	if ( m_uiFlags_PSP & s_uiFrontEndCameraLoopFlag_PSP )
	{
		const TFLOAT fDuration = TFLOAT( m_uiFrameCount_PSP ) / TFLOAT( m_uiSampleRate_PSP );
		m_fTime_PSP -= TMath::FloorToInt( m_fTime_PSP / fDuration ) * fDuration;
	}
	TFLOAT fFrame = m_fTime_PSP * TFLOAT( m_uiSampleRate_PSP );
	if ( !( m_uiFlags_PSP & s_uiFrontEndCameraLoopFlag_PSP ) &&
	     fFrame >= TFLOAT( m_uiFrameCount_PSP - 1 ) )
	{
		fFrame = TFLOAT( m_uiFrameCount_PSP - 1 );
	}

	const TUINT uiFrame0 = TUINT( TMath::FloorToInt( fFrame ) );
	const TUINT uiFrame1 = uiFrame0 + 1 < m_uiFrameCount_PSP ? uiFrame0 + 1 :
		( m_uiFlags_PSP & s_uiFrontEndCameraLoopFlag_PSP ? 0 : uiFrame0 );
	ApplyFrame_PSP( uiFrame0, uiFrame1, fFrame - TFLOAT( uiFrame0 ) );
}

TBOOL AFrontEndCamera_PSP::ApplyFrame_PSP( TUINT a_uiFrame0, TUINT a_uiFrame1, TFLOAT a_fProgress )
{
	const AFrontEndCameraFrame_PSP& rFrame0 = m_aFrames_PSP[ a_uiFrame0 ];
	const AFrontEndCameraFrame_PSP& rFrame1 = m_aFrames_PSP[ a_uiFrame1 ];
	for ( TUINT axis = 0; axis < 3; axis++ )
	{
		m_RenderCamera_PSP.aPosition[ axis ] = TMath::LERP( rFrame0.aPosition[ axis ], rFrame1.aPosition[ axis ], a_fProgress );
		m_RenderCamera_PSP.aDirection[ axis ] = TMath::LERP( rFrame0.aDirection[ axis ], rFrame1.aDirection[ axis ], a_fProgress );
		m_RenderCamera_PSP.aUp[ axis ] = TMath::LERP( rFrame0.aUp[ axis ], rFrame1.aUp[ axis ], a_fProgress );
	}
	if ( !AFrontEndCameraNormalise_PSP( m_RenderCamera_PSP.aDirection ) )
		return TFALSE;

	const TFLOAT fDot =
		m_RenderCamera_PSP.aDirection[ 0 ] * m_RenderCamera_PSP.aUp[ 0 ] +
		m_RenderCamera_PSP.aDirection[ 1 ] * m_RenderCamera_PSP.aUp[ 1 ] +
		m_RenderCamera_PSP.aDirection[ 2 ] * m_RenderCamera_PSP.aUp[ 2 ];
	for ( TUINT axis = 0; axis < 3; axis++ )
		m_RenderCamera_PSP.aUp[ axis ] -= m_RenderCamera_PSP.aDirection[ axis ] * fDot;
	return AFrontEndCameraNormalise_PSP( m_RenderCamera_PSP.aUp );
}
