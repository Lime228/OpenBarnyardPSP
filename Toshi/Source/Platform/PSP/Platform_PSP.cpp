#include <Toshi/Defines.h>
#include <Toshi/Platform.h>

#include <pspthreadman.h>

#include <limits>

TOSHI_NAMESPACE_START

void ThreadSleep( TUINT a_uiMilliseconds )
{
	constexpr TUINT MAX_DELAY_MILLISECONDS = std::numeric_limits<TUINT>::max() / 1000U;

	while ( a_uiMilliseconds > MAX_DELAY_MILLISECONDS )
	{
		sceKernelDelayThread( MAX_DELAY_MILLISECONDS * 1000U );
		a_uiMilliseconds -= MAX_DELAY_MILLISECONDS;
	}

	sceKernelDelayThread( a_uiMilliseconds * 1000U );
}

TOSHI_NAMESPACE_END
