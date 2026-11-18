#include "pch.h"

#include "AGameRuntimeReport_PSP.h"

#include <Core/TMemory.h>
#include <Math/TMath.h>
#include <Platform/PSP/TRenderMemory_PSP.h>
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
	static TCHAR s_aReportBuffer_PSP[ 512 ];
	const TINT iLength = std::snprintf(
		s_aReportBuffer_PSP,
		TARRAYSIZE( s_aReportBuffer_PSP ),
		"version=2\n"
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
		"render_main_budget=%u\n",
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
		TUINT( s_GameRuntimeReport_PSP.uiRenderMainBudget )
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
