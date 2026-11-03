#pragma once

#include <Toshi/Defines.h>

TBOOL ExportPSPAnimationDump(
    const TCHAR* a_pModelPath,
    const TCHAR* a_pKeyLibraryPath,
    const TCHAR* a_pSequenceName,
    TINT         a_iSampleRate,
    const TCHAR* a_pOutputPath
);
