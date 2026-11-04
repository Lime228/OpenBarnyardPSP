#include <pspkernel.h>

#include <Toshi/Typedefs.h>
#include <Toshi/TDebug.h>
#include <Toshi/THPTimer.h>
#include <Toshi/TScheduler.h>
#include <Math/TMathInline.h>
#include <Math/TVector4.h>
#include <File/TFile.h>
#include <Platform/PSP/Input/TInputInterface_PSP.h>
#include <Platform/PSP/TSkeletonFactory_PSP.h>
#include <Platform/PSP/TSkeletonInstance_PSP.h>
#include <Platform/PSP/TAnimationClipFactory_PSP.h>
#include <Render/TBackend.h>

#include "Frontend/AApplication_PSP.h"
#include "GUI/AGUIAudioContext_PSP.h"
#include "Frontend/AFrontendLocale_PSP.h"
#include "Diagnostics/APortReport_PSP.h"
#include "Locale/ALocaleManager_PSP.h"
#include "Sound/ASoundManager_PSP.h"
#include "Sound/ASoundMixer_PSP.h"
#include "Sound/ASoundCommandQueue_PSP.h"
#include "Sound/ASoundDelayedEventQueue_PSP.h"
#include "Sound/AWaveBank_PSP.h"
#include "Sound/AWaveBankLoader_PSP.h"
#include "Sound/AStreamWave_PSP.h"
#include "ToshiSmoke.h"

PSP_MODULE_INFO( "OpenBarnyard PSP Probe", PSP_MODULE_USER, 0, 1 );
PSP_MAIN_THREAD_ATTR( PSP_THREAD_ATTR_USER );
PSP_HEAP_SIZE_KB( -1024 );

static volatile TBOOL g_Running = TTRUE;

struct ProbeTextureData_PSP
{
	Toshi::T2TextureDataHeader_PSP oHeader;
	TUINT32                        aPixels[ 16 * 16 ];
};

alignas( 16 ) static ProbeTextureData_PSP g_ProbeTextureData = {
	{
	    Toshi::T2TextureDataHeader_PSP::MAGIC,
	    Toshi::T2TextureDataHeader_PSP::VERSION,
	    16,
	    16,
	    16,
	    Toshi::T2TEXTUREFORMAT_PSP_RGBA8888,
	},
	{}
};
static Toshi::T2Texture g_ProbeTexture;
static Toshi::T2Texture g_FontTexture_PSP;
static Toshi::T2GUIFont_PSP g_Font_PSP;
static ALocaleManager_PSP g_LocaleManager_PSP;
static ASoundManager_PSP g_SoundManager_PSP;
static ASoundMixer_PSP g_SoundMixer_PSP;
static ASoundCommandQueue_PSP g_SoundCommandQueue_PSP;
static ASoundDelayedEventQueue_PSP g_DelayedEventQueue_PSP;
static AWaveBank_PSP g_WaveBank_PSP;
static AWave_PSP g_BootstrapWaves_PSP[ 4 ];
static AWaveBankLoader_PSP g_WaveBankLoader_PSP;
static TUINT g_uiLoadedWaves_PSP = 0;
static AStreamWave_PSP g_BootstrapStream_PSP;
static TUINT g_uiLoadedStreams_PSP = 0;
static AGUIAudioContext_PSP g_GUIAudioContext_PSP;
static Toshi::TTextureFactory_PSP g_TextureFactory_PSP;
static Toshi::T2GUIFontFactory_PSP g_FontFactory_PSP;
static Toshi::TModelFactory_PSP g_ModelFactory_PSP;
static Toshi::TSkeletonFactory_PSP g_SkeletonFactory_PSP;
static Toshi::TAnimationClipFactory_PSP g_AnimationFactory_PSP;
static Toshi::TRenderResourceCache_PSP g_ResourceCache_PSP;
static Toshi::TRenderInterface_PSP g_RenderInterface;
static Toshi::TVertexPoolResource_PSP g_ProbeVertices_PSP;
static Toshi::TIndexPoolResource_PSP  g_ProbeIndices_PSP;
static Toshi::TModel_PSP                g_ProbeModel_PSP;
static Toshi::TModel_PSP                g_ProbeSkinnedModel_PSP;
static Toshi::TSkeleton_PSP             g_ProbeSkeleton_PSP;
static Toshi::TAnimationClip_PSP        g_ProbeAnimation_PSP;
static Toshi::TSkeletonInstance_PSP     g_ProbeSkeletonInstance_PSP;
static Toshi::TSkinningBuffer_PSP       g_ProbeSkinningBuffer_PSP;
static Toshi::TModel_PSP                g_PreviewModel_PSP;
static Toshi::TSkeleton_PSP             g_PreviewSkeleton_PSP;
static Toshi::TAnimationClip_PSP        g_PreviewAnimation_PSP;
static Toshi::TSkeletonInstance_PSP     g_PreviewSkeletonInstance_PSP;
static Toshi::TSkinningBuffer_PSP       g_PreviewSkinningBuffer_PSP;
static Toshi::T2Texture                 g_PreviewTexture_PSP;
static const Toshi::TRenderModelTransform_PSP g_IdentityModelTransform_PSP = {
	1.0f,
	{ 0.0f, 0.0f, 0.0f },
};
static Toshi::TRenderModelTransform_PSP g_PreviewTransform_PSP = g_IdentityModelTransform_PSP;
static TBOOL                            g_bExternalTexture_PSP = TFALSE;
static TBOOL                            g_bPreviewTexture_PSP = TFALSE;
static const Toshi::TRenderMaterial_PSP g_ProbeMaterial_PSP = {
	TTRUE,
	Toshi::TRENDERTEXTUREFUNCTION_PSP_MODULATE,
	Toshi::TRENDERCULLMODE_PSP_BACK,
};

static_assert( sizeof( void* ) == 4, "The PSP port requires a 32-bit ABI" );

static TBOOL LoadProbeTextureFile_PSP()
{
	return g_TextureFactory_PSP.CreateTextureFromFile( g_ProbeTexture, "local:probe.ptex" );
}

static TBOOL LoadFontTextureFile_PSP()
{
	return g_TextureFactory_PSP.CreateTextureFromFile( g_FontTexture_PSP, "local:bootstrap_font.ptex" );
}

static TBOOL LoadFontFile_PSP()
{
	return g_FontFactory_PSP.CreateFontFromFile(
	    g_Font_PSP,
	    &g_RenderInterface,
	    &g_FontTexture_PSP,
	    "local:bootstrap_font.pfnt"
	);
}

static TBOOL LoadLocaleFile_PSP()
{
	return g_LocaleManager_PSP.SetLanguage( ALocaleManager_PSP::GetDefaultLanguage() ) &&
	       g_LocaleManager_PSP.GetNumStrings() == AFrontendLocaleString_PSP_Count;
}

static TBOOL LoadProbeModelFile_PSP()
{
	return g_ModelFactory_PSP.CreateModelFromFile( g_ProbeModel_PSP, "local:probe.pmes" );
}

static TBOOL LoadProbeSkinnedModelFile_PSP()
{
	return g_ModelFactory_PSP.CreateModelFromFile( g_ProbeSkinnedModel_PSP, "local:probe_skinned.pmes" );
}

static TBOOL TestModelBounds_PSP( const Toshi::TModel_PSP& a_rModel )
{
	const Toshi::TModelBounds_PSP& rBounds = a_rModel.GetBounds();
	if ( rBounds.aMin[ 0 ] != -1.0f || rBounds.aMin[ 1 ] != -1.0f || rBounds.aMin[ 2 ] != -1.0f ||
	     rBounds.aMax[ 0 ] != 1.0f || rBounds.aMax[ 1 ] != 1.0f || rBounds.aMax[ 2 ] != 1.0f )
	{
		return TFALSE;
	}

	struct InvalidModelData_PSP
	{
		Toshi::TModelDataHeader_PSP oHeader;
		Toshi::TModelVertex_PSP     oVertex;
		Toshi::TIndexType           aIndices[ 3 ];
	};
	const InvalidModelData_PSP oInvalidData = {
		{
		    Toshi::TModelDataHeader_PSP::MAGIC,
		    Toshi::TModelDataHeader_PSP::VERSION,
		    1,
		    3,
		    sizeof( Toshi::TModelVertex_PSP ),
		    Toshi::TMODELFORMAT_PSP_COLOUR_XYZ,
		},
		{ 0xffffffff, Toshi::TMath::FLOAT_NAN, 0.0f, 0.0f },
		{ 0, 0, 0 },
	};
	Toshi::TModel_PSP oInvalidModel;
	return !g_ModelFactory_PSP.CreateModelFromMemory(
	    oInvalidModel,
	    &oInvalidData,
	    sizeof( Toshi::TModelDataHeader_PSP ) + sizeof( Toshi::TModelVertex_PSP ) +
	        sizeof( oInvalidData.aIndices )
	);
}

static TBOOL CalculatePreviewTransform_PSP(
    const Toshi::TModel_PSP& a_rModel,
    Toshi::TRenderModelTransform_PSP& a_rTransform
)
{
	const Toshi::TModelBounds_PSP& rBounds = a_rModel.GetBounds();
	const TFLOAT fWidth = rBounds.aMax[ 0 ] - rBounds.aMin[ 0 ];
	const TFLOAT fHeight = rBounds.aMax[ 1 ] - rBounds.aMin[ 1 ];
	const TFLOAT fDepth = rBounds.aMax[ 2 ] - rBounds.aMin[ 2 ];
	const TFLOAT fLargestSize = Toshi::TMath::Max( Toshi::TMath::Max( fWidth, fHeight ), fDepth );
	if ( !Toshi::TMath::IsFinite( fLargestSize ) || fLargestSize <= Toshi::TMath::TFLOAT_EPSILON )
		return TFALSE;

	a_rTransform.fScale = 2.0f / fLargestSize;
	for ( TUINT32 i = 0; i < 3; i++ )
		a_rTransform.aCenter[ i ] = rBounds.aMin[ i ] +
		                             ( rBounds.aMax[ i ] - rBounds.aMin[ i ] ) * 0.5f;
	return Toshi::TMath::IsFinite( a_rTransform.fScale );
}

static TBOOL TestPreviewTransform_PSP( const Toshi::TModel_PSP& a_rModel )
{
	Toshi::TRenderModelTransform_PSP oTransform = {};
	return CalculatePreviewTransform_PSP( a_rModel, oTransform ) &&
	       oTransform.fScale == 1.0f &&
	       oTransform.aCenter[ 0 ] == 0.0f &&
	       oTransform.aCenter[ 1 ] == 0.0f &&
	       oTransform.aCenter[ 2 ] == 0.0f;
}

static void UnloadPreviewModel_PSP()
{
	g_PreviewSkinningBuffer_PSP.Destroy();
	g_PreviewSkeletonInstance_PSP.Destroy();
	g_PreviewAnimation_PSP.Unload();
	g_PreviewSkeleton_PSP.Unload();
	g_PreviewModel_PSP.Unload();
	g_PreviewTexture_PSP.Unload();
	g_PreviewTransform_PSP = g_IdentityModelTransform_PSP;
	g_bPreviewTexture_PSP = TFALSE;
}

static TBOOL LoadPreviewModel_PSP()
{
	const TBOOL bReady =
	    g_ModelFactory_PSP.CreateModelFromFile( g_PreviewModel_PSP, "local:barnyard_preview.pmes" ) &&
	    g_SkeletonFactory_PSP.CreateSkeletonFromFile( g_PreviewSkeleton_PSP, "local:barnyard_preview.pskl" ) &&
	    g_AnimationFactory_PSP.CreateAnimationFromFile(
	        g_PreviewAnimation_PSP,
	        &g_PreviewSkeleton_PSP,
	        "local:barnyard_preview.panm"
	    ) &&
	    g_PreviewSkeletonInstance_PSP.Create( &g_PreviewSkeleton_PSP ) &&
	    g_PreviewSkinningBuffer_PSP.Create( &g_PreviewModel_PSP, &g_PreviewSkeleton_PSP ) &&
	    g_PreviewSkeletonInstance_PSP.SetAnimationPose( g_PreviewAnimation_PSP, 0 ) &&
	    g_PreviewSkinningBuffer_PSP.Skin( g_PreviewSkeletonInstance_PSP ) &&
	    CalculatePreviewTransform_PSP( g_PreviewModel_PSP, g_PreviewTransform_PSP );
	if ( !bReady )
		UnloadPreviewModel_PSP();
	else
		g_bPreviewTexture_PSP = g_TextureFactory_PSP.CreateTextureFromFile(
		    g_PreviewTexture_PSP,
		    "local:barnyard_preview.ptex"
		);
	return bReady;
}

