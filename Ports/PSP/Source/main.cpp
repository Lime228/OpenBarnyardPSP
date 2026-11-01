#include <pspkernel.h>

#include <Toshi/Typedefs.h>
#include <Toshi/TDebug.h>
#include <Toshi/THPTimer.h>
#include <Toshi/TScheduler.h>
#include <File/TFile.h>
#include <Platform/PSP/Input/TInputInterface_PSP.h>
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
static Toshi::TRenderResourceCache_PSP g_ResourceCache_PSP;
static Toshi::TRenderInterface_PSP g_RenderInterface;
static Toshi::TVertexPoolResource_PSP g_ProbeVertices_PSP;
static Toshi::TIndexPoolResource_PSP  g_ProbeIndices_PSP;
static Toshi::TModel_PSP                g_ProbeModel_PSP;
static TBOOL                            g_bExternalTexture_PSP = TFALSE;
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
	                                 &g_SoundCommandQueue_PSP
	                             ) &&
	                             g_GUIAudioContext_PSP.Create( &g_SoundManager_PSP ) &&
	                             TestGUIAudioContext_PSP();
	const TBOOL bManagedCueReady = bGUIAudioReady && TestManagedSoundCue_PSP();
	const TBOOL bResidentAudioReady = bManagedCueReady &&
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
	const TBOOL bModelReady     = bFileReady && LoadProbeModelFile_PSP();
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
		pCachedTexture,
		pCachedFont,
		&g_LocaleManager_PSP,
		pCachedModel,
		&g_ProbeMaterial_PSP,
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
	                              bMeshReady && bModelReady && bCacheReady;
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
	TDebug_FinalPrintf( "[PSP render] index mesh  %s\n", bMeshReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP render] model       %s\n", bModelReady ? "probe.pmes" : "FAILED" );
	TDebug_FinalPrintf( "[PSP render] cache       %s (%u/%u)\n", bCacheReady ? "OK" : "FAILED", g_ResourceCache_PSP.GetCount(), g_ResourceCache_PSP.GetCapacity() );
	TDebug_FinalPrintf( "[PSP frontend] scheduler %s\n", bSchedulerReady ? "OK" : "FAILED" );
	const Toshi::TRenderMemoryStats_PSP oMemoryStats = Toshi::TRenderMemory_PSP::GetStats();
	TDebug_FinalPrintf(
	    "[PSP memory] main %u/%u peak %u, edram %u/%u\n",
	    TUINT( oMemoryStats.uiMainCurrent ),
	    TUINT( oMemoryStats.uiMainBudget ),
	    TUINT( oMemoryStats.uiMainPeak ),
	    TUINT( oMemoryStats.uiEDRAMCurrent ),
	    TUINT( oMemoryStats.uiEDRAMBudget )
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
		g_BootstrapStream_PSP.GetUnderrunCount(),
		bTextureReady,
		bFontReady,
		bLocaleReady,
		bMeshReady,
		bModelReady,
		bCacheReady,
		bSchedulerReady,
		0,
		oMemoryStats,
	};
	if ( bFileReady )
		APortReport_PSP::Write( oPortReport );

	while ( g_Running && oApplication.Update() ) {}

	oPortReport.pStage = "CLEAN_EXIT";
	oPortReport.uiFrameCount = oApplication.GetFrontendState().uiFrameCount;
	oPortReport.uiStreamUnderruns = g_BootstrapStream_PSP.GetUnderrunCount();
	oPortReport.oMemory = Toshi::TRenderMemory_PSP::GetStats();
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
	g_ProbeModel_PSP.Unload();
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
