#include <Toshi/Defines.h>
#include <Toshi/Platform.h>
#include <Toshi/TString8.h>

#include <pspthreadman.h>

#include <limits>

TOSHI_NAMESPACE_START

static void FixPathSlashes_PSP( TString8& a_rPath )
{
	for ( TINT i = 0; i < a_rPath.Length(); i++ )
	{
		if ( a_rPath[ i ] == '\\' )
			a_rPath[ i ] = '/';
	}
}

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

void FixPathSlashes( TString8& a_rPath )
{
	FixPathSlashes_PSP( a_rPath );
}

void FixPathSlashesBack( TString8& a_rPath )
{
	FixPathSlashes_PSP( a_rPath );
}

TOSHI_NAMESPACE_END