static TBOOL TestProbeSkeletonFile_PSP()
{
	if ( !g_SkeletonFactory_PSP.CreateSkeletonFromFile( g_ProbeSkeleton_PSP, "local:probe.pskl" ) ||
	     g_ProbeSkeleton_PSP.GetBoneCount() != 3 ||
	     g_ProbeSkeleton_PSP.GetBoneID( "ROOT" ) != 0 ||
	     g_ProbeSkeleton_PSP.GetBoneID( "center" ) != 1 ||
	     g_ProbeSkeleton_PSP.GetBoneID( "marker" ) != 2 ||
	     g_ProbeSkeleton_PSP.GetBoneID( "missing" ) != -1 )
	{
		return TFALSE;
	}

	const Toshi::TSkeletonBone_PSP* pRoot = g_ProbeSkeleton_PSP.GetBone( 0 );
	const Toshi::TSkeletonBone_PSP* pCenter = g_ProbeSkeleton_PSP.GetBone( 1 );
	const Toshi::TSkeletonBone_PSP* pMarker = g_ProbeSkeleton_PSP.GetBone( 2 );
	if ( pRoot == TNULL || pCenter == TNULL || pMarker == TNULL ||
	     g_ProbeSkeleton_PSP.GetBone( 3 ) != TNULL ||
	     pRoot->iParentBone != -1 ||
	     pCenter->iParentBone != 0 ||
	     pMarker->iParentBone != 1 ||
	     pCenter->aTranslation[ 1 ] != 0.5f ||
	     pMarker->aRotation[ 3 ] != 1.0f )
	{
		return TFALSE;
	}

	const void* pSourceData = TNULL;
	TUINT uiSourceSize = 1;
	g_ProbeSkeleton_PSP.GetData( pSourceData, uiSourceSize );
	if ( pSourceData != TNULL || uiSourceSize != 0 )
		return TFALSE;

	struct InvalidSkeletonData_PSP
	{
		Toshi::TSkeletonDataHeader_PSP oHeader;
		Toshi::TSkeletonBone_PSP       aBones[ 3 ];
	};
	InvalidSkeletonData_PSP oInvalidData = {};
	oInvalidData.oHeader = {
		Toshi::TSkeletonDataHeader_PSP::MAGIC,
		Toshi::TSkeletonDataHeader_PSP::VERSION,
		3,
		sizeof( Toshi::TSkeletonBone_PSP ),
	};
	oInvalidData.aBones[ 0 ] = *pRoot;
	oInvalidData.aBones[ 1 ] = *pCenter;
	oInvalidData.aBones[ 2 ] = *pMarker;
	oInvalidData.aBones[ 2 ].iParentBone = 2;
	Toshi::TSkeleton_PSP oInvalidSkeleton;
	return !g_SkeletonFactory_PSP.CreateSkeletonFromMemory(
	    oInvalidSkeleton,
	    &oInvalidData,
	    sizeof( oInvalidData )
	);
}

static TBOOL TestProbeSkeletonPose_PSP()
{
	Toshi::TSkeletonInstance_PSP oInstance;
	if ( !oInstance.Create( &g_ProbeSkeleton_PSP ) ||
	     oInstance.GetSkeleton() != &g_ProbeSkeleton_PSP )
	{
		return TFALSE;
	}

	const Toshi::TSkeletonTransform_PSP* pRoot = oInstance.GetBoneTransform( "root" );
	const Toshi::TSkeletonTransform_PSP* pCenter = oInstance.GetBoneTransform( 1 );
	const Toshi::TSkeletonTransform_PSP* pMarker = oInstance.GetBoneTransform( "MARKER" );
	if ( pRoot == TNULL || pCenter == TNULL || pMarker == TNULL ||
	     oInstance.GetBoneTransform( 3 ) != TNULL ||
	     oInstance.GetBoneTransform( "missing" ) != TNULL ||
	     pRoot->aTranslation[ 1 ] != 0.0f ||
	     pCenter->aTranslation[ 1 ] != 0.5f ||
	     pMarker->aTranslation[ 1 ] != 1.0f ||
	     pMarker->aRotation[ 3 ] != 1.0f )
	{
		return TFALSE;
	}

	oInstance.Destroy();
	return oInstance.GetBoneTransform( TUINT32( 0 ) ) == TNULL && !oInstance.SetBindPose();
}

static TBOOL IsAnimationValueNear_PSP( TFLOAT a_fValue, TFLOAT a_fExpected )
{
	const TFLOAT fDifference = a_fValue - a_fExpected;
	return fDifference >= -0.001f && fDifference <= 0.001f;
}

static TBOOL TestProbeAnimation_PSP()
{
	if ( !g_AnimationFactory_PSP.CreateAnimationFromFile(
	         g_ProbeAnimation_PSP,
	         &g_ProbeSkeleton_PSP,
	         "local:probe.panm"
	     ) ||
	     g_ProbeAnimation_PSP.GetSkeleton() != &g_ProbeSkeleton_PSP ||
	     g_ProbeAnimation_PSP.GetBoneCount() != 3 ||
	     g_ProbeAnimation_PSP.GetFrameCount() != 2 ||
	     g_ProbeAnimation_PSP.GetSampleRate() != 2 ||
	     g_ProbeAnimation_PSP.IsLooping() )
	{
		return TFALSE;
	}

	Toshi::TSkeletonInstance_PSP oInstance;
	if ( !oInstance.Create( &g_ProbeSkeleton_PSP ) )
		return TFALSE;

	static const TUINT aTimes[] = { 0, 250, 500, 1000 };
	static const TFLOAT aExpectedMarkerY[] = { 1.0f, 1.25f, 1.5f, 1.5f };
	for ( TUINT32 i = 0; i < TARRAYSIZE( aTimes ); i++ )
	{
		if ( !oInstance.SetAnimationPose( g_ProbeAnimation_PSP, aTimes[ i ] ) )
			return TFALSE;
		const Toshi::TSkeletonTransform_PSP* pMarker = oInstance.GetBoneTransform( "marker" );
		if ( pMarker == TNULL ||
		     !IsAnimationValueNear_PSP( pMarker->aTranslation[ 1 ], aExpectedMarkerY[ i ] ) ||
		     !IsAnimationValueNear_PSP( pMarker->aRotation[ 3 ], 1.0f ) )
		{
			return TFALSE;
		}
	}

	return TTRUE;
}

struct LoopAnimationData_PSP
{
	Toshi::TAnimationClipDataHeader_PSP oHeader;
	Toshi::TAnimationSample_PSP        aSamples[ 2 * 3 ];
};

static void SetLoopAnimationSample_PSP( Toshi::TAnimationSample_PSP& a_rSample, TINT16 a_iTranslationY )
{
	a_rSample.aRotation[ 3 ]    = 32767;
	a_rSample.aTranslation[ 1 ] = a_iTranslationY;
}

static TBOOL TestLoopingAnimation_PSP()
{
	LoopAnimationData_PSP oData = {};
	oData.oHeader = {
		Toshi::TAnimationClipDataHeader_PSP::MAGIC,
		Toshi::TAnimationClipDataHeader_PSP::VERSION,
		3,
		2,
		2,
		sizeof( Toshi::TAnimationSample_PSP ),
		1.0f,
		Toshi::TANIMATIONCLIPFLAGS_PSP_LOOP,
	};
	for ( TUINT32 frame = 0; frame < 2; frame++ )
	{
		SetLoopAnimationSample_PSP( oData.aSamples[ frame * 3 ], 0 );
		SetLoopAnimationSample_PSP( oData.aSamples[ frame * 3 + 1 ], 16384 );
	}
	SetLoopAnimationSample_PSP( oData.aSamples[ 2 ], 16384 );
	SetLoopAnimationSample_PSP( oData.aSamples[ 5 ], 32767 );

	Toshi::TAnimationClip_PSP oClip;
	Toshi::TSkeletonInstance_PSP oInstance;
	TBOOL bReady = g_AnimationFactory_PSP.CreateAnimationFromMemory(
	    oClip,
	    &g_ProbeSkeleton_PSP,
	    &oData,
	    sizeof( oData )
	) && oInstance.Create( &g_ProbeSkeleton_PSP );
	static const TUINT aTimes[] = { 0, 250, 500, 750, 1000 };
	static const TFLOAT aExpectedMarkerY[] = { 1.0f, 1.25f, 1.5f, 1.25f, 1.0f };
	for ( TUINT32 i = 0; bReady && i < TARRAYSIZE( aTimes ); i++ )
	{
		bReady = oInstance.SetAnimationPose( oClip, aTimes[ i ] );
		const Toshi::TSkeletonTransform_PSP* pMarker = oInstance.GetBoneTransform( "marker" );
		bReady = bReady && pMarker != TNULL &&
		         IsAnimationValueNear_PSP( pMarker->aTranslation[ 1 ], aExpectedMarkerY[ i ] );
	}

	oInstance.Destroy();
	oClip.Unload();
	return bReady;
}

static TBOOL TestProbeSkinning_PSP()
{
	if ( !LoadProbeSkinnedModelFile_PSP() ||
	     !TestModelBounds_PSP( g_ProbeSkinnedModel_PSP ) ||
	     !g_ProbeSkeletonInstance_PSP.Create( &g_ProbeSkeleton_PSP ) ||
	     !g_ProbeSkinningBuffer_PSP.Create( &g_ProbeSkinnedModel_PSP, &g_ProbeSkeleton_PSP ) ||
	     !g_ProbeSkeletonInstance_PSP.SetAnimationPose( g_ProbeAnimation_PSP, 0 ) ||
	     !g_ProbeSkinningBuffer_PSP.Skin( g_ProbeSkeletonInstance_PSP ) )
	{
		return TFALSE;
	}

	const Toshi::TVertexPoolResource_PSP* pFirstBuffer = g_ProbeSkinningBuffer_PSP.GetVertices();
	if ( pFirstBuffer == TNULL || pFirstBuffer->GetNumVertices() == 0 )
		return TFALSE;
	const auto* pFirstVertices = static_cast<const Toshi::TModelTextureVertex_PSP*>( pFirstBuffer->GetData() );
	if ( !IsAnimationValueNear_PSP( pFirstVertices[ 0 ].fY, 1.0f ) ||
	     !g_ProbeSkeletonInstance_PSP.SetAnimationPose( g_ProbeAnimation_PSP, 500 ) ||
	     !g_ProbeSkinningBuffer_PSP.Skin( g_ProbeSkeletonInstance_PSP ) )
	{
		return TFALSE;
	}

	const Toshi::TVertexPoolResource_PSP* pSecondBuffer = g_ProbeSkinningBuffer_PSP.GetVertices();
	if ( pSecondBuffer == TNULL || pSecondBuffer == pFirstBuffer )
		return TFALSE;
	const auto* pSecondVertices = static_cast<const Toshi::TModelTextureVertex_PSP*>( pSecondBuffer->GetData() );
	const Toshi::TAnimationMemoryStats_PSP oStats = Toshi::TAnimationMemory_PSP::GetStats();
	return IsAnimationValueNear_PSP( pSecondVertices[ 0 ].fY, 1.5f ) &&
	       oStats.uiSampleCurrent == 96 &&
	       oStats.uiInstanceCount == 2 &&
	       oStats.uiSkinnedModelCount == 1 &&
	       oStats.uiAnimatedVertices == 6 &&
	       oStats.uiProfiledPoseCount >= 6 &&
	       oStats.uiProfiledSkinCount >= 2;
}

static TBOOL TestAnimationBudgets_PSP()
{
	Toshi::TSkeletonInstance_PSP aInstances[ 7 ];
	TBOOL bCreated = TTRUE;
	for ( TUINT32 i = 0; i < 6; i++ )
		bCreated = aInstances[ i ].Create( &g_ProbeSkeleton_PSP ) && bCreated;
	const TBOOL bInstanceRejected = !aInstances[ 6 ].Create( &g_ProbeSkeleton_PSP );
	for ( TUINT32 i = 0; i < 6; i++ )
		aInstances[ i ].Destroy();

	const TBOOL bSamplesRejected = !Toshi::TAnimationMemory_PSP::ReserveSamples(
	    Toshi::TAnimationMemory_PSP::SAMPLE_BUDGET
	);
	const TBOOL bVerticesRejected = !Toshi::TAnimationMemory_PSP::ReserveSkinning(
	    Toshi::TAnimationMemory_PSP::ANIMATED_VERTEX_BUDGET
	);
	const Toshi::TAnimationMemoryStats_PSP oStats = Toshi::TAnimationMemory_PSP::GetStats();
	return bCreated && bInstanceRejected && bSamplesRejected && bVerticesRejected &&
	       oStats.uiInstanceCount == 2 &&
	       oStats.uiSkinnedModelCount == 1 &&
	       oStats.uiAnimatedVertices == 6;
}

static TBOOL SubmitBootstrapWave_PSP()
{
	TINT16* pOutput = g_SoundManager_PSP.LockOutputBuffer();
	if ( pOutput == TNULL )
		return TFALSE;

	const TBOOL bMixed = g_SoundMixer_PSP.Mix(
	    pOutput,
	    ASoundManager_PSP::BUFFER_SAMPLE_FRAMES
	);
	const TBOOL bSubmitted = g_SoundManager_PSP.UnlockOutputBuffer();
	return bMixed && bSubmitted && g_SoundManager_PSP.WaitForSubmittedBuffer();
}

