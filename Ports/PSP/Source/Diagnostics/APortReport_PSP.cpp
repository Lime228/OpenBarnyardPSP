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

	pFile->CPrintf( "OpenBarnyard PSP port report v1\n" );
	pFile->CPrintf( "stage=%s\n", a_rData.pStage != TNULL ? a_rData.pStage : "UNKNOWN" );
	pFile->CPrintf( "bootstrap=%s\n", GetReportStatus_PSP( a_rData.bBootstrapReady ) );
	pFile->CPrintf( "input=%s\n", GetReportStatus_PSP( a_rData.bInputReady ) );
	pFile->CPrintf( "toshi=%s\n", GetReportStatus_PSP( a_rData.bToshiReady ) );
	pFile->CPrintf( "file=%s\n", GetReportStatus_PSP( a_rData.bFileReady ) );
	pFile->CPrintf( "audio=%s\n", GetReportStatus_PSP( a_rData.bAudioReady ) );
	pFile->CPrintf( "adpcm=%s\n", GetReportStatus_PSP( a_rData.bADPCMReady ) );
	pFile->CPrintf( "gui_audio=%s\n", GetReportStatus_PSP( a_rData.bGUIAudioReady ) );
	pFile->CPrintf( "stream_underruns=%u\n", a_rData.uiStreamUnderruns );
	pFile->CPrintf( "texture=%s\n", GetReportStatus_PSP( a_rData.bTextureReady ) );
	pFile->CPrintf( "font=%s\n", GetReportStatus_PSP( a_rData.bFontReady ) );
	pFile->CPrintf( "locale=%s\n", GetReportStatus_PSP( a_rData.bLocaleReady ) );
	pFile->CPrintf( "mesh=%s\n", GetReportStatus_PSP( a_rData.bMeshReady ) );
	pFile->CPrintf( "model=%s\n", GetReportStatus_PSP( a_rData.bModelReady ) );
	pFile->CPrintf( "cache=%s\n", GetReportStatus_PSP( a_rData.bCacheReady ) );
	pFile->CPrintf( "scheduler=%s\n", GetReportStatus_PSP( a_rData.bSchedulerReady ) );
	pFile->CPrintf( "frames=%u\n", a_rData.uiFrameCount );
	pFile->CPrintf( "main_current=%u\n", TUINT( a_rData.oMemory.uiMainCurrent ) );
	pFile->CPrintf( "main_peak=%u\n", TUINT( a_rData.oMemory.uiMainPeak ) );
	pFile->CPrintf( "main_budget=%u\n", TUINT( a_rData.oMemory.uiMainBudget ) );
	pFile->CPrintf( "edram_current=%u\n", TUINT( a_rData.oMemory.uiEDRAMCurrent ) );
	pFile->CPrintf( "edram_budget=%u\n", TUINT( a_rData.oMemory.uiEDRAMBudget ) );
	pFile->FlushBuffers();
	pFile->Destroy();
	return TTRUE;
}
