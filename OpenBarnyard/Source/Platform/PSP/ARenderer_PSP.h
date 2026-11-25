#pragma once

#include <Platform/PSP/TRenderInterface_PSP.h>

Toshi::TRenderInterface_PSP& AGetRenderInterface_PSP();
void ALoadFrontEndWorld_PSP();
void AUnloadFrontEndWorld_PSP();
TBOOL ACheckNewGameTerrain_PSP();
TBOOL AHasSSeqOpening_PSP();
TBOOL AStartSSeqOpening_PSP();
TBOOL AHasSSeqContinuation_PSP();
TBOOL AStartSSeqContinuation_PSP( TUINT a_uiGender, TUINT a_uiCostume );
TBOOL AIsSSeqContinuationComplete_PSP();
TBOOL AIsSSeqOpeningPlaying_PSP();
TBOOL AIsSSeqCharacterSetupReady_PSP();
void ASetSSeqCharacterSetupVisible_PSP( TBOOL a_bVisible );
void AStopSSeqOpening_PSP( TBOOL a_bRestoreFrontend );