static TBOOL TestBootstrapVoiceControls_PSP()
{
	const ASoundVoiceHandle_PSP hStoppedVoice = g_SoundMixer_PSP.Play(
	    &g_BootstrapWaves_PSP[ 0 ],
	    0,
	    0,
	    TTRUE
	);
	const ASoundPosition_PSP oHalfRightPosition = { 500, 0, 0, 1000 };
	const ASoundPosition_PSP oHalfRightWorldPosition = { 100, 0, -400, 1000 };
	const ASoundPosition_PSP oInvalidPosition = { 0, 0, 0, 0 };
	const ASoundListener_PSP oRotatedListener = {
		100, 0, 100,
		0, -ASoundMixer_PSP::LISTENER_AXIS_SCALE,
		ASoundMixer_PSP::LISTENER_AXIS_SCALE, 0,
	};
	const ASoundListener_PSP oIdentityListener = {
		0, 0, 0,
		ASoundMixer_PSP::LISTENER_AXIS_SCALE, 0,
		0, ASoundMixer_PSP::LISTENER_AXIS_SCALE,
	};
	const ASoundListener_PSP oInvalidListener = {};
	TINT iSpatialPan = 0;
	TUINT uiAttenuation = 0;
	if ( hStoppedVoice == ASoundMixer_PSP::INVALID_VOICE ||
	     !g_SoundMixer_PSP.SetPosition( hStoppedVoice, oHalfRightPosition ) ||
	     !g_SoundMixer_PSP.GetSpatialState( hStoppedVoice, iSpatialPan, uiAttenuation ) ||
	     iSpatialPan != ASoundMixer_PSP::MAX_PAN / 2 ||
	     uiAttenuation != ASoundMixer_PSP::MAX_VOLUME / 2 ||
	     !g_SoundMixer_PSP.SetListener( oRotatedListener ) ||
	     !g_SoundMixer_PSP.SetWorldPosition( hStoppedVoice, oHalfRightWorldPosition ) ||
	     !g_SoundMixer_PSP.GetSpatialState( hStoppedVoice, iSpatialPan, uiAttenuation ) ||
	     iSpatialPan != ASoundMixer_PSP::MAX_PAN / 2 ||
	     uiAttenuation != ASoundMixer_PSP::MAX_VOLUME / 2 ||
	     g_SoundMixer_PSP.SetListener( oInvalidListener ) ||
	     !g_SoundMixer_PSP.SetListener( oIdentityListener ) ||
	     g_SoundMixer_PSP.SetPosition( hStoppedVoice, oInvalidPosition ) ||
	     !g_SoundMixer_PSP.Stop( hStoppedVoice ) ||
	     g_SoundMixer_PSP.SetVolume( hStoppedVoice, ASoundMixer_PSP::MAX_VOLUME ) ||
	     g_SoundMixer_PSP.GetSpatialState( hStoppedVoice, iSpatialPan, uiAttenuation ) )
	{
		return TFALSE;
	}

	const ASoundVoiceHandle_PSP hBootstrapVoice = g_SoundMixer_PSP.Play( &g_BootstrapWaves_PSP[ 0 ] );
	if ( hBootstrapVoice == ASoundMixer_PSP::INVALID_VOICE ||
	     !g_SoundMixer_PSP.SetVolume( hBootstrapVoice, ASoundMixer_PSP::MAX_VOLUME / 2 ) ||
	     !g_SoundMixer_PSP.SetPan( hBootstrapVoice, -ASoundMixer_PSP::MAX_PAN / 2 ) ||
	     !SubmitBootstrapWave_PSP() )
	{
		return TFALSE;
	}

	return !g_SoundMixer_PSP.IsVoiceActive( hBootstrapVoice ) &&
	       !g_SoundMixer_PSP.SetPan( hBootstrapVoice, 0 );
}

static TBOOL TestBootstrapVoiceFrequency_PSP()
{
	const ASoundVoiceHandle_PSP hVoice = g_SoundMixer_PSP.Play(
	    &g_BootstrapWaves_PSP[ 0 ],
	    0,
	    0,
	    TFALSE,
	    ASoundMixer_PSP::DEFAULT_FREQUENCY / 2
	);
	if ( hVoice == ASoundMixer_PSP::INVALID_VOICE ||
	     g_SoundMixer_PSP.SetFrequency( hVoice, ASoundMixer_PSP::MAX_FREQUENCY + 1 ) ||
	     !SubmitBootstrapWave_PSP() ||
	     !g_SoundMixer_PSP.IsVoiceActive( hVoice ) ||
	     !g_SoundMixer_PSP.SetFrequency( hVoice, ASoundMixer_PSP::MAX_FREQUENCY ) ||
	     !SubmitBootstrapWave_PSP() )
	{
		return TFALSE;
	}

	return !g_SoundMixer_PSP.IsVoiceActive( hVoice ) &&
	       !g_SoundMixer_PSP.SetFrequency( hVoice, ASoundMixer_PSP::DEFAULT_FREQUENCY );
}

static TBOOL TestGUIAudioContext_PSP()
{
	g_GUIAudioContext_PSP.PlaySound( -1 );
	if ( g_GUIAudioContext_PSP.GetRejectedSoundCount() != 1 ||
	     g_GUIAudioContext_PSP.WasLastPlayQueued() )
	{
		return TFALSE;
	}
	g_GUIAudioContext_PSP.PlaySound( 1 );
	if ( g_GUIAudioContext_PSP.GetRejectedSoundCount() != 2 ||
	     g_GUIAudioContext_PSP.WasLastPlayQueued() ||
	     g_SoundManager_PSP.GetRejectedCueCount() != 1 )
	{
		return TFALSE;
	}
	g_GUIAudioContext_PSP.PlaySound( ASoundId_PSP_UI_MENUCLICK );
	if ( !g_GUIAudioContext_PSP.WasLastPlayQueued() )
		return TFALSE;
	g_GUIAudioContext_PSP.PlaySound( ASoundId_PSP_UI_MENUBACK );
	if ( !g_GUIAudioContext_PSP.WasLastPlayQueued() )
		return TFALSE;
	g_GUIAudioContext_PSP.PlaySound( ASoundId_PSP_UI_MENUOK );
	return g_GUIAudioContext_PSP.WasLastPlayQueued() &&
	       g_SoundCommandQueue_PSP.GetQueuedCommandCount() == 3 &&
	       g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) &&
	       g_SoundCommandQueue_PSP.GetPlayResultCount() == 0 &&
	       g_SoundManager_PSP.GetRejectedCueCount() == 1 &&
	       g_SoundMixer_PSP.GetActiveVoiceCount() == 3 &&
	       SubmitBootstrapWave_PSP() &&
	       g_SoundMixer_PSP.GetActiveVoiceCount() == 0;
}

