#pragma once
#ifdef TOSHI_SKU_WINDOWS
#  include "Platform/Windows/TDebugConsole_Win.h"
#elif defined( TOSHI_SKU_PSP )
#  include "Platform/PSP/TDebugConsole_PSP.h"
#endif // TOSHI_SKU_WINDOWS
