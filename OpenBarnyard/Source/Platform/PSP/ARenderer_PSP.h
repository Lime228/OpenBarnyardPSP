#pragma once

#include <Platform/PSP/TRenderInterface_PSP.h>

Toshi::TRenderInterface_PSP& AGetRenderInterface_PSP();
void ALoadFrontEndWorld_PSP();
void AUnloadFrontEndWorld_PSP();
TBOOL ACheckNewGameTerrain_PSP();
TBOOL AHasSSeqOpening_PSP();
TBOOL AStartSSeqOpening_PSP();
TBOOL AIsSSeqOpeningPlaying_PSP();
void AStopSSeqOpening_PSP( TBOOL a_bRestoreFrontend );