static TBOOL TestManagedSoundCue_PSP()
{
	const TINT iFirstCue = g_SoundManager_PSP.PlayCue( ASoundId_PSP_UI_MENUCLICK );
	ASoundVoiceHandle_PSP hVoice = ASoundMixer_PSP::INVALID_VOICE;
	if ( iFirstCue < 0 || !g_SoundManager_PSP.IsCuePlaying( iFirstCue ) ||
	     g_SoundManager_PSP.GetManagedCueCount() != 1 ||
	     g_SoundManager_PSP.GetPendingCueCount() != 1 ||
	     g_SoundManager_PSP.GetCueVoice( iFirstCue, hVoice ) ||
	     !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
	{
		return TFALSE;
	}

	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	if ( g_SoundManager_PSP.GetPendingCueCount() != 0 ||
	     !g_SoundManager_PSP.GetCueVoice( iFirstCue, hVoice ) ||
	     !g_SoundMixer_PSP.IsVoiceActive( hVoice ) ||
	     !SubmitBootstrapWave_PSP() )
	{
		return TFALSE;
	}

	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	if ( g_SoundManager_PSP.IsCuePlaying( iFirstCue ) ||
	     g_SoundManager_PSP.GetManagedCueCount() != 0 )
	{
		return TFALSE;
	}

	const TINT iSecondCue = g_SoundManager_PSP.PlayCue( ASoundId_PSP_UI_MENUOK );
	if ( iSecondCue < 0 || iSecondCue == iFirstCue ||
	     g_SoundManager_PSP.IsCuePlaying( iFirstCue ) ||
	     !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
	{
		return TFALSE;
	}

	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	if ( !g_SoundManager_PSP.GetCueVoice( iSecondCue, hVoice ) ||
	     !SubmitBootstrapWave_PSP() )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	return !g_SoundManager_PSP.IsCuePlaying( iSecondCue ) &&
	       g_SoundManager_PSP.GetManagedCueCount() == 0 &&
	       g_SoundManager_PSP.GetPendingCueCount() == 0 &&
	       g_SoundCommandQueue_PSP.GetPlayResultCount() == 0;
}

static TBOOL TestPlayCueEx_PSP()
{
	const TUINT uiRejectedBefore = g_SoundManager_PSP.GetRejectedCueCount();
	if ( g_SoundManager_PSP.PlayCueEx(
	         ASoundId_PSP_UI_MENUOK, 1.1f, TFALSE, 0.0f, -1
	     ) != -1 ||
	     g_SoundManager_PSP.PlayCueEx(
	         ASoundId_PSP_UI_MENUOK, 1.0f, TFALSE, -1.0f, -1
	     ) != -1 ||
	     g_SoundManager_PSP.PlayCueEx(
	         ASoundId_PSP_UI_MENUOK, 1.0f, TFALSE, 0.0f, 0
	     ) != -1 ||
	     g_SoundManager_PSP.GetRejectedCueCount() != uiRejectedBefore + 3 )
	{
		return TFALSE;
	}

	const TFLOAT fDelay = TFLOAT( ASoundManager_PSP::BUFFER_SAMPLE_FRAMES ) /
	                       TFLOAT( ASoundManager_PSP::SAMPLE_RATE );
	const TINT iCue = g_SoundManager_PSP.PlayCueEx(
	    ASoundId_PSP_UI_MENUOK,
	    0.5f,
	    TFALSE,
	    fDelay,
	    -1
	);
	ASoundVoiceHandle_PSP hVoice = ASoundMixer_PSP::INVALID_VOICE;
	if ( iCue < 0 || g_SoundManager_PSP.GetPendingCueCount() != 1 ||
	     g_DelayedEventQueue_PSP.GetEventCount() != 1 ||
	     g_SoundCommandQueue_PSP.GetQueuedCommandCount() != 0 ||
	     g_SoundManager_PSP.GetCueVoice( iCue, hVoice ) )
	{
		return TFALSE;
	}

	g_DelayedEventQueue_PSP.Update(
	    ASoundManager_PSP::BUFFER_SAMPLE_FRAMES / 2,
	    g_SoundCommandQueue_PSP
	);
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	if ( g_DelayedEventQueue_PSP.GetEventCount() != 1 ||
	     g_SoundCommandQueue_PSP.GetQueuedCommandCount() != 0 )
	{
		return TFALSE;
	}

	g_DelayedEventQueue_PSP.Update(
	    ASoundManager_PSP::BUFFER_SAMPLE_FRAMES / 2,
	    g_SoundCommandQueue_PSP
	);
	if ( g_DelayedEventQueue_PSP.GetEventCount() != 0 ||
	     g_SoundCommandQueue_PSP.GetQueuedCommandCount() != 1 ||
	     !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
	{
		return TFALSE;
	}
	g_DelayedEventQueue_PSP.Update( 0, g_SoundCommandQueue_PSP );
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	if ( !g_SoundManager_PSP.GetCueVoice( iCue, hVoice ) ||
	     !g_SoundMixer_PSP.IsVoiceActive( hVoice ) )
	{
		return TFALSE;
	}

	TINT16* pOutput = g_SoundManager_PSP.LockOutputBuffer();
	if ( pOutput == TNULL )
		return TFALSE;
	const TBOOL bMixed = g_SoundMixer_PSP.Mix(
	    pOutput,
	    ASoundManager_PSP::BUFFER_SAMPLE_FRAMES
	);
	const TBOOL bHalfVolume = pOutput[ 0 ] == 4000 && pOutput[ 1 ] == 4000;
	const TBOOL bSubmitted = g_SoundManager_PSP.UnlockOutputBuffer() &&
	                         g_SoundManager_PSP.WaitForSubmittedBuffer();
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	return bMixed && bHalfVolume && bSubmitted &&
	       !g_SoundManager_PSP.IsCuePlaying( iCue ) &&
	       g_SoundManager_PSP.GetManagedCueCount() == 0 &&
	       g_SoundManager_PSP.GetPendingCueCount() == 0 &&
	       g_DelayedEventQueue_PSP.GetPendingPlayResultCount() == 0 &&
	       g_DelayedEventQueue_PSP.GetReadyPlayResultCount() == 0 &&
	       g_SoundCommandQueue_PSP.GetPlayResultCount() == 0;
}

static TBOOL TestManagedCueControls_PSP()
{
	TINT iCue = g_SoundManager_PSP.PlayCueEx(
	    ASoundId_PSP_UI_MENUOK,
	    1.0f,
	    TFALSE,
	    TFLOAT( ASoundManager_PSP::BUFFER_SAMPLE_FRAMES * 2 ) /
	        TFLOAT( ASoundManager_PSP::SAMPLE_RATE ),
	    -1
	);
	if ( iCue < 0 || g_DelayedEventQueue_PSP.GetEventCount() != 1 )
		return TFALSE;
	g_SoundManager_PSP.SetCueVolume( iCue, 0.25f );
	const TINT iCancelledCue = iCue;
	g_SoundManager_PSP.StopCue( iCue );
	if ( iCue != -1 || g_SoundManager_PSP.IsCuePlaying( iCancelledCue ) ||
	     g_DelayedEventQueue_PSP.GetEventCount() != 0 ||
	     g_SoundManager_PSP.GetManagedCueCount() != 0 ||
	     g_SoundManager_PSP.GetPendingCueCount() != 0 )
	{
		return TFALSE;
	}

	iCue = g_SoundManager_PSP.PlayCue( ASoundId_PSP_UI_MENUOK );
	if ( iCue < 0 )
		return TFALSE;
	g_SoundManager_PSP.SetCueVolume( iCue, 0.25f );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );

	TINT16* pOutput = g_SoundManager_PSP.LockOutputBuffer();
	if ( pOutput == TNULL )
		return TFALSE;
	const TBOOL bMixed = g_SoundMixer_PSP.Mix(
	    pOutput,
	    ASoundManager_PSP::BUFFER_SAMPLE_FRAMES
	);
	const TBOOL bQuarterVolume = pOutput[ 0 ] == 2000 && pOutput[ 1 ] == 2000;
	const TBOOL bSubmitted = g_SoundManager_PSP.UnlockOutputBuffer() &&
	                         g_SoundManager_PSP.WaitForSubmittedBuffer();
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	if ( !bMixed || !bQuarterVolume || !bSubmitted ||
	     g_SoundManager_PSP.IsCuePlaying( iCue ) )
	{
		return TFALSE;
	}

	iCue = g_SoundManager_PSP.PlayCue( ASoundId_PSP_UI_MENUCLICK );
	if ( iCue < 0 )
		return TFALSE;
	const TINT iStoppedCue = iCue;
	g_SoundManager_PSP.StopCue( iCue );
	if ( iCue != -1 || !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	return !g_SoundManager_PSP.IsCuePlaying( iStoppedCue ) &&
	       g_SoundMixer_PSP.GetActiveVoiceCount() == 0 &&
	       g_SoundManager_PSP.GetManagedCueCount() == 0 &&
	       g_SoundManager_PSP.GetPendingCueCount() == 0 &&
	       g_SoundCommandQueue_PSP.GetPlayResultCount() == 0;
}

static TBOOL TestDelayedCueControls_PSP()
{
	const TFLOAT fOneFrameDelay = 1.0f / TFLOAT( ASoundManager_PSP::SAMPLE_RATE );
	TINT iCue = g_SoundManager_PSP.PlayCue( ASoundId_PSP_UI_MENUOK );
	if ( iCue < 0 || !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	g_SoundManager_PSP.SetCueVolume( iCue, 0.25f, fOneFrameDelay );
	if ( g_DelayedEventQueue_PSP.GetEventCount() != 1 )
		return TFALSE;
	g_DelayedEventQueue_PSP.Update( 1, g_SoundCommandQueue_PSP );
	if ( g_DelayedEventQueue_PSP.GetEventCount() != 0 ||
	     !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );

	TINT16* pOutput = g_SoundManager_PSP.LockOutputBuffer();
	if ( pOutput == TNULL )
		return TFALSE;
	const TBOOL bMixed = g_SoundMixer_PSP.Mix(
	    pOutput,
	    ASoundManager_PSP::BUFFER_SAMPLE_FRAMES
	);
	const TBOOL bQuarterVolume = pOutput[ 0 ] == 2000 && pOutput[ 1 ] == 2000;
	const TBOOL bSubmitted = g_SoundManager_PSP.UnlockOutputBuffer() &&
	                         g_SoundManager_PSP.WaitForSubmittedBuffer();
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	if ( !bMixed || !bQuarterVolume || !bSubmitted ||
	     g_SoundManager_PSP.IsCuePlaying( iCue ) )
	{
		return TFALSE;
	}

	iCue = g_SoundManager_PSP.PlayCue( ASoundId_PSP_UI_MENUCLICK );
	if ( iCue < 0 || !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	ASoundVoiceHandle_PSP hVoice = ASoundMixer_PSP::INVALID_VOICE;
	if ( !g_SoundManager_PSP.GetCueVoice( iCue, hVoice ) )
		return TFALSE;
	const TINT iStoppedCue = iCue;
	g_SoundManager_PSP.StopCueAsync( iCue, fOneFrameDelay );
	if ( iCue != -1 || g_DelayedEventQueue_PSP.GetEventCount() != 1 ||
	     !g_SoundMixer_PSP.IsVoiceActive( hVoice ) )
	{
		return TFALSE;
	}
	g_DelayedEventQueue_PSP.Update( 1, g_SoundCommandQueue_PSP );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	return !g_SoundManager_PSP.IsCuePlaying( iStoppedCue ) &&
	       !g_SoundMixer_PSP.IsVoiceActive( hVoice ) &&
	       g_DelayedEventQueue_PSP.GetEventCount() == 0 &&
	       g_SoundManager_PSP.GetManagedCueCount() == 0 &&
	       g_SoundCommandQueue_PSP.GetPlayResultCount() == 0;
}

static TBOOL TestManagedCueFrequency_PSP()
{
	const TFLOAT fOneFrameDelay = 1.0f / TFLOAT( ASoundManager_PSP::SAMPLE_RATE );
	TINT iCue = g_SoundManager_PSP.PlayCue( ASoundId_PSP_UI_MENUOK );
	if ( iCue < 0 || !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );

	ASoundVoiceHandle_PSP hVoice = ASoundMixer_PSP::INVALID_VOICE;
	TUINT uiFrequency = 0;
	if ( !g_SoundManager_PSP.GetCueVoice( iCue, hVoice ) )
		return TFALSE;
	g_SoundManager_PSP.SetCueFrequency( iCue, 2.0f );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     !g_SoundMixer_PSP.GetFrequency( hVoice, uiFrequency ) ||
	     uiFrequency != ASoundMixer_PSP::MAX_FREQUENCY )
	{
		return TFALSE;
	}

	g_SoundManager_PSP.SetCueFrequency( iCue, 2.01f );
	if ( !g_SoundMixer_PSP.GetFrequency( hVoice, uiFrequency ) ||
	     uiFrequency != ASoundMixer_PSP::MAX_FREQUENCY )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.SetCueFrequency( iCue, 0.25f, fOneFrameDelay );
	if ( g_DelayedEventQueue_PSP.GetEventCount() != 1 ||
	     !g_SoundMixer_PSP.GetFrequency( hVoice, uiFrequency ) ||
	     uiFrequency != ASoundMixer_PSP::MAX_FREQUENCY )
	{
		return TFALSE;
	}
	g_DelayedEventQueue_PSP.Update( 1, g_SoundCommandQueue_PSP );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     !g_SoundMixer_PSP.GetFrequency( hVoice, uiFrequency ) ||
	     uiFrequency != ASoundMixer_PSP::MIN_FREQUENCY ||
	     !SubmitBootstrapWave_PSP() ||
	     !g_SoundMixer_PSP.IsVoiceActive( hVoice ) ||
	     !SubmitBootstrapWave_PSP() ||
	     g_SoundMixer_PSP.IsVoiceActive( hVoice ) )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	if ( g_SoundManager_PSP.IsCuePlaying( iCue ) )
		return TFALSE;

	iCue = g_SoundManager_PSP.PlayCueEx(
	    ASoundId_PSP_UI_MENUCLICK,
	    1.0f,
	    TFALSE,
	    fOneFrameDelay,
	    -1
	);
	if ( iCue < 0 )
		return TFALSE;
	g_SoundManager_PSP.SetCueFrequency( iCue, 0.5f, fOneFrameDelay );
	g_DelayedEventQueue_PSP.Update( 1, g_SoundCommandQueue_PSP );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_DelayedEventQueue_PSP.Update( 0, g_SoundCommandQueue_PSP );
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	if ( !g_SoundManager_PSP.GetCueVoice( iCue, hVoice ) ||
	     !g_SoundMixer_PSP.GetFrequency( hVoice, uiFrequency ) ||
	     uiFrequency != ASoundMixer_PSP::DEFAULT_FREQUENCY / 2 )
	{
		return TFALSE;
	}

	const TINT iStoppedCue = iCue;
	g_SoundManager_PSP.StopCue( iCue );
	if ( iCue != -1 || !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	return !g_SoundManager_PSP.IsCuePlaying( iStoppedCue ) &&
	       g_SoundMixer_PSP.GetActiveVoiceCount() == 0 &&
	       g_DelayedEventQueue_PSP.GetEventCount() == 0 &&
	       g_SoundManager_PSP.GetManagedCueCount() == 0 &&
	       g_SoundCommandQueue_PSP.GetPlayResultCount() == 0;
}

static TBOOL TestManagedCuePosition_PSP()
{
	const TFLOAT fOneFrameDelay = 1.0f / TFLOAT( ASoundManager_PSP::SAMPLE_RATE );
	const Toshi::TVector4 oHalfRight( 50.0f, 0.0f, 0.0f );
	const Toshi::TVector4 oHalfLeft( -50.0f, 0.0f, 0.0f );
	const Toshi::TVector4 oInvalid( 1001.0f, 0.0f, 0.0f );
	TINT iCue = g_SoundManager_PSP.PlayCue( ASoundId_PSP_UI_MENUOK );
	if ( iCue < 0 || !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );

	ASoundVoiceHandle_PSP hVoice = ASoundMixer_PSP::INVALID_VOICE;
	TINT iPan = 0;
	TUINT uiAttenuation = 0;
	if ( !g_SoundManager_PSP.GetCueVoice( iCue, hVoice ) )
		return TFALSE;
	g_SoundManager_PSP.SetCuePosition( iCue, oHalfRight );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     !g_SoundMixer_PSP.GetSpatialState( hVoice, iPan, uiAttenuation ) ||
	     iPan != ASoundMixer_PSP::MAX_PAN / 2 ||
	     uiAttenuation != ASoundMixer_PSP::MAX_VOLUME / 2 )
	{
		return TFALSE;
	}

	g_SoundManager_PSP.SetCuePosition( iCue, oInvalid );
	if ( g_SoundCommandQueue_PSP.GetQueuedCommandCount() != 0 )
		return TFALSE;
	g_SoundManager_PSP.SetCuePosition( iCue, oHalfLeft, fOneFrameDelay );
	if ( g_DelayedEventQueue_PSP.GetEventCount() != 1 ||
	     !g_SoundMixer_PSP.GetSpatialState( hVoice, iPan, uiAttenuation ) ||
	     iPan != ASoundMixer_PSP::MAX_PAN / 2 )
	{
		return TFALSE;
	}
	g_DelayedEventQueue_PSP.Update( 1, g_SoundCommandQueue_PSP );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     !g_SoundMixer_PSP.GetSpatialState( hVoice, iPan, uiAttenuation ) ||
	     iPan != -ASoundMixer_PSP::MAX_PAN / 2 ||
	     uiAttenuation != ASoundMixer_PSP::MAX_VOLUME / 2 )
	{
		return TFALSE;
	}

	const TINT iStoppedCue = iCue;
	g_SoundManager_PSP.StopCue( iCue );
	if ( iCue != -1 || !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	return !g_SoundManager_PSP.IsCuePlaying( iStoppedCue ) &&
	       g_SoundMixer_PSP.GetActiveVoiceCount() == 0 &&
	       g_DelayedEventQueue_PSP.GetEventCount() == 0 &&
	       g_SoundManager_PSP.GetManagedCueCount() == 0;
}

static TBOOL TestPositionalCuePlayback_PSP()
{
	const TFLOAT fOneFrameDelay = 1.0f / TFLOAT( ASoundManager_PSP::SAMPLE_RATE );
	const Toshi::TVector4 oHalfRight( 50.0f, 0.0f, 0.0f );
	const Toshi::TVector4 oHalfLeft( -50.0f, 0.0f, 0.0f );
	const Toshi::TVector4 oInvalid( 1001.0f, 0.0f, 0.0f );
	const TUINT uiRejectedBefore = g_SoundManager_PSP.GetRejectedCueCount();
	if ( g_SoundManager_PSP.PlayCueAtPosition(
	         ASoundId_PSP_UI_MENUCLICK,
	         oInvalid
	     ) != -1 ||
	     g_SoundManager_PSP.GetRejectedCueCount() != uiRejectedBefore + 1 )
	{
		return TFALSE;
	}

	TINT iCue = g_SoundManager_PSP.PlayCueAtPosition(
	    ASoundId_PSP_UI_MENUCLICK,
	    oHalfRight
	);
	if ( iCue < 0 || !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;

	ASoundVoiceHandle_PSP hVoice = ASoundMixer_PSP::INVALID_VOICE;
	TINT iPan = 0;
	TUINT uiAttenuation = 0;
	if ( !g_SoundManager_PSP.GetCueVoice( iCue, hVoice ) ||
	     !g_SoundMixer_PSP.GetSpatialState( hVoice, iPan, uiAttenuation ) ||
	     iPan != ASoundMixer_PSP::MAX_PAN / 2 ||
	     uiAttenuation != ASoundMixer_PSP::MAX_VOLUME / 2 )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.StopCue( iCue );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );

	iCue = g_SoundManager_PSP.PlayCueAtPositionEx(
	    ASoundId_PSP_UI_MENUCLICK,
	    oHalfLeft,
	    0.5f,
	    TFALSE,
	    fOneFrameDelay,
	    -1
	);
	if ( iCue < 0 || g_DelayedEventQueue_PSP.GetEventCount() != 1 )
		return TFALSE;
	g_DelayedEventQueue_PSP.Update( 1, g_SoundCommandQueue_PSP );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_DelayedEventQueue_PSP.Update( 0, g_SoundCommandQueue_PSP );
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     !g_SoundManager_PSP.GetCueVoice( iCue, hVoice ) ||
	     !g_SoundMixer_PSP.GetSpatialState( hVoice, iPan, uiAttenuation ) ||
	     iPan != -ASoundMixer_PSP::MAX_PAN / 2 ||
	     uiAttenuation != ASoundMixer_PSP::MAX_VOLUME / 2 )
	{
		return TFALSE;
	}

	const TINT iStoppedCue = iCue;
	g_SoundManager_PSP.StopCue( iCue );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	return !g_SoundManager_PSP.IsCuePlaying( iStoppedCue ) &&
	       g_SoundMixer_PSP.GetActiveVoiceCount() == 0 &&
	       g_DelayedEventQueue_PSP.GetEventCount() == 0 &&
	       g_SoundManager_PSP.GetManagedCueCount() == 0;
}

static TBOOL TestManagedCuePause_PSP()
{
	TINT iCue = g_SoundManager_PSP.PlayCueEx(
	    ASoundId_PSP_UI_MENUOK,
	    1.0f,
	    TTRUE,
	    0.0f,
	    -1
	);
	if ( iCue < 0 || !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;

	ASoundVoiceHandle_PSP hVoice = ASoundMixer_PSP::INVALID_VOICE;
	if ( !g_SoundManager_PSP.GetCueVoice( iCue, hVoice ) ||
	     !g_SoundMixer_PSP.IsVoicePaused( hVoice ) )
	{
		return TFALSE;
	}

	TINT16* pOutput = g_SoundManager_PSP.LockOutputBuffer();
	if ( pOutput == TNULL )
		return TFALSE;
	const TBOOL bMixedWhilePaused = g_SoundMixer_PSP.Mix(
	    pOutput,
	    ASoundManager_PSP::BUFFER_SAMPLE_FRAMES
	);
	const TBOOL bPausedSilence = pOutput[ 0 ] == 0 && pOutput[ 1 ] == 0;
	const TBOOL bPausedSubmitted = g_SoundManager_PSP.UnlockOutputBuffer() &&
	                              g_SoundManager_PSP.WaitForSubmittedBuffer();
	if ( bMixedWhilePaused || !bPausedSilence || !bPausedSubmitted ||
	     !g_SoundMixer_PSP.IsVoiceActive( hVoice ) )
	{
		return TFALSE;
	}

	g_SoundManager_PSP.PauseCue( iCue, TFALSE );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     g_SoundMixer_PSP.IsVoicePaused( hVoice ) )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.PauseCue( iCue, TTRUE );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     !g_SoundMixer_PSP.IsVoicePaused( hVoice ) )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.PauseCue( iCue, TFALSE );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     g_SoundMixer_PSP.IsVoicePaused( hVoice ) )
	{
		return TFALSE;
	}

	pOutput = g_SoundManager_PSP.LockOutputBuffer();
	if ( pOutput == TNULL )
		return TFALSE;
	const TBOOL bMixedAfterResume = g_SoundMixer_PSP.Mix(
	    pOutput,
	    ASoundManager_PSP::BUFFER_SAMPLE_FRAMES
	);
	const TBOOL bResumedAtStart = pOutput[ 0 ] == 8000 && pOutput[ 1 ] == 8000;
	const TBOOL bResumeSubmitted = g_SoundManager_PSP.UnlockOutputBuffer() &&
	                              g_SoundManager_PSP.WaitForSubmittedBuffer();
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	if ( !bMixedAfterResume || !bResumedAtStart || !bResumeSubmitted ||
	     g_SoundManager_PSP.IsCuePlaying( iCue ) ||
	     g_SoundManager_PSP.GetManagedCueCount() != 0 ||
	     g_SoundMixer_PSP.GetActiveVoiceCount() != 0 )
	{
		return TFALSE;
	}

	TINT iFirstCue = g_SoundManager_PSP.PlayCue( ASoundId_PSP_UI_MENUCLICK );
	TINT iSecondCue = g_SoundManager_PSP.PlayCue( ASoundId_PSP_UI_MENUBACK );
	if ( iFirstCue < 0 || iSecondCue < 0 ||
	     !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	ASoundVoiceHandle_PSP hFirstVoice = ASoundMixer_PSP::INVALID_VOICE;
	ASoundVoiceHandle_PSP hSecondVoice = ASoundMixer_PSP::INVALID_VOICE;
	if ( !g_SoundManager_PSP.GetCueVoice( iFirstCue, hFirstVoice ) ||
	     !g_SoundManager_PSP.GetCueVoice( iSecondCue, hSecondVoice ) )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.PauseAllCues( TTRUE );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     !g_SoundMixer_PSP.IsVoicePaused( hFirstVoice ) ||
	     !g_SoundMixer_PSP.IsVoicePaused( hSecondVoice ) )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.PauseAllCues( TFALSE );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     g_SoundMixer_PSP.IsVoicePaused( hFirstVoice ) ||
	     g_SoundMixer_PSP.IsVoicePaused( hSecondVoice ) )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.StopCue( iFirstCue );
	g_SoundManager_PSP.StopCue( iSecondCue );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	return g_SoundManager_PSP.GetManagedCueCount() == 0 &&
	       g_SoundMixer_PSP.GetActiveVoiceCount() == 0;
}

