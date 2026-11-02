#pragma once

#include <Toshi/Typedefs.h>
#include <Platform/PSP/TRenderMemory_PSP.h>

struct APortReportData_PSP
{
	const TCHAR* pStage;
	TBOOL bBootstrapReady;
	TBOOL bInputReady;
	TBOOL bToshiReady;
	TBOOL bFileReady;
	TBOOL bAudioReady;
	TBOOL bADPCMReady;
	TBOOL bGUIAudioReady;
	TBOOL bManagedCueReady;
	TBOOL bCueExReady;
	TBOOL bCueControlsReady;
	TBOOL bDelayedCueControlsReady;
	TBOOL bCueFrequencyReady;
	TBOOL bCuePositionReady;
	TUINT uiStreamUnderruns;
	TBOOL bTextureReady;
	TBOOL bFontReady;
	TBOOL bLocaleReady;
	TBOOL bMeshReady;
	TBOOL bModelReady;
	TBOOL bCacheReady;
	TBOOL bSchedulerReady;
	TUINT uiFrameCount;
	Toshi::TRenderMemoryStats_PSP oMemory;
};

class APortReport_PSP
{
public:
	static TBOOL Write( const APortReportData_PSP& a_rData );
};
