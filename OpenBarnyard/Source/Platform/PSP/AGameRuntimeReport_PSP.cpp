#include "pch.h"

#include "AGameRuntimeReport_PSP.h"

#include <Core/TMemory.h>
#include <Math/TMath.h>
#include <Platform/PSP/TRenderMemory_PSP.h>
#include <Platform/PSP/Input/TInputInterface_PSP.h>
#include <Toshi/TStringManager.h>

#include <pspiofilemgr.h>

#include <cstdio>

TOSHI_NAMESPACE_USING

static AGameRuntimeReport_PSP s_GameRuntimeReport_PSP = {};

AGameRuntimeReport_PSP& AGetGameRuntimeReport_PSP()
{
	return s_GameRuntimeReport_PSP;
}

static void ACopyGameRuntimeString_PSP( TCHAR* a_szDestination, TSIZE a_uiCapacity, const TCHAR* a_szSource )
{
	if ( a_uiCapacity == 0 )
		return;

	TStringManager::String8Copy(
		a_szDestination,
		a_szSource != TNULL ? a_szSource : "",
		a_uiCapacity
	);
	a_szDestination[ a_uiCapacity - 1 ] = '\0';
}

void ASetGameRuntimePhase_PSP( const TCHAR* a_szPhase )
{
	ACopyGameRuntimeString_PSP(
		s_GameRuntimeReport_PSP.szPhase,
		TARRAYSIZE( s_GameRuntimeReport_PSP.szPhase ),
		a_szPhase
	);
	AWriteGameRuntimeReport_PSP();
}

void ASetGameRuntimePicture_PSP( const TCHAR* a_szPicture )
{
	ACopyGameRuntimeString_PSP(
		s_GameRuntimeReport_PSP.szPicture,
		TARRAYSIZE( s_GameRuntimeReport_PSP.szPicture ),
		a_szPicture
	);
}

void ASetGameRuntimeFailedPath_PSP( const TCHAR* a_szPath )
{
	ACopyGameRuntimeString_PSP(
		s_GameRuntimeReport_PSP.szFailedPath,
		TARRAYSIZE( s_GameRuntimeReport_PSP.szFailedPath ),
		a_szPath
	);
}

void AUpdateGameRuntimeMemory_PSP()
{
	if ( g_pMemory != TNULL )
	{
		TMemory::MemInfo oMemoryInfo;
		TMemory::GetMemInfo( oMemoryInfo, g_pMemory->GetGlobalBlock() );
		s_GameRuntimeReport_PSP.uiHeapFree        = oMemoryInfo.m_uiTotalFree;
		s_GameRuntimeReport_PSP.uiHeapLargestHole = oMemoryInfo.m_uiLargestHole;
		s_GameRuntimeReport_PSP.uiHeapUsed        = oMemoryInfo.m_uiTotalUsed;
	}

	const TRenderMemoryStats_PSP oRenderMemory = TRenderMemory_PSP::GetStats();
	s_GameRuntimeReport_PSP.uiRenderMainCurrent = oRenderMemory.uiMainCurrent;
	s_GameRuntimeReport_PSP.uiRenderMainPeak    = oRenderMemory.uiMainPeak;
	s_GameRuntimeReport_PSP.uiRenderMainBudget  = oRenderMemory.uiMainBudget;
}

