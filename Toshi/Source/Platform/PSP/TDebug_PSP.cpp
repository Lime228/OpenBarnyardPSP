#include <Toshi/TDebug.h>

#include <pspthreadman.h>

#include <cstdio>

static constexpr TSIZE kScratchBufferSize = 4096;
static TCHAR           g_ScratchBuffer[ kScratchBufferSize ];
static SceUID          g_DebugSemaphore = -1;

void TDebug_AcquireMutex()
{
	if ( g_DebugSemaphore < 0 )
	{
		g_DebugSemaphore = sceKernelCreateSema( "ToshiDebug", 0, 1, 1, nullptr );
	}

	if ( g_DebugSemaphore >= 0 )
	{
		sceKernelWaitSema( g_DebugSemaphore, 1, nullptr );
	}
}

void TDebug_ReleaseMutex()
{
	if ( g_DebugSemaphore >= 0 )
	{
		sceKernelSignalSema( g_DebugSemaphore, 1 );
	}
}

TBOOL TDebug_IsValidAddress( const void* a_pPtr )
{
	const TUINTPTR uiAddress = reinterpret_cast<TUINTPTR>( a_pPtr );

	return a_pPtr != nullptr &&
	       uiAddress != 0xcdcdcdcdU &&
	       uiAddress != 0xdeaddeadU &&
	       uiAddress != 0xbeefbeefU &&
	       uiAddress >= 0x4fU;
}

void TDebug_FinalPrintf( const TCHAR* a_szFormat, ... )
{
	va_list args;
	va_start( args, a_szFormat );
	TDebug_FinalVPrintf( a_szFormat, args );
	va_end( args );
}

void TDebug_FinalVPrintf( const TCHAR* a_szFormat, va_list a_Args )
{
	TDebug_FinalVSPrintf( a_szFormat, a_Args );
	TDebug_OutputDebugString( g_ScratchBuffer );
}

void TDebug_FinalSPrintf( const TCHAR* a_szFormat, ... )
{
	va_list args;
	va_start( args, a_szFormat );
	TDebug_FinalVSPrintf( a_szFormat, args );
	va_end( args );
}

void TDebug_FinalVSPrintf( const TCHAR* a_szFormat, va_list a_Args )
{
	TDebug_AcquireMutex();
	std::vsnprintf( g_ScratchBuffer, kScratchBufferSize, a_szFormat, a_Args );
	g_ScratchBuffer[ kScratchBufferSize - 1 ] = '\0';
	TDebug_ReleaseMutex();
}

void TDebug_OutputDebugString( const TCHAR* a_szString )
{
	std::fputs( a_szString, stdout );
	std::fflush( stdout );
	TDebug_BroadcastDebugString( a_szString );
}

void TDebug_BroadcastDebugString( const TCHAR* )
{
}

TINT TDebug_AssertHandler(
	const TCHAR* a_szExpression,
	const TCHAR* a_szFileName,
	TUINT        a_uiLineNumber,
	const TCHAR* a_szDescription )
{
	TDebug_FinalPrintf(
	    "%s: assertion (%s) failed ('%s', at line %u)\n",
	    a_szDescription,
	    a_szExpression,
	    a_szFileName,
	    a_uiLineNumber
	);

	return 0;
}
