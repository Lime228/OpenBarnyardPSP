#pragma once

#ifdef TOSHI_SKU_WINDOWS
#  include "Platform/Windows/T2Atomic_Win.h"
#endif // TOSHI_SKU_WINDOWS

#ifdef TOSHI_SKU_PSP
#  include "Platform/PSP/T2Atomic_PSP.h"
#endif // TOSHI_SKU_PSP