static TBOOL TestManagedCueQueries_PSP()
{
	if ( g_SoundManager_PSP.IsCueUsed( -1 ) ||
	     g_SoundManager_PSP.GetCueVolume( -1 ) != 0.0f ||
	     g_SoundManager_PSP.GetCueSoundId( -1 ) != ASOUNDID_PSP_INVALID ||
	     g_SoundManager_PSP.FindPlayingCue( ASoundId_PSP_UI_MENUOK ) != -1 )
	{
		return TFALSE;
	}

	TINT iCue = g_SoundManager_PSP.PlayCue( ASoundId_PSP_UI_MENUOK );
	if ( iCue < 0 || !g_SoundManager_PSP.IsCueUsed( iCue ) ||
	     g_SoundManager_PSP.GetCueVolume( iCue ) != 1.0f ||
	     g_SoundManager_PSP.GetCueSoundId( iCue ) != ASoundId_PSP_UI_MENUOK ||
	     g_SoundManager_PSP.FindPlayingCue( ASoundId_PSP_UI_MENUOK ) != iCue )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.SetCueVolume( iCue, 0.25f );
	if ( g_SoundManager_PSP.GetCueVolume( iCue ) != 0.25f ||
	     !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;

	const TINT iStoppedCue = iCue;
	g_SoundManager_PSP.StopCue( iCue );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	return !g_SoundManager_PSP.IsCueUsed( iStoppedCue ) &&
	       g_SoundManager_PSP.GetCueVolume( iStoppedCue ) == 0.0f &&
	       g_SoundManager_PSP.GetCueSoundId( iStoppedCue ) == ASOUNDID_PSP_INVALID &&
	       g_SoundManager_PSP.FindPlayingCue( ASoundId_PSP_UI_MENUOK ) == -1;
}

static TBOOL TestManagedCueFade_PSP()
{
	const TFLOAT fOneFrame = 1.0f / TFLOAT( ASoundManager_PSP::SAMPLE_RATE );
	TINT iCue = g_SoundManager_PSP.PlayCue( ASoundId_PSP_UI_MENUOK );
	if ( iCue < 0 || !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	g_SoundManager_PSP.FadeCue( iCue, fOneFrame * 2.0f, 0.0f, 0.0f );
	if ( !g_SoundManager_PSP.IsCueFading( iCue ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateFades( 1 );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     g_SoundManager_PSP.GetCueVolume( iCue ) != 0.5f ||
	     !g_SoundManager_PSP.IsCueFading( iCue ) )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.UpdateFades( 1 );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     g_SoundManager_PSP.GetCueVolume( iCue ) != 0.0f ||
	     g_SoundManager_PSP.IsCueFading( iCue ) )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.StopCue( iCue );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );

	iCue = g_SoundManager_PSP.PlayCue( ASoundId_PSP_UI_MENUCLICK );
	if ( iCue < 0 || !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	const TINT iStoppedCue = iCue;
	g_SoundManager_PSP.FadeOutCue( iCue, fOneFrame, TTRUE, 0.0f );
	if ( !g_SoundManager_PSP.IsCueFading( iCue ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateFades( 1 );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	return !g_SoundManager_PSP.IsCueUsed( iStoppedCue ) &&
	       !g_SoundManager_PSP.IsCueFading( iStoppedCue ) &&
	       g_SoundManager_PSP.GetManagedCueCount() == 0 &&
	       g_SoundMixer_PSP.GetActiveVoiceCount() == 0;
}

static TBOOL TestGlobalCueVolume_PSP()
{
	g_SoundManager_PSP.SetGlobalVolume( 0.5f );
	TINT iCue = g_SoundManager_PSP.PlayCue( ASoundId_PSP_UI_MENUOK );
	if ( iCue < 0 || !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	ASoundVoiceHandle_PSP hVoice = ASoundMixer_PSP::INVALID_VOICE;
	TUINT uiAppliedVolume = 0;
	if ( !g_SoundManager_PSP.GetCueVoice( iCue, hVoice ) ||
	     !g_SoundMixer_PSP.GetVolume( hVoice, uiAppliedVolume ) ||
	     uiAppliedVolume != ASoundMixer_PSP::MAX_VOLUME / 2 ||
	     g_SoundManager_PSP.GetCueVolume( iCue ) != 1.0f )
	{
		return TFALSE;
	}

	g_SoundManager_PSP.SetCueVolume( iCue, 0.5f );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     !g_SoundMixer_PSP.GetVolume( hVoice, uiAppliedVolume ) ||
	     uiAppliedVolume != ASoundMixer_PSP::MAX_VOLUME / 4 ||
	     g_SoundManager_PSP.GetCueVolume( iCue ) != 0.5f )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.SetGlobalVolume( 0.25f );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     !g_SoundMixer_PSP.GetVolume( hVoice, uiAppliedVolume ) ||
	     uiAppliedVolume != ASoundMixer_PSP::MAX_VOLUME / 8 ||
	     g_SoundManager_PSP.GetCueVolume( iCue ) != 0.5f )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.StopCue( iCue );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );

	const TFLOAT fOneFrame = 1.0f / TFLOAT( ASoundManager_PSP::SAMPLE_RATE );
	iCue = g_SoundManager_PSP.PlayCueEx(
	    ASoundId_PSP_UI_MENUCLICK,
	    0.5f,
	    TFALSE,
	    fOneFrame,
	    -1
	);
	if ( iCue < 0 )
		return TFALSE;
	g_SoundManager_PSP.SetGlobalVolume( 0.5f );
	g_DelayedEventQueue_PSP.Update( 1, g_SoundCommandQueue_PSP );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_DelayedEventQueue_PSP.Update( 0, g_SoundCommandQueue_PSP );
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	if ( !g_SoundManager_PSP.GetCueVoice( iCue, hVoice ) ||
	     !g_SoundMixer_PSP.GetVolume( hVoice, uiAppliedVolume ) ||
	     uiAppliedVolume != ASoundMixer_PSP::MAX_VOLUME / 4 ||
	     g_SoundManager_PSP.GetCueVolume( iCue ) != 0.5f )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.StopCue( iCue );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	g_SoundManager_PSP.SetGlobalVolume( 1.0f );
	return g_SoundManager_PSP.GetManagedCueCount() == 0 &&
	       g_SoundMixer_PSP.GetActiveVoiceCount() == 0;
}

static TBOOL TestManagedStreamCue_PSP()
{
	TINT iCue = g_SoundManager_PSP.PlayCue( ASoundId_PSP_BOOTSTRAP_STREAM );
	if ( iCue < 0 || !g_SoundManager_PSP.IsCueStreamReady( iCue ) ||
	     !g_SoundManager_PSP.IsStreamCueActive( iCue ) ||
	     !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	ASoundVoiceHandle_PSP hVoice = ASoundMixer_PSP::INVALID_VOICE;
	TUINT uiFrequency = 0;
	if ( !g_SoundManager_PSP.GetCueVoice( iCue, hVoice ) ||
	     !g_SoundMixer_PSP.GetFrequency( hVoice, uiFrequency ) ||
	     uiFrequency != ASoundMixer_PSP::DEFAULT_FREQUENCY )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.SetCueFrequency( iCue, 0.5f );
	if ( g_SoundCommandQueue_PSP.GetQueuedCommandCount() != 0 )
		return TFALSE;
	g_SoundManager_PSP.PauseCue( iCue, TTRUE );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     !g_SoundMixer_PSP.IsVoicePaused( hVoice ) )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.PauseCue( iCue, TFALSE );
	g_SoundManager_PSP.SetCueVolume( iCue, 0.5f );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     g_SoundMixer_PSP.IsVoicePaused( hVoice ) ||
	     g_SoundManager_PSP.GetStreamCueTime( iCue ) != 0 ||
	     !SubmitBootstrapWave_PSP() ||
	     g_SoundManager_PSP.GetStreamCueTime( iCue ) != 23 )
	{
		return TFALSE;
	}
	g_SoundManager_PSP.SetStreamCueTime( iCue, 0 );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     g_SoundManager_PSP.GetStreamCueTime( iCue ) != 0 )
	{
		return TFALSE;
	}

	const TINT iStoppedCue = iCue;
	g_SoundManager_PSP.StopCue( iCue );
	if ( !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) )
		return TFALSE;
	g_SoundManager_PSP.UpdateCues( g_SoundMixer_PSP );
	return !g_SoundManager_PSP.IsCueStreamReady( iStoppedCue ) &&
	       !g_SoundManager_PSP.IsStreamCueActive( iStoppedCue ) &&
	       g_SoundManager_PSP.GetManagedCueCount() == 0 &&
	       g_SoundMixer_PSP.GetActiveVoiceCount() == 0;
}