void AWriteGameRuntimeReport_PSP()
{
	if ( TInputInterface::IsSingletonCreated() )
	{
		TInputInterface_PSP* pInputInterface = TSTATICCAST( TInputInterface_PSP, TInputInterface::GetSingleton() );
		const TInputDeviceController_PSP& rController = pInputInterface->GetController();
		s_GameRuntimeReport_PSP.uiControllerButtons = rController.GetCurrentButtons_PSP();
		s_GameRuntimeReport_PSP.uiControllerObservedButtons = rController.GetObservedButtons_PSP();
		s_GameRuntimeReport_PSP.uiControllerNonZeroButtonSamples = rController.GetNonZeroButtonSamples_PSP();
	}

	static TCHAR s_aReportBuffer_PSP[ 4096 ];
	const TINT iLength = std::snprintf(
		s_aReportBuffer_PSP,
		TARRAYSIZE( s_aReportBuffer_PSP ),
		"version=26\n"
		"phase=%s\n"
		"engine=%d\n"
		"application=%d\n"
		"root=%d\n"
		"gui2=%d\n"
		"gui_system=%d\n"
		"controls=%d\n"
		"picture=%s\n"
		"picture_tiles=%d\n"
		"failed_path=%s\n"
		"heap_free=%u\n"
		"heap_largest_hole=%u\n"
		"heap_used=%u\n"
		"render_main_current=%u\n"
		"render_main_peak=%u\n"
		"render_main_budget=%u\n"
		"renderer_frames=%u\n"
		"frontend_terrain_loaded=%d\n"
		"frontend_terrain_meshes=%u\n"
		"frontend_terrain_draws=%u\n"
		"frontend_terrain_scale=%.6f\n"
		"frontend_terrain_pack_loaded=%d\n"
		"frontend_terrain_pack_models=%u\n"
		"frontend_terrain_pack_textures=%u\n"
		"frontend_terrain_pack_draws=%u\n"
		"frontend_grass_loaded=%d\n"
		"frontend_grass_models=%u\n"
		"frontend_grass_textures=%u\n"
		"frontend_grass_draws=%u\n"
		"frontend_glow_loaded=%d\n"
		"frontend_glow_models=%u\n"
		"frontend_glow_textures=%u\n"
		"frontend_glow_draws=%u\n"
		"frontend_barnnorth_loaded=%d\n"
		"frontend_barnnorth_models=%u\n"
		"frontend_barnnorth_textures=%u\n"
		"frontend_barnnorth_draws=%u\n"
		"frontend_bgbarneast_loaded=%d\n"
		"frontend_bgbarneast_models=%u\n"
		"frontend_bgbarneast_textures=%u\n"
		"frontend_bgbarneast_draws=%u\n"
		"frontend_bgbarnwest_loaded=%d\n"
		"frontend_bgbarnwest_models=%u\n"
		"frontend_bgbarnwest_textures=%u\n"
		"frontend_bgbarnwest_draws=%u\n"
		"frontend_hillbase_loaded=%d\n"
		"frontend_hillbase_models=%u\n"
		"frontend_hillbase_textures=%u\n"
		"frontend_hillbase_draws=%u\n"
		"frontend_hilltop_loaded=%d\n"
		"frontend_hilltop_models=%u\n"
		"frontend_hilltop_textures=%u\n"
		"frontend_hilltop_draws=%u\n"
		"frontend_meadowsouth_loaded=%d\n"
		"frontend_meadowsouth_models=%u\n"
		"frontend_meadowsouth_textures=%u\n"
		"frontend_meadowsouth_draws=%u\n"
		"frontend_instances_loaded=%d\n"
		"frontend_instance_packs=%u\n"
		"frontend_instances=%u\n"
		"frontend_instance_models=%u\n"
		"frontend_instance_textures=%u\n"
		"frontend_instance_draws=%u\n"
		"frontend_hour=%.6f\n"
		"frontend_barnsign_loaded=%d\n"
		"frontend_barnsign_meshes=%u\n"
		"frontend_barnsign_draws=%u\n"
		"frontend_camera_loaded=%d\n"
		"frontend_camera_frames=%u\n"
		"frontend_camera_sample_rate=%u\n"
		"frontend_camera_position=%.6f,%.6f,%.6f\n"
		"frontend_camera_direction=%.6f,%.6f,%.6f\n"
		"frontend_camera_up=%.6f,%.6f,%.6f\n"
		"picture_render_frames=%u\n"
		"picture_draws=%u\n"
		"gui_colour_draws=%u\n"
		"gui_quad_draws=%u\n"
		"gui_rotated_quad_draws=%u\n"
		"last_gui_colour=%08x\n"
		"controller_buttons=%08x\n"
		"controller_observed_buttons=%08x\n"
		"controller_nonzero_button_samples=%u\n"
		"menu_state=%d\n"
		"menu_focus=%d\n"
		"menu_last_pressed_doodad=%d\n"
		"menu_input_down_count=%u\n"
		"menu_last_command=%d\n"
		"menu_last_event_type=%d\n"
		"menu_last_doodad=%d\n"
		"menu_last_action_command=%d\n"
		"menu_last_action_event_type=%d\n"
		"menu_last_action_doodad=%d\n"
		"menu_action_command_count=%u\n"
		"menu_command_up_count=%u\n"
		"menu_command_down_count=%u\n"
		"menu_command_ok_count=%u\n"
		"menu_activated_button=%d\n"
		"menu_activation_count=%u\n"
		"menu_close_count=%u\n"
		"new_game_requested_slot=%d\n"
		"option_effects=%d\n"
		"option_music=%d\n"
		"option_speech=%d\n"
		"option_inverted=%d\n"
		"option_sensitivity=%.6f\n"
		"menu_audio_ready=%d\n"
		"menu_audio_last_cue=%d\n"
		"menu_audio_cues=%u\n"
		"menu_audio_vo_cues=%u\n"
		"menu_audio_non_silent_blocks=%u\n"
		"menu_music_ready=%d\n"
		"menu_music_position_ms=%d\n"
		"menu_music_loops=%u\n"
		"menu_music_underruns=%u\n"
		"menu_music_gain_per_mille=%u\n",
		s_GameRuntimeReport_PSP.szPhase,
		s_GameRuntimeReport_PSP.bEngineCreated,
		s_GameRuntimeReport_PSP.bApplicationCreated,
		s_GameRuntimeReport_PSP.bRootCreated,
		s_GameRuntimeReport_PSP.bGUI2Created,
		s_GameRuntimeReport_PSP.bGUISystemCreated,
		s_GameRuntimeReport_PSP.bControlsLoaded,
		s_GameRuntimeReport_PSP.szPicture,
		s_GameRuntimeReport_PSP.iPictureTiles,
		s_GameRuntimeReport_PSP.szFailedPath,
		TUINT( s_GameRuntimeReport_PSP.uiHeapFree ),
		TUINT( s_GameRuntimeReport_PSP.uiHeapLargestHole ),
		TUINT( s_GameRuntimeReport_PSP.uiHeapUsed ),
		TUINT( s_GameRuntimeReport_PSP.uiRenderMainCurrent ),
		TUINT( s_GameRuntimeReport_PSP.uiRenderMainPeak ),
		TUINT( s_GameRuntimeReport_PSP.uiRenderMainBudget ),
		s_GameRuntimeReport_PSP.uiRendererFrames,
		s_GameRuntimeReport_PSP.bFrontendTerrainLoaded,
		s_GameRuntimeReport_PSP.uiFrontendTerrainMeshes,
		s_GameRuntimeReport_PSP.uiFrontendTerrainDraws,
		s_GameRuntimeReport_PSP.fFrontendTerrainScale,
		s_GameRuntimeReport_PSP.bFrontendTerrainPackLoaded,
		s_GameRuntimeReport_PSP.uiFrontendTerrainPackModels,
		s_GameRuntimeReport_PSP.uiFrontendTerrainPackTextures,
		s_GameRuntimeReport_PSP.uiFrontendTerrainPackDraws,
		s_GameRuntimeReport_PSP.bFrontendGrassLoaded,
		s_GameRuntimeReport_PSP.uiFrontendGrassModels,
		s_GameRuntimeReport_PSP.uiFrontendGrassTextures,
		s_GameRuntimeReport_PSP.uiFrontendGrassDraws,
		s_GameRuntimeReport_PSP.bFrontendGlowLoaded,
		s_GameRuntimeReport_PSP.uiFrontendGlowModels,
		s_GameRuntimeReport_PSP.uiFrontendGlowTextures,
		s_GameRuntimeReport_PSP.uiFrontendGlowDraws,
		s_GameRuntimeReport_PSP.bFrontendBarnnorthLoaded,
		s_GameRuntimeReport_PSP.uiFrontendBarnnorthModels,
		s_GameRuntimeReport_PSP.uiFrontendBarnnorthTextures,
		s_GameRuntimeReport_PSP.uiFrontendBarnnorthDraws,
		s_GameRuntimeReport_PSP.bFrontendBGbarneastLoaded,
		s_GameRuntimeReport_PSP.uiFrontendBGbarneastModels,
		s_GameRuntimeReport_PSP.uiFrontendBGbarneastTextures,
		s_GameRuntimeReport_PSP.uiFrontendBGbarneastDraws,
		s_GameRuntimeReport_PSP.bFrontendBGbarnwestLoaded,
		s_GameRuntimeReport_PSP.uiFrontendBGbarnwestModels,
		s_GameRuntimeReport_PSP.uiFrontendBGbarnwestTextures,
		s_GameRuntimeReport_PSP.uiFrontendBGbarnwestDraws,
		s_GameRuntimeReport_PSP.bFrontendHillbaseLoaded,
		s_GameRuntimeReport_PSP.uiFrontendHillbaseModels,
		s_GameRuntimeReport_PSP.uiFrontendHillbaseTextures,
		s_GameRuntimeReport_PSP.uiFrontendHillbaseDraws,
		s_GameRuntimeReport_PSP.bFrontendHilltopLoaded,
		s_GameRuntimeReport_PSP.uiFrontendHilltopModels,
		s_GameRuntimeReport_PSP.uiFrontendHilltopTextures,
		s_GameRuntimeReport_PSP.uiFrontendHilltopDraws,
		s_GameRuntimeReport_PSP.bFrontendMeadowsouthLoaded,
		s_GameRuntimeReport_PSP.uiFrontendMeadowsouthModels,
		s_GameRuntimeReport_PSP.uiFrontendMeadowsouthTextures,
		s_GameRuntimeReport_PSP.uiFrontendMeadowsouthDraws,
		s_GameRuntimeReport_PSP.bFrontendInstancesLoaded,
		s_GameRuntimeReport_PSP.uiFrontendInstancePacks,
		s_GameRuntimeReport_PSP.uiFrontendInstances,
		s_GameRuntimeReport_PSP.uiFrontendInstanceModels,
		s_GameRuntimeReport_PSP.uiFrontendInstanceTextures,
		s_GameRuntimeReport_PSP.uiFrontendInstanceDraws,
		s_GameRuntimeReport_PSP.fFrontendHour,
		s_GameRuntimeReport_PSP.bFrontendBarnSignLoaded,
		s_GameRuntimeReport_PSP.uiFrontendBarnSignMeshes,
		s_GameRuntimeReport_PSP.uiFrontendBarnSignDraws,
		s_GameRuntimeReport_PSP.bFrontendCameraLoaded,
		s_GameRuntimeReport_PSP.uiFrontendCameraFrames,
		s_GameRuntimeReport_PSP.uiFrontendCameraSampleRate,
		s_GameRuntimeReport_PSP.fFrontendCameraX,
		s_GameRuntimeReport_PSP.fFrontendCameraY,
		s_GameRuntimeReport_PSP.fFrontendCameraZ,
		s_GameRuntimeReport_PSP.fFrontendCameraDirectionX,
		s_GameRuntimeReport_PSP.fFrontendCameraDirectionY,
		s_GameRuntimeReport_PSP.fFrontendCameraDirectionZ,
		s_GameRuntimeReport_PSP.fFrontendCameraUpX,
		s_GameRuntimeReport_PSP.fFrontendCameraUpY,
		s_GameRuntimeReport_PSP.fFrontendCameraUpZ,
		s_GameRuntimeReport_PSP.uiPictureRenderFrames,
		s_GameRuntimeReport_PSP.uiPictureDraws,
		s_GameRuntimeReport_PSP.uiGUIColourDraws,
		s_GameRuntimeReport_PSP.uiGUIQuadDraws,
		s_GameRuntimeReport_PSP.uiGUIRotatedQuadDraws,
		s_GameRuntimeReport_PSP.uiLastGUIColour,
		s_GameRuntimeReport_PSP.uiControllerButtons,
		s_GameRuntimeReport_PSP.uiControllerObservedButtons,
		s_GameRuntimeReport_PSP.uiControllerNonZeroButtonSamples,
		s_GameRuntimeReport_PSP.iMenuState,
		s_GameRuntimeReport_PSP.iMenuFocusID,
		s_GameRuntimeReport_PSP.iMenuLastPressedDoodad,
		s_GameRuntimeReport_PSP.uiMenuInputDownCount,
		s_GameRuntimeReport_PSP.iMenuLastCommand,
		s_GameRuntimeReport_PSP.iMenuLastEventType,
		s_GameRuntimeReport_PSP.iMenuLastDoodad,
		s_GameRuntimeReport_PSP.iMenuLastActionCommand,
		s_GameRuntimeReport_PSP.iMenuLastActionEventType,
		s_GameRuntimeReport_PSP.iMenuLastActionDoodad,
		s_GameRuntimeReport_PSP.uiMenuActionCommandCount,
		s_GameRuntimeReport_PSP.uiMenuCommandUpCount,
		s_GameRuntimeReport_PSP.uiMenuCommandDownCount,
		s_GameRuntimeReport_PSP.uiMenuCommandOKCount,
		s_GameRuntimeReport_PSP.iMenuActivatedButtonID,
		s_GameRuntimeReport_PSP.uiMenuActivationCount,
		s_GameRuntimeReport_PSP.uiMenuCloseCount,
		s_GameRuntimeReport_PSP.iNewGameRequestedSlot,
		s_GameRuntimeReport_PSP.iOptionEffects,
		s_GameRuntimeReport_PSP.iOptionMusic,
		s_GameRuntimeReport_PSP.iOptionSpeech,
		s_GameRuntimeReport_PSP.bOptionInverted,
		s_GameRuntimeReport_PSP.fOptionSensitivity,
		s_GameRuntimeReport_PSP.bMenuAudioReady,
		s_GameRuntimeReport_PSP.iMenuAudioLastCue,
		s_GameRuntimeReport_PSP.uiMenuAudioCueCount,
		s_GameRuntimeReport_PSP.uiMenuAudioVOCueCount,
		s_GameRuntimeReport_PSP.uiMenuAudioNonSilentBlocks,
		s_GameRuntimeReport_PSP.bMenuMusicReady,
		s_GameRuntimeReport_PSP.iMenuMusicPositionMs,
		s_GameRuntimeReport_PSP.uiMenuMusicLoops,
		s_GameRuntimeReport_PSP.uiMenuMusicUnderruns,
		s_GameRuntimeReport_PSP.uiMenuMusicGain
	);

	if ( iLength <= 0 )
		return;

	const TINT iHandle = sceIoOpen(
		"openbarnyard_psp_game.log",
		PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC,
		0777
	);
	if ( iHandle < 0 )
		return;

	const TSIZE uiLength = TMath::Min<TSIZE>( TSIZE( iLength ), TARRAYSIZE( s_aReportBuffer_PSP ) - 1 );
	sceIoWrite( iHandle, s_aReportBuffer_PSP, uiLength );
	sceIoClose( iHandle );
}
