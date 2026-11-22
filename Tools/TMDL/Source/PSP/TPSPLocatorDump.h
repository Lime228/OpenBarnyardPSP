#pragma once

#include <Toshi/Defines.h>

TBOOL ExportPSPLocatorDump(
	const TCHAR* a_pInputPath,
	const TCHAR* a_pLocatorName,
	const TCHAR* a_pOutputPath
);

TBOOL ExportPSPLocatorTableDump(
	const TCHAR* a_pInputPath,
	const TCHAR* a_pOutputPath
);
