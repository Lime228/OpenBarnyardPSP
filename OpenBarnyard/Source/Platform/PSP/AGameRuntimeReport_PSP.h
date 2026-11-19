#pragma once

#include <Toshi/Defines.h>

struct AGameRuntimeReport_PSP
{
	TCHAR szPhase[ 48 ];
	TCHAR szPicture[ 64 ];
	TCHAR szFailedPath[ 96 ];
	TINT  iPictureTiles;
	TSIZE uiHeapFree;
	TSIZE uiHeapLargestHole;
	TSIZE uiHeapUsed;
	TSIZE uiRenderMainCurrent;
	TSIZE uiRenderMainPeak;
	TSIZE uiRenderMainBudget;
	TUINT uiRendererFrames;
	TUINT uiPictureRenderFrames;
	TUINT uiPictureDraws;
	TUINT uiGUIColourDraws;
	TUINT uiLastGUIColour;
	TBOOL bEngineCreated;
	TBOOL bApplicationCreated;
	TBOOL bRootCreated;
	TBOOL bGUI2Created;
	TBOOL bGUISystemCreated;
	TBOOL bControlsLoaded;
};

AGameRuntimeReport_PSP& AGetGameRuntimeReport_PSP();
void ASetGameRuntimePhase_PSP( const TCHAR* a_szPhase );
void ASetGameRuntimePicture_PSP( const TCHAR* a_szPicture );
void ASetGameRuntimeFailedPath_PSP( const TCHAR* a_szPath );
void AUpdateGameRuntimeMemory_PSP();
void AWriteGameRuntimeReport_PSP();
