#pragma once

#include <Toshi/Defines.h>

TBOOL ExportPSPAnimationInfo( const TCHAR* a_pInputPath, const TCHAR* a_pOutputPath );

TBOOL ExportPSPAnimationDump(
    const TCHAR* a_pModelPath,
    const TCHAR* a_pKeyLibraryPath,
    const TCHAR* a_pKeyLibrarySymbol,
    const TCHAR* a_pSequenceName,
    TINT         a_iSampleRate,
    const TCHAR* a_pOutputPath
);