static TBOOL TestBootstrapStreamLoop_PSP()
{
	g_SoundCommandQueue_PSP.Clear();
	const ASoundEvent_PSP oPlayEvent = {
		ASoundEventType_PSP_PlayAudio,
		"BOOTSTRAP_STREAM",
		ASoundMixer_PSP::INVALID_VOICE,
		0,
		0,
		TTRUE,
	};
	ASoundCommandRequest_PSP uiRequest = ASoundCommandQueue_PSP::INVALID_REQUEST;
	ASoundVoiceHandle_PSP hVoice = ASoundMixer_PSP::INVALID_VOICE;
	if ( g_BootstrapStream_PSP.GetLoopStartFrame() != 1024 ||
	     g_BootstrapStream_PSP.GetLoopEndFrame() != 3072 ||
	     g_SoundCommandQueue_PSP.QueuePlayNamed(
	         g_WaveBank_PSP,
	         "BOOTSTRAP_STREAM",
	         0,
	         0,
	         TTRUE,
	         ASoundMixer_PSP::DEFAULT_FREQUENCY / 2
	     ) != ASoundCommandQueue_PSP::INVALID_REQUEST ||
	     !g_SoundCommandQueue_PSP.QueueEvent( g_WaveBank_PSP, oPlayEvent, uiRequest ) ||
	     !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     !g_SoundCommandQueue_PSP.GetPlayResult( uiRequest, hVoice ) ||
	     hVoice == ASoundMixer_PSP::INVALID_VOICE )
	{
		g_SoundCommandQueue_PSP.Clear();
		return TFALSE;
	}

	for ( TUINT uiBuffer = 0; uiBuffer < 4; uiBuffer++ )
	{
		if ( !SubmitBootstrapWave_PSP() ||
		     !g_SoundMixer_PSP.IsVoiceActive( hVoice ) )
		{
			return TFALSE;
		}
	}

	const TBOOL bStopped = g_SoundCommandQueue_PSP.QueueStop( hVoice ) &&
	                       g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) &&
	                       !g_SoundMixer_PSP.IsVoiceActive( hVoice );
	g_SoundCommandQueue_PSP.Clear();
	return bStopped && g_BootstrapStream_PSP.GetUnderrunCount() == 0;
}

static TBOOL TestBootstrapCompressedStream_PSP()
{
	const TINT16* pFirstFrame = g_BootstrapStream_PSP.GetFrame( 0 );
	return pFirstFrame != TNULL &&
	       pFirstFrame[ 0 ] == 1200 && pFirstFrame[ 1 ] == 1200 &&
	       TestBootstrapStreamLoop_PSP();
}

static TBOOL TestSoundCommandQueue_PSP()
{
	ASoundCommandRequest_PSP uiRequest = ASoundCommandQueue_PSP::INVALID_REQUEST;
	const ASoundEvent_PSP oPlayEvent = {
		ASoundEventType_PSP_PlayAudio,
		"BOOTSTRAP",
		ASoundMixer_PSP::INVALID_VOICE,
		0,
		0,
		TTRUE,
	};
	const ASoundEvent_PSP oMissingPlayEvent = {
		ASoundEventType_PSP_PlayAudio,
		"missing",
		ASoundMixer_PSP::INVALID_VOICE,
		ASoundMixer_PSP::MAX_VOLUME,
		0,
		TFALSE,
	};
	ASoundVoiceHandle_PSP hVoice = ASoundMixer_PSP::INVALID_VOICE;
	if ( g_SoundCommandQueue_PSP.QueueEvent( g_WaveBank_PSP, oMissingPlayEvent, uiRequest ) ||
	     uiRequest != ASoundCommandQueue_PSP::INVALID_REQUEST ||
	     !g_SoundCommandQueue_PSP.QueueEvent( g_WaveBank_PSP, oPlayEvent, uiRequest ) ||
	     !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     !g_SoundCommandQueue_PSP.GetPlayResult( uiRequest, hVoice ) ||
	     hVoice == ASoundMixer_PSP::INVALID_VOICE )
	{
		g_SoundCommandQueue_PSP.Clear();
		return TFALSE;
	}

	ASoundCommandRequest_PSP uiControlRequest = ASoundCommandQueue_PSP::INVALID_REQUEST;
	const ASoundEvent_PSP oVolumeEvent = {
		ASoundEventType_PSP_UpdateChannelVolume, TNULL, hVoice,
		ASoundMixer_PSP::MAX_VOLUME / 4, 0, TFALSE,
	};
	const ASoundEvent_PSP oPanEvent = {
		ASoundEventType_PSP_UpdateChannelPan, TNULL, hVoice,
		0, ASoundMixer_PSP::MAX_PAN / 2, TFALSE,
	};
	const ASoundEvent_PSP oFrequencyEvent = {
		ASoundEventType_PSP_UpdateChannelFrequency, TNULL, hVoice,
		0, 0, TFALSE, ASoundMixer_PSP::DEFAULT_FREQUENCY / 2,
	};
	const ASoundEvent_PSP oPositionEvent = {
		ASoundEventType_PSP_UpdateChannelPosition, TNULL, hVoice,
		0, 0, TFALSE, 0, { 100, 0, -150, 1000 },
	};
	const ASoundListener_PSP oQueuedListener = {
		100, 0, 100,
		0, -ASoundMixer_PSP::LISTENER_AXIS_SCALE,
		ASoundMixer_PSP::LISTENER_AXIS_SCALE, 0,
	};
	const ASoundEvent_PSP oStopEvent = {
		ASoundEventType_PSP_StopAudio, TNULL, hVoice, 0, 0, TFALSE,
	};
	const TBOOL bControlsQueued =
	    g_SoundCommandQueue_PSP.QueueSetListener( oQueuedListener ) &&
	    g_SoundCommandQueue_PSP.QueueEvent( g_WaveBank_PSP, oVolumeEvent, uiControlRequest ) &&
	    uiControlRequest == ASoundCommandQueue_PSP::INVALID_REQUEST &&
	    g_SoundCommandQueue_PSP.QueueEvent( g_WaveBank_PSP, oFrequencyEvent, uiControlRequest ) &&
	    g_SoundCommandQueue_PSP.QueueEvent( g_WaveBank_PSP, oPositionEvent, uiControlRequest ) &&
	    g_SoundCommandQueue_PSP.QueueEvent( g_WaveBank_PSP, oPanEvent, uiControlRequest ) &&
	    g_SoundCommandQueue_PSP.QueueEvent( g_WaveBank_PSP, oStopEvent, uiControlRequest );
	const TBOOL bControlsProcessed = bControlsQueued &&
	                                g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) &&
	                                !g_SoundMixer_PSP.IsVoiceActive( hVoice );
	const TBOOL bStaleQueued = g_SoundCommandQueue_PSP.QueueSetVolume(
	    hVoice,
	    ASoundMixer_PSP::MAX_VOLUME
	);
	const TBOOL bStaleRejected = bStaleQueued &&
	                             !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) &&
	                             g_SoundCommandQueue_PSP.GetFailedCommandCount() == 1;
	g_SoundCommandQueue_PSP.Clear();
	return bControlsProcessed && bStaleRejected;
}

static TBOOL TestDelayedSoundEvents_PSP()
{
	const ASoundVoiceHandle_PSP hLoopingVoice = g_SoundMixer_PSP.Play(
	    &g_BootstrapWaves_PSP[ 0 ], 0, 0, TTRUE
	);
	const ASoundEvent_PSP oStopEvent = {
		ASoundEventType_PSP_StopAudio, TNULL, hLoopingVoice, 0, 0, TFALSE,
	};
	const ASoundScheduledEventHandle_PSP hStopEvent = g_DelayedEventQueue_PSP.Schedule(
	    g_WaveBank_PSP,
	    oStopEvent,
	    ASoundManager_PSP::BUFFER_SAMPLE_FRAMES * 2
	);
	if ( hLoopingVoice == ASoundMixer_PSP::INVALID_VOICE ||
	     hStopEvent == ASoundDelayedEventQueue_PSP::INVALID_EVENT ||
	     !g_DelayedEventQueue_PSP.IsPending( hStopEvent ) ||
	     g_DelayedEventQueue_PSP.Update(
	         ASoundManager_PSP::BUFFER_SAMPLE_FRAMES,
	         g_SoundCommandQueue_PSP
	     ) ||
	     !g_SoundMixer_PSP.IsVoiceActive( hLoopingVoice ) )
	{
		return TFALSE;
	}

	if ( !g_DelayedEventQueue_PSP.Update(
	         ASoundManager_PSP::BUFFER_SAMPLE_FRAMES,
	         g_SoundCommandQueue_PSP
	     ) ||
	     g_DelayedEventQueue_PSP.IsPending( hStopEvent ) ||
	     g_DelayedEventQueue_PSP.Cancel( hStopEvent ) ||
	     !g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) ||
	     g_SoundMixer_PSP.IsVoiceActive( hLoopingVoice ) )
	{
		return TFALSE;
	}

	const ASoundEvent_PSP oLoopingPlayEvent = {
		ASoundEventType_PSP_PlayAudio,
		"bootstrap",
		ASoundMixer_PSP::INVALID_VOICE,
		0,
		0,
		TTRUE,
	};
	const ASoundEvent_PSP oMissingEvent = {
		ASoundEventType_PSP_PlayAudio,
		"missing",
		ASoundMixer_PSP::INVALID_VOICE,
		0,
		0,
		TFALSE,
	};
	const TBOOL bMissingRejected = g_DelayedEventQueue_PSP.Schedule(
	                                   g_WaveBank_PSP, oMissingEvent, 0
	                               ) == ASoundDelayedEventQueue_PSP::INVALID_EVENT &&
	                               g_DelayedEventQueue_PSP.GetRejectedEventCount() == 1;
	const ASoundScheduledEventHandle_PSP hCancelledEvent = g_DelayedEventQueue_PSP.Schedule(
	    g_WaveBank_PSP,
	    oLoopingPlayEvent,
	    ASoundManager_PSP::BUFFER_SAMPLE_FRAMES * 4
	);
	const TBOOL bCancelled = hCancelledEvent != ASoundDelayedEventQueue_PSP::INVALID_EVENT &&
	                         g_DelayedEventQueue_PSP.Cancel( hCancelledEvent ) &&
	                         !g_DelayedEventQueue_PSP.Cancel( hCancelledEvent );
	const ASoundScheduledEventHandle_PSP hPlayEvent = g_DelayedEventQueue_PSP.Schedule(
	    g_WaveBank_PSP,
	    oLoopingPlayEvent,
	    ASoundManager_PSP::BUFFER_SAMPLE_FRAMES
	);
	const TBOOL bPlayDispatched = hPlayEvent != ASoundDelayedEventQueue_PSP::INVALID_EVENT &&
	                              hPlayEvent != hCancelledEvent &&
	                              g_DelayedEventQueue_PSP.IsPending( hPlayEvent ) &&
	                              g_DelayedEventQueue_PSP.Update(
	                                  ASoundManager_PSP::BUFFER_SAMPLE_FRAMES,
	                                  g_SoundCommandQueue_PSP
	                              ) &&
	                              g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) &&
	                              g_DelayedEventQueue_PSP.GetPendingPlayResultCount() == 1;
	ASoundVoiceHandle_PSP hDelayedVoice = ASoundMixer_PSP::INVALID_VOICE;
	const TBOOL bResultReady = bPlayDispatched &&
	                           g_DelayedEventQueue_PSP.Update( 0, g_SoundCommandQueue_PSP ) &&
	                           !g_DelayedEventQueue_PSP.IsPending( hPlayEvent ) &&
	                           g_DelayedEventQueue_PSP.GetPendingPlayResultCount() == 0 &&
	                           g_DelayedEventQueue_PSP.GetReadyPlayResultCount() == 1;
	const TBOOL bResultConsumed = bResultReady &&
	                              g_DelayedEventQueue_PSP.GetPlayResult( hPlayEvent, hDelayedVoice ) &&
	                              hDelayedVoice != ASoundMixer_PSP::INVALID_VOICE &&
	                              g_SoundMixer_PSP.IsVoiceActive( hDelayedVoice ) &&
	                              !g_DelayedEventQueue_PSP.GetPlayResult( hPlayEvent, hDelayedVoice ) &&
	                              g_SoundMixer_PSP.Stop( hDelayedVoice );

	const ASoundEvent_PSP oInvalidStreamEvent = {
		ASoundEventType_PSP_PlayAudio,
		"bootstrap_stream",
		ASoundMixer_PSP::INVALID_VOICE,
		0,
		0,
		TTRUE,
		ASoundMixer_PSP::DEFAULT_FREQUENCY / 2,
	};
	const ASoundEvent_PSP oStreamEvent = {
		ASoundEventType_PSP_PlayAudio,
		"bootstrap_stream",
		ASoundMixer_PSP::INVALID_VOICE,
		0,
		0,
		TTRUE,
	};
	const TBOOL bInvalidStreamRejected =
	    g_DelayedEventQueue_PSP.Schedule(
	        g_WaveBank_PSP,
	        oInvalidStreamEvent,
	        0
	    ) == ASoundDelayedEventQueue_PSP::INVALID_EVENT &&
	    g_DelayedEventQueue_PSP.GetRejectedEventCount() == 2;
	const ASoundScheduledEventHandle_PSP hStreamEvent =
	    g_DelayedEventQueue_PSP.Schedule(
	        g_WaveBank_PSP,
	        oStreamEvent,
	        ASoundManager_PSP::BUFFER_SAMPLE_FRAMES
	    );
	const TBOOL bStreamDispatched =
	    hStreamEvent != ASoundDelayedEventQueue_PSP::INVALID_EVENT &&
	    g_DelayedEventQueue_PSP.Update(
	        ASoundManager_PSP::BUFFER_SAMPLE_FRAMES,
	        g_SoundCommandQueue_PSP
	    ) &&
	    g_SoundCommandQueue_PSP.Process( g_SoundMixer_PSP ) &&
	    g_DelayedEventQueue_PSP.GetPendingPlayResultCount() == 1 &&
	    g_DelayedEventQueue_PSP.Update( 0, g_SoundCommandQueue_PSP );
	ASoundVoiceHandle_PSP hStreamVoice = ASoundMixer_PSP::INVALID_VOICE;
	const TBOOL bStreamResultConsumed = bStreamDispatched &&
	                                        g_DelayedEventQueue_PSP.GetPlayResult(
	                                            hStreamEvent,
	                                            hStreamVoice
	                                        ) &&
	                                        hStreamVoice != ASoundMixer_PSP::INVALID_VOICE &&
	                                        g_SoundMixer_PSP.IsVoiceActive( hStreamVoice ) &&
	                                        g_SoundMixer_PSP.Stop( hStreamVoice );
	return bMissingRejected && bCancelled && bResultConsumed &&
	       bInvalidStreamRejected && bStreamResultConsumed;
}

