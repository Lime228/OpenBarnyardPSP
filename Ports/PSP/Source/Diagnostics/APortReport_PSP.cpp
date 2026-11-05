#include "APortReport_PSP.h"

#include <Toshi/TDebug.h>
#include <File/TFile.h>

static const TCHAR* GetReportStatus_PSP( TBOOL a_bReady )
{
	return a_bReady ? "OK" : "FAILED";
}

TBOOL APortReport_PSP::Write( const APortReportData_PSP& a_rData )
{
	Toshi::TFile* pFile = Toshi::TFile::Create(
	    "local:openbarnyard_psp.log",
	    Toshi::TFILEMODE_WRITE | Toshi::TFILEMODE_CREATENEW | Toshi::TFILEMODE_NOBUFFER
	);
	if ( pFile == TNULL )
		return TFALSE;

	pFile->CPrintf( "OpenBarnyard PSP port report v6\n" );
	pFile->CPrintf( "stage=%s\n", a_rData.pStage != TNULL ? a_rData.pStage : "UNKNOWN" );
	pFile->CPrintf( "bootstrap=%s\n", GetReportStatus_PSP( a_rData.bBootstrapReady ) );
	pFile->CPrintf( "input=%s\n", GetReportStatus_PSP( a_rData.bInputReady ) );
	pFile->CPrintf( "toshi=%s\n", GetReportStatus_PSP( a_rData.bToshiReady ) );
	pFile->CPrintf( "file=%s\n", GetReportStatus_PSP( a_rData.bFileReady ) );
	pFile->CPrintf( "audio=%s\n", GetReportStatus_PSP( a_rData.bAudioReady ) );
	pFile->CPrintf( "adpcm=%s\n", GetReportStatus_PSP( a_rData.bADPCMReady ) );
	pFile->CPrintf( "gui_audio=%s\n", GetReportStatus_PSP( a_rData.bGUIAudioReady ) );
	pFile->CPrintf( "managed_cues=%s\n", GetReportStatus_PSP( a_rData.bManagedCueReady ) );
	pFile->CPrintf( "cue_ex=%s\n", GetReportStatus_PSP( a_rData.bCueExReady ) );
	pFile->CPrintf( "cue_controls=%s\n", GetReportStatus_PSP( a_rData.bCueControlsReady ) );
	pFile->CPrintf( "async_controls=%s\n", GetReportStatus_PSP( a_rData.bDelayedCueControlsReady ) );
	pFile->CPrintf( "cue_frequency=%s\n", GetReportStatus_PSP( a_rData.bCueFrequencyReady ) );
	pFile->CPrintf( "cue_position=%s\n", GetReportStatus_PSP( a_rData.bCuePositionReady ) );
	pFile->CPrintf( "position_play=%s\n", GetReportStatus_PSP( a_rData.bPositionalCueReady ) );
	pFile->CPrintf( "cue_pause=%s\n", GetReportStatus_PSP( a_rData.bCuePauseReady ) );
	pFile->CPrintf( "cue_queries=%s\n", GetReportStatus_PSP( a_rData.bCueQueriesReady ) );
	pFile->CPrintf( "cue_fade=%s\n", GetReportStatus_PSP( a_rData.bCueFadeReady ) );
	pFile->CPrintf( "global_volume=%s\n", GetReportStatus_PSP( a_rData.bGlobalVolumeReady ) );
	pFile->CPrintf( "stream_cues=%s\n", GetReportStatus_PSP( a_rData.bManagedStreamReady ) );
	pFile->CPrintf( "stream_underruns=%u\n", a_rData.uiStreamUnderruns );
	pFile->CPrintf( "texture=%s\n", GetReportStatus_PSP( a_rData.bTextureReady ) );
	pFile->CPrintf( "font=%s\n", GetReportStatus_PSP( a_rData.bFontReady ) );
	pFile->CPrintf( "locale=%s\n", GetReportStatus_PSP( a_rData.bLocaleReady ) );
	pFile->CPrintf( "mesh=%s\n", GetReportStatus_PSP( a_rData.bMeshReady ) );
	pFile->CPrintf( "model=%s\n", GetReportStatus_PSP( a_rData.bModelReady ) );
	pFile->CPrintf( "world=%s\n", GetReportStatus_PSP( a_rData.bWorldReady ) );
	pFile->CPrintf( "world_instances=%u\n", a_rData.uiWorldInstances );
	pFile->CPrintf( "world_visible=%u\n", a_rData.uiWorldVisibleInstances );
	pFile->CPrintf( "world_resources=%u\n", a_rData.uiWorldResources );
	pFile->CPrintf( "world_cells=%u\n", a_rData.uiWorldCells );
	pFile->CPrintf( "skeleton=%s\n", GetReportStatus_PSP( a_rData.bSkeletonReady ) );
	pFile->CPrintf( "skeleton_pose=%s\n", GetReportStatus_PSP( a_rData.bSkeletonPoseReady ) );
	pFile->CPrintf( "animation=%s\n", GetReportStatus_PSP( a_rData.bAnimationReady ) );
	pFile->CPrintf( "skinning=%s\n", GetReportStatus_PSP( a_rData.bSkinningReady ) );
	pFile->CPrintf( "animation_budget=%s\n", GetReportStatus_PSP( a_rData.bAnimationBudgetReady ) );
	pFile->CPrintf( "preview=%s\n", a_rData.bPreviewReady ? "ACTIVE" : "INACTIVE" );
	pFile->CPrintf(
	    "preview_stage=%s\n",
	    a_rData.pPreviewLoadStage != TNULL ? a_rData.pPreviewLoadStage : "unknown"
	);
	pFile->CPrintf(
	    "preview_texture=%s\n",
	    a_rData.bPreviewReady && a_rData.bPreviewTextureReady ? "barnyard_preview.ptex" : "probe.ptex"
	);
	pFile->CPrintf( "preview_scale=%.6f\n", a_rData.fPreviewScale );
	pFile->CPrintf(
	    "preview_center=%.6f,%.6f,%.6f\n",
	    a_rData.aPreviewCenter[ 0 ],
	    a_rData.aPreviewCenter[ 1 ],
	    a_rData.aPreviewCenter[ 2 ]
	);
	pFile->CPrintf( "cache=%s\n", GetReportStatus_PSP( a_rData.bCacheReady ) );
	pFile->CPrintf( "scheduler=%s\n", GetReportStatus_PSP( a_rData.bSchedulerReady ) );
	pFile->CPrintf( "frames=%u\n", a_rData.uiFrameCount );
	pFile->CPrintf( "main_current=%u\n", TUINT( a_rData.oMemory.uiMainCurrent ) );
	pFile->CPrintf( "main_peak=%u\n", TUINT( a_rData.oMemory.uiMainPeak ) );
	pFile->CPrintf( "main_budget=%u\n", TUINT( a_rData.oMemory.uiMainBudget ) );
	pFile->CPrintf( "edram_current=%u\n", TUINT( a_rData.oMemory.uiEDRAMCurrent ) );
	pFile->CPrintf( "edram_budget=%u\n", TUINT( a_rData.oMemory.uiEDRAMBudget ) );
	pFile->CPrintf( "animation_sample_current=%u\n", TUINT( a_rData.oAnimation.uiSampleCurrent ) );
	pFile->CPrintf( "animation_sample_peak=%u\n", TUINT( a_rData.oAnimation.uiSamplePeak ) );
	pFile->CPrintf( "animation_sample_budget=%u\n", TUINT( a_rData.oAnimation.uiSampleBudget ) );
	pFile->CPrintf( "animation_instances=%u\n", a_rData.oAnimation.uiInstanceCount );
	pFile->CPrintf( "animation_instance_budget=%u\n", a_rData.oAnimation.uiInstanceBudget );
	pFile->CPrintf( "skinned_models=%u\n", a_rData.oAnimation.uiSkinnedModelCount );
	pFile->CPrintf( "skinned_model_budget=%u\n", a_rData.oAnimation.uiSkinnedModelBudget );
	pFile->CPrintf( "animated_vertices=%u\n", a_rData.oAnimation.uiAnimatedVertices );
	pFile->CPrintf( "animated_vertex_peak=%u\n", a_rData.oAnimation.uiAnimatedVertexPeak );
	pFile->CPrintf( "animated_vertex_budget=%u\n", a_rData.oAnimation.uiAnimatedVertexBudget );
	pFile->CPrintf( "pose_last_us=%u\n", a_rData.oAnimation.uiLastPoseMicroseconds );
	pFile->CPrintf( "pose_max_us=%u\n", a_rData.oAnimation.uiMaxPoseMicroseconds );
	pFile->CPrintf( "skin_last_us=%u\n", a_rData.oAnimation.uiLastSkinMicroseconds );
	pFile->CPrintf( "skin_max_us=%u\n", a_rData.oAnimation.uiMaxSkinMicroseconds );
	pFile->CPrintf( "pose_samples=%u\n", a_rData.oAnimation.uiProfiledPoseCount );
	pFile->CPrintf( "skin_samples=%u\n", a_rData.oAnimation.uiProfiledSkinCount );
	pFile->FlushBuffers();
	pFile->Destroy();
	return TTRUE;
}
