#include "ToshiSmoke.h"

#include <Toshi/Defines.h>
#include <Toshi/T2Allocator.h>
#include <Toshi/TDebug.h>
#include <Toshi/Endianness.h>
#include <Toshi/Platform.h>
#include <Toshi/THPTimer.h>
#include <Toshi/Verify.h>

TBOOL RunToshiPortabilitySmokeTest()
{
	alignas( TUINT32 ) constexpr TUINT8 bytes[] = { 0x78, 0x56, 0x34, 0x12 };
	Toshi::THPTimer timer;
	Toshi::T2Allocator* pAllocator = Toshi::GetGlobalAllocator();
	TUINT8* pBuffer = TSTATICCAST( TUINT8, pAllocator->Malloc( 48, 64 ) );
	TBOOL bAllocatorReady = pBuffer != TNULL &&
	                        ( reinterpret_cast<TUINTPTR>( pBuffer ) & 63U ) == 0;

	if ( pBuffer != TNULL )
	{
		for ( TSIZE i = 0; i < 48; i++ )
			pBuffer[ i ] = TUINT8( i );

		bAllocatorReady = bAllocatorReady && pBuffer[ 47 ] == 47;
		pAllocator->Free( pBuffer );
	}

	Toshi::ThreadSleep( 0 );
	timer.Update();
	TDebug_FinalPrintf( "Toshi PSP smoke test started\n" );

	return sizeof( TUINT32 ) == 4 &&
	       sizeof( TUINTPTR ) == 4 &&
	       CURRENT_ENDIANESS == Endianess_Little &&
	       PARSEDWORD( bytes ) == 0x12345678U &&
	       bAllocatorReady &&
	       timer.GetRaw64() > 0 &&
	       timer.GetDelta() >= 0.0f;
}