static TBOOL InitialiseProbeTexture( TBOOL a_bLoadExternal )
{
	g_bExternalTexture_PSP = a_bLoadExternal && LoadProbeTextureFile_PSP();
	if ( g_bExternalTexture_PSP )
		return TTRUE;

	for ( TUINT y = 0; y < 16; y++ )
	{
		for ( TUINT x = 0; x < 16; x++ )
		{
			const TBOOL bAlternate = ( ( x / 4 ) + ( y / 4 ) ) % 2 != 0;
			g_ProbeTextureData.aPixels[ y * 16 + x ] = bAlternate ? 0xfff0cf65 : 0xff5ac8fa;
		}
	}

	g_ProbeTexture.SetData( &g_ProbeTextureData, sizeof( g_ProbeTextureData ) );
	return g_ProbeTexture.Load();
}

static TBOOL InitialiseProbeMesh_PSP()
{
	if ( !g_ProbeVertices_PSP.Create( TUINT16( sizeof( Toshi::TRenderColourVertex_PSP ) ), 4 ) ||
	     !g_ProbeIndices_PSP.Create( 6 ) )
	{
		g_ProbeVertices_PSP.Destroy();
		g_ProbeIndices_PSP.Destroy();
		return TFALSE;
	}

	auto* pVertices = reinterpret_cast<Toshi::TRenderColourVertex_PSP*>( g_ProbeVertices_PSP.Lock() );
	auto* pIndices  = g_ProbeIndices_PSP.Lock();
	if ( pVertices == TNULL || pIndices == TNULL )
	{
		if ( pVertices != TNULL ) g_ProbeVertices_PSP.Unlock( 0 );
		if ( pIndices != TNULL ) g_ProbeIndices_PSP.Unlock( 0 );
		g_ProbeVertices_PSP.Destroy();
		g_ProbeIndices_PSP.Destroy();
		return TFALSE;
	}

	pVertices[ 0 ] = { 0xff303060, 152.0f, 48.0f, 0.0f };
	pVertices[ 1 ] = { 0xff603030, 328.0f, 48.0f, 0.0f };
	pVertices[ 2 ] = { 0xff306030, 328.0f, 224.0f, 0.0f };
	pVertices[ 3 ] = { 0xff606030, 152.0f, 224.0f, 0.0f };
	pIndices[ 0 ] = 0;
	pIndices[ 1 ] = 1;
	pIndices[ 2 ] = 2;
	pIndices[ 3 ] = 0;
	pIndices[ 4 ] = 2;
	pIndices[ 5 ] = 3;
	g_ProbeVertices_PSP.Unlock( 4 );
	g_ProbeIndices_PSP.Unlock( 6 );
	return TTRUE;
}

static TINT ExitCallback( TINT, TINT, void* )
{
	g_Running = TFALSE;
	return 0;
}

static TINT CallbackThread( SceSize, void* )
{
	const TINT iCallback = sceKernelCreateCallback( "OpenBarnyardExit", ExitCallback, TNULL );
	sceKernelRegisterExitCallback( iCallback );
	sceKernelSleepThreadCB();
	return 0;
}

static void SetupCallbacks()
{
	const TINT iThread = sceKernelCreateThread(
	    "OpenBarnyardCallbacks",
	    CallbackThread,
	    0x11,
	    0x1000,
	    PSP_THREAD_ATTR_USER,
	    TNULL
	);

	if ( iThread >= 0 )
		sceKernelStartThread( iThread, 0, TNULL );
}

