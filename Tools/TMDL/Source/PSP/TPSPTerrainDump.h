#pragma once

#include <Toshi/Defines.h>

TBOOL ExportPSPTerrainDump(
    const TCHAR* a_pInputPath,
    TINT         a_iWorld,
    TINT         a_iCell,
    TINT         a_iMesh,
    const TCHAR* a_pOutputPath
);

TBOOL ExportPSPTerrainCellDump(
    const TCHAR* a_pInputPath,
    TINT         a_iWorld,
    TINT         a_iCell,
    const TCHAR* a_pOutputPath
);
