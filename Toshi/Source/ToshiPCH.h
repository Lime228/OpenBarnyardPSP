#pragma once

#ifdef TOSHI_SKU_PSP

#  include <algorithm>
#  include <cstdarg>
#  include <map>
#  include <memory>
#  include <string>
#  include <unordered_map>
#  include <unordered_set>
#  include <utility>
#  include <vector>

#  include "Toshi/Toshi.h"

#else

#include <map>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <algorithm>
#include <string>
#include <memory>
#include <utility>
#include <cstdarg>

#include "Toshi/Toshi.h"

/* Platform specific headers */
#ifdef TOSHI_SKU_WINDOWS
#  include <WinSock2.h>
#  include <Windows.h>
#endif

#endif // TOSHI_SKU_PSP