int main()
{
	SetupCallbacks();
	Toshi::TInputInterface_PSP oInputInterface;
	Toshi::TScheduler          oScheduler;
	Toshi::THPTimer            oFrameTimer;
	if ( !g_RenderInterface.CreateDisplay() )
	{
		sceKernelExitGame();
		return 1;
	}

	const TBOOL bInputReady = oInputInterface.Initialise();
	const TBOOL bAudioDeviceReady = g_SoundManager_PSP.Create();
	const TBOOL bToshiReady     = RunToshiPortabilitySmokeTest();
	const TBOOL bFileReady      = Toshi::TFileManager::Create();
	const TBOOL bAudioBankReady = bAudioDeviceReady && bFileReady &&
	                              g_WaveBankLoader_PSP.Load(
	                                  g_WaveBank_PSP,
	                                  g_BootstrapWaves_PSP,
	                                  TARRAYSIZE( g_BootstrapWaves_PSP ),
	                                  &g_BootstrapStream_PSP,
	                                  1,
	                                  "local:bootstrap_audio.pbnk",
	                                  g_uiLoadedWaves_PSP,
	                                  g_uiLoadedStreams_PSP
	                              ) &&
	                              g_uiLoadedWaves_PSP == TARRAYSIZE( g_BootstrapWaves_PSP ) &&
	                              g_uiLoadedStreams_PSP == 1 &&
	                              g_WaveBank_PSP.GetCount() == 5 &&
	                              g_WaveBank_PSP.FindWave( "BOOTSTRAP" ) == &g_BootstrapWaves_PSP[ 0 ] &&
	                              g_WaveBank_PSP.FindWave( "UI_MENUCLICK" ) == &g_BootstrapWaves_PSP[ 1 ] &&
	                              g_WaveBank_PSP.FindWave( "UI_MENUBACK" ) == &g_BootstrapWaves_PSP[ 2 ] &&
	                              g_WaveBank_PSP.FindWave( "UI_MENUOK" ) == &g_BootstrapWaves_PSP[ 3 ] &&
	                              g_WaveBank_PSP.FindStream( "BOOTSTRAP_STREAM" ) == &g_BootstrapStream_PSP &&
	                              g_WaveBank_PSP.FindWave( "BOOTSTRAP_STREAM" ) == TNULL &&
	                              g_WaveBank_PSP.FindStream( "BOOTSTRAP" ) == TNULL;
	const TBOOL bGUIAudioReady = bAudioBankReady &&
	                             g_SoundManager_PSP.BindCueBank(
	                                 &g_WaveBank_PSP,
	                                 &g_SoundMixer_PSP,
	                                 &g_SoundCommandQueue_PSP,
	                                 &g_DelayedEventQueue_PSP
	                             ) &&
	                             g_GUIAudioContext_PSP.Create( &g_SoundManager_PSP ) &&
	                             TestGUIAudioContext_PSP();
	const TBOOL bManagedCueReady = bGUIAudioReady && TestManagedSoundCue_PSP();
	const TBOOL bCueExReady = bManagedCueReady && TestPlayCueEx_PSP();
	const TBOOL bCueControlsReady = bCueExReady && TestManagedCueControls_PSP();
	const TBOOL bDelayedCueControlsReady = bCueControlsReady && TestDelayedCueControls_PSP();
	const TBOOL bCueFrequencyReady = bDelayedCueControlsReady && TestManagedCueFrequency_PSP();
	const TBOOL bCuePositionReady = bCueFrequencyReady && TestManagedCuePosition_PSP();
	const TBOOL bPositionalCueReady = bCuePositionReady && TestPositionalCuePlayback_PSP();
	const TBOOL bCuePauseReady = bPositionalCueReady && TestManagedCuePause_PSP();
	const TBOOL bCueQueriesReady = bCuePauseReady && TestManagedCueQueries_PSP();
	const TBOOL bCueFadeReady = bCueQueriesReady && TestManagedCueFade_PSP();
	const TBOOL bGlobalVolumeReady = bCueFadeReady && TestGlobalCueVolume_PSP();
	const TBOOL bManagedStreamReady = bGlobalVolumeReady && TestManagedStreamCue_PSP();
	const TBOOL bResidentAudioReady = bManagedStreamReady &&
	                                  TestSoundCommandQueue_PSP() &&
	                                  TestDelayedSoundEvents_PSP() &&
	                                  TestBootstrapVoiceFrequency_PSP();
	const TBOOL bADPCMReady = bAudioBankReady && TestBootstrapCompressedStream_PSP();
	const TBOOL bAudioReady = bGUIAudioReady && bResidentAudioReady && bADPCMReady &&
	                          TestBootstrapVoiceControls_PSP();
	const TBOOL bTextureReady   = InitialiseProbeTexture( bFileReady );
	const TBOOL bFontTextureReady = bFileReady && LoadFontTextureFile_PSP();
	const TBOOL bFontReady      = bFontTextureReady && LoadFontFile_PSP();
	const TBOOL bLocaleReady    = bFileReady && LoadLocaleFile_PSP();
	const TBOOL bModelReady = bFileReady && LoadProbeModelFile_PSP() &&
	                          TestModelBounds_PSP( g_ProbeModel_PSP ) &&
	                          TestPreviewTransform_PSP( g_ProbeModel_PSP );
	const TBOOL bSkeletonReady  = bFileReady && TestProbeSkeletonFile_PSP();
	const TBOOL bSkeletonPoseReady = bSkeletonReady && TestProbeSkeletonPose_PSP();
	const TBOOL bAnimationReady = bSkeletonPoseReady && TestProbeAnimation_PSP();
	const TBOOL bLoopAnimationReady = bAnimationReady && TestLoopingAnimation_PSP();
	const TBOOL bSkinningReady = bLoopAnimationReady && TestProbeSkinning_PSP();
	const TBOOL bAnimationBudgetReady = bSkinningReady && TestAnimationBudgets_PSP();
	const TBOOL bPreviewReady = bAnimationBudgetReady && LoadPreviewModel_PSP();
	const TBOOL bMeshReady      = InitialiseProbeMesh_PSP();
	const TBOOL bTextureCached  = bTextureReady && g_ResourceCache_PSP.Register(
	    "probe.texture",
	    Toshi::TRENDERRESOURCETYPE_PSP_TEXTURE,
	    &g_ProbeTexture
	);
	const TBOOL bModelCached = bModelReady && g_ResourceCache_PSP.Register(
	    "probe.model",
	    Toshi::TRENDERRESOURCETYPE_PSP_MODEL,
	    &g_ProbeModel_PSP
	);
	const TBOOL bFontTextureCached = bFontTextureReady && g_ResourceCache_PSP.Register(
	    "bootstrap.font.atlas",
	    Toshi::TRENDERRESOURCETYPE_PSP_TEXTURE,
	    &g_FontTexture_PSP
	);
	const TBOOL bFontCached = bFontReady && g_ResourceCache_PSP.Register(
	    "bootstrap.font",
	    Toshi::TRENDERRESOURCETYPE_PSP_FONT,
	    &g_Font_PSP
	);
	auto* pCachedTexture = static_cast<Toshi::T2Texture*>( g_ResourceCache_PSP.Find(
	    "PROBE.TEXTURE",
	    Toshi::TRENDERRESOURCETYPE_PSP_TEXTURE
	) );
	auto* pCachedModel = static_cast<Toshi::TModel_PSP*>( g_ResourceCache_PSP.Find(
	    "PROBE.MODEL",
	    Toshi::TRENDERRESOURCETYPE_PSP_MODEL
	) );
	auto* pCachedFontTexture = static_cast<Toshi::T2Texture*>( g_ResourceCache_PSP.Find(
	    "BOOTSTRAP.FONT.ATLAS",
	    Toshi::TRENDERRESOURCETYPE_PSP_TEXTURE
	) );
	auto* pCachedFont = static_cast<Toshi::T2GUIFont_PSP*>( g_ResourceCache_PSP.Find(
	    "BOOTSTRAP.FONT",
	    Toshi::TRENDERRESOURCETYPE_PSP_FONT
	) );
	const TBOOL bCacheReady = bTextureCached && bModelCached && bFontTextureCached && bFontCached &&
	                          pCachedTexture == &g_ProbeTexture &&
	                          pCachedModel == &g_ProbeModel_PSP &&
	                          pCachedFontTexture == &g_FontTexture_PSP &&
	                          pCachedFont == &g_Font_PSP;
	AFrontendRenderContext_PSP oRenderContext = {
		&g_RenderInterface,
		&g_ProbeVertices_PSP,
		&g_ProbeIndices_PSP,
		bPreviewReady && g_bPreviewTexture_PSP ? &g_PreviewTexture_PSP : pCachedTexture,
		pCachedFont,
		&g_LocaleManager_PSP,
		pCachedModel,
		bPreviewReady ? &g_PreviewModel_PSP : ( bSkinningReady ? &g_ProbeSkinnedModel_PSP : TNULL ),
		bPreviewReady ? &g_PreviewSkinningBuffer_PSP : ( bSkinningReady ? &g_ProbeSkinningBuffer_PSP : TNULL ),
		bPreviewReady ? &g_PreviewSkeletonInstance_PSP : ( bSkinningReady ? &g_ProbeSkeletonInstance_PSP : TNULL ),
		bPreviewReady ? &g_PreviewAnimation_PSP : ( bSkinningReady ? &g_ProbeAnimation_PSP : TNULL ),
		&g_ProbeMaterial_PSP,
		bPreviewReady ? g_PreviewTransform_PSP : g_IdentityModelTransform_PSP,
		TFALSE,
		bMeshReady,
	};

	AApplication_PSP oApplication( &oInputInterface, &oScheduler, &oFrameTimer );
	const TBOOL bSchedulerReady = oApplication.Create(
	    &oRenderContext,
	    &g_SoundManager_PSP,
	    &g_SoundMixer_PSP,
	    &g_SoundCommandQueue_PSP,
	    &g_DelayedEventQueue_PSP,
	    &g_GUIAudioContext_PSP
	);
	const TBOOL bBootstrapReady = bInputReady && bSchedulerReady && bToshiReady &&
	                              bAudioReady && bTextureReady && bFontReady && bLocaleReady &&
	                              bMeshReady && bModelReady && bSkeletonReady &&
	                              bSkeletonPoseReady && bAnimationReady && bLoopAnimationReady && bSkinningReady &&
	                              bAnimationBudgetReady &&
	                              bCacheReady;
	oRenderContext.bBootstrapReady = bBootstrapReady;
	TDebug_FinalPrintf( "[PSP render] texture     %s\n", bTextureReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP render] source      %s\n", g_bExternalTexture_PSP ? "probe.ptex" : "embedded fallback" );
	TDebug_FinalPrintf( "[PSP render] layout      %s\n", g_ProbeTexture.IsSwizzled() ? "swizzled" : "linear" );
	TDebug_FinalPrintf( "[PSP render] font atlas  %s\n", bFontTextureReady ? "bootstrap_font.ptex" : "FAILED" );
	TDebug_FinalPrintf( "[PSP render] font data   %s\n", bFontReady ? "bootstrap_font.pfnt" : "FAILED" );
	TDebug_FinalPrintf( "[PSP frontend] locale    %s\n", bLocaleReady ? "bootstrap_locale.ploc" : "FAILED" );
	TDebug_FinalPrintf(
	    "[PSP sound] audio        %s (bootstrap_audio.pbnk, channel %d)\n",
	    bAudioReady ? "OK" : "FAILED",
	    g_SoundManager_PSP.GetChannel()
	);
	TDebug_FinalPrintf(
	    "[PSP sound] underruns    %u\n",
	    g_BootstrapStream_PSP.GetUnderrunCount()
	);
	TDebug_FinalPrintf( "[PSP sound] ADPCM        %s\n", bADPCMReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP frontend] GUI audio %s\n", bGUIAudioReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP sound] managed cues %s\n", bManagedCueReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP sound] PlayCueEx    %s\n", bCueExReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP sound] cue controls %s\n", bCueControlsReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP sound] async controls %s\n", bDelayedCueControlsReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP sound] cue frequency %s\n", bCueFrequencyReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP sound] cue position  %s\n", bCuePositionReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP sound] position play %s\n", bPositionalCueReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP sound] cue pause     %s\n", bCuePauseReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP sound] cue queries   %s\n", bCueQueriesReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP sound] cue fade      %s\n", bCueFadeReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP sound] global volume %s\n", bGlobalVolumeReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP sound] stream cues   %s\n", bManagedStreamReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP render] index mesh  %s\n", bMeshReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP render] model       %s\n", bModelReady ? "probe.pmes" : "FAILED" );
	TDebug_FinalPrintf( "[PSP animation] skeleton %s\n", bSkeletonReady ? "probe.pskl" : "FAILED" );
	TDebug_FinalPrintf( "[PSP animation] bind pose %s\n", bSkeletonPoseReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP animation] clip      %s\n", bAnimationReady ? "probe.panm" : "FAILED" );
	TDebug_FinalPrintf( "[PSP animation] loop      %s\n", bLoopAnimationReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP animation] skinning  %s\n", bSkinningReady ? "probe_skinned.pmes" : "FAILED" );
	TDebug_FinalPrintf( "[PSP animation] budgets   %s\n", bAnimationBudgetReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP animation] preview   %s\n", bPreviewReady ? "barnyard_preview" : "inactive" );
	TDebug_FinalPrintf(
	    "[PSP render] preview texture %s\n",
	    bPreviewReady && g_bPreviewTexture_PSP ? "barnyard_preview.ptex" : "probe fallback"
	);
	TDebug_FinalPrintf(
	    "[PSP render] preview fit %.6f @ %.6f,%.6f,%.6f\n",
	    bPreviewReady ? g_PreviewTransform_PSP.fScale : g_IdentityModelTransform_PSP.fScale,
	    bPreviewReady ? g_PreviewTransform_PSP.aCenter[ 0 ] : g_IdentityModelTransform_PSP.aCenter[ 0 ],
	    bPreviewReady ? g_PreviewTransform_PSP.aCenter[ 1 ] : g_IdentityModelTransform_PSP.aCenter[ 1 ],
	    bPreviewReady ? g_PreviewTransform_PSP.aCenter[ 2 ] : g_IdentityModelTransform_PSP.aCenter[ 2 ]
	);
	TDebug_FinalPrintf( "[PSP render] cache       %s (%u/%u)\n", bCacheReady ? "OK" : "FAILED", g_ResourceCache_PSP.GetCount(), g_ResourceCache_PSP.GetCapacity() );
	TDebug_FinalPrintf( "[PSP frontend] scheduler %s\n", bSchedulerReady ? "OK" : "FAILED" );
	const Toshi::TRenderMemoryStats_PSP oMemoryStats = Toshi::TRenderMemory_PSP::GetStats();
	const Toshi::TAnimationMemoryStats_PSP oAnimationStats = Toshi::TAnimationMemory_PSP::GetStats();
	TDebug_FinalPrintf(
	    "[PSP memory] main %u/%u peak %u, edram %u/%u\n",
	    TUINT( oMemoryStats.uiMainCurrent ),
	    TUINT( oMemoryStats.uiMainBudget ),
	    TUINT( oMemoryStats.uiMainPeak ),
	    TUINT( oMemoryStats.uiEDRAMCurrent ),
	    TUINT( oMemoryStats.uiEDRAMBudget )
	);
	TDebug_FinalPrintf(
	    "[PSP animation] samples %u/%u peak %u, instances %u/%u, skin %u/%u vertices %u/%u\n",
	    TUINT( oAnimationStats.uiSampleCurrent ),
	    TUINT( oAnimationStats.uiSampleBudget ),
	    TUINT( oAnimationStats.uiSamplePeak ),
	    oAnimationStats.uiInstanceCount,
	    oAnimationStats.uiInstanceBudget,
	    oAnimationStats.uiSkinnedModelCount,
	    oAnimationStats.uiSkinnedModelBudget,
	    oAnimationStats.uiAnimatedVertices,
	    oAnimationStats.uiAnimatedVertexBudget
	);
	TDebug_FinalPrintf(
	    "[PSP animation] pose %u us max %u, skin %u us max %u\n",
	    oAnimationStats.uiLastPoseMicroseconds,
	    oAnimationStats.uiMaxPoseMicroseconds,
	    oAnimationStats.uiLastSkinMicroseconds,
	    oAnimationStats.uiMaxSkinMicroseconds
	);
	APortReportData_PSP oPortReport = {
		"RUNNING",
		bBootstrapReady,
		bInputReady,
		bToshiReady,
		bFileReady,
		bAudioReady,
		bADPCMReady,
		bGUIAudioReady,
		bManagedCueReady,
		bCueExReady,
		bCueControlsReady,
		bDelayedCueControlsReady,
		bCueFrequencyReady,
		bCuePositionReady,
		bPositionalCueReady,
		bCuePauseReady,
		bCueQueriesReady,
		bCueFadeReady,
		bGlobalVolumeReady,
		bManagedStreamReady,
		g_BootstrapStream_PSP.GetUnderrunCount(),
		bTextureReady,
		bFontReady,
		bLocaleReady,
		bMeshReady,
		bModelReady,
		bSkeletonReady,
		bSkeletonPoseReady,
		bAnimationReady,
		bSkinningReady,
		bAnimationBudgetReady,
		bPreviewReady,
		bPreviewReady && g_bPreviewTexture_PSP,
		bPreviewReady ? g_PreviewTransform_PSP.fScale : g_IdentityModelTransform_PSP.fScale,
		{
		    bPreviewReady ? g_PreviewTransform_PSP.aCenter[ 0 ] : g_IdentityModelTransform_PSP.aCenter[ 0 ],
		    bPreviewReady ? g_PreviewTransform_PSP.aCenter[ 1 ] : g_IdentityModelTransform_PSP.aCenter[ 1 ],
		    bPreviewReady ? g_PreviewTransform_PSP.aCenter[ 2 ] : g_IdentityModelTransform_PSP.aCenter[ 2 ],
		},
		bCacheReady,
		bSchedulerReady,
		0,
		oMemoryStats,
		oAnimationStats,
	};
	if ( bFileReady )
		APortReport_PSP::Write( oPortReport );

	while ( g_Running && oApplication.Update() ) {}

	oPortReport.pStage = "CLEAN_EXIT";
	oPortReport.uiFrameCount = oApplication.GetFrontendState().uiFrameCount;
	oPortReport.uiStreamUnderruns = g_BootstrapStream_PSP.GetUnderrunCount();
	oPortReport.oMemory = Toshi::TRenderMemory_PSP::GetStats();
	oPortReport.oAnimation = Toshi::TAnimationMemory_PSP::GetStats();
	if ( bFileReady )
		APortReport_PSP::Write( oPortReport );

	oApplication.Destroy();
	oInputInterface.Deinitialise();
	g_GUIAudioContext_PSP.Destroy();
	g_SoundManager_PSP.UnbindCueBank();
	g_SoundManager_PSP.Destroy();
	g_SoundCommandQueue_PSP.Clear();
	g_DelayedEventQueue_PSP.Clear();
	g_SoundMixer_PSP.StopAll();
	g_WaveBankLoader_PSP.Unload(
	    g_WaveBank_PSP,
	    g_BootstrapWaves_PSP,
	    g_uiLoadedWaves_PSP,
	    &g_BootstrapStream_PSP,
	    g_uiLoadedStreams_PSP
	);
	g_ResourceCache_PSP.Clear();
	UnloadPreviewModel_PSP();
	g_ProbeSkinningBuffer_PSP.Destroy();
	g_ProbeSkeletonInstance_PSP.Destroy();
	g_ProbeSkinnedModel_PSP.Unload();
	g_ProbeModel_PSP.Unload();
	g_ProbeAnimation_PSP.Unload();
	g_ProbeSkeleton_PSP.Unload();
	g_Font_PSP.Destroy();
	g_LocaleManager_PSP.Destroy();
	g_FontTexture_PSP.Unload();
	g_ProbeTexture.Unload();
	g_ProbeIndices_PSP.Destroy();
	g_ProbeVertices_PSP.Destroy();
	if ( bFileReady )
		Toshi::TFileManager::Destroy();
	g_RenderInterface.DestroyDisplay();
	sceKernelExitGame();
	return 0;
}
