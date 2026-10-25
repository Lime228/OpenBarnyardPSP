#pragma once

#ifdef TOSHI_SKU_WINDOWS
#  include "Platform/Windows/THPTimer_Win.h"
#elif defined( TOSHI_SKU_PSP )
#  include "Platform/PSP/THPTimer_PSP.h"
#else
#  error Unsupported platform timer
#endif
