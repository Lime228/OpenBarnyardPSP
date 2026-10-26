#include "ToshiSmoke.h"

#include <Toshi/Defines.h>
#include <Toshi/TDebug.h>
#include <Toshi/T2Allocator.h>
#include <Toshi/T2Map.h>
#include <Toshi/T2String8.h>
#include <Toshi/Endianness.h>
#include <Toshi/Platform.h>
#include <Toshi/THPTimer.h>
#include <Toshi/TString8.h>
#include <Toshi/Verify.h>

#include <File/TFile.h>
#include <Thread/TMutex.h>

TBOOL RunToshiPortabilitySmokeTest()
{
	alignas( TUINT32 ) constexpr TUINT8 bytes[] = { 0x78, 0x56, 0x34, 0x12 };
	Toshi::THPTimer timer;
	Toshi::T2Allocator* pAllocator = Toshi::GetGlobalAllocator();
	TUINT8* pBuffer = TSTATICCAST( TUINT8, pAllocator->Malloc( 48, 64 ) );
	TBOOL bAllocatorReady = pBuffer != TNULL &&
	                        ( reinterpret_cast<TUINTPTR>( pBuffer ) & 63U ) == 0;
	Toshi::T2Map<TINT, TINT> oValues;
	oValues.Insert( 2, 20 );
	oValues.Insert( 1, 10 );
	oValues.Insert( 3, 30 );
	Toshi::T2Map<TINT, TINT>::Iterator itValue = oValues.Find( 2 );
	const TBOOL bContainerReady = oValues.Size() == 3 &&
	                              itValue != oValues.End() &&
	                              itValue->GetSecond() == 20;
	Toshi::TMutex oMutex;
	TBOOL bMutexReady = oMutex.Create() &&
	                    oMutex.Lock() &&
	                    oMutex.Lock( Toshi::TMutex::FLAG_NO_WAIT ) &&
	                    oMutex.Unlock() &&
	                    oMutex.Unlock();
	bMutexReady = oMutex.Destroy() && bMutexReady;
	TCHAR aStringBuffer[ 32 ];
	Toshi::T2String8::Format( aStringBuffer, TARRAYSIZE( aStringBuffer ), "psp-%d", 228 );
	const TBOOL bStringReady = Toshi::T2String8::Compare( aStringBuffer, "psp-228" ) == 0 &&
	                           Toshi::T2String8::CompareNoCase( "BARNYARD", "barnyard" ) == 0;
	Toshi::T2String8::IntToString( -228, aStringBuffer, 10 );
	const TBOOL bIntegerStringReady = Toshi::T2String8::Compare( aStringBuffer, "-228" ) == 0;
	Toshi::TString8 oDynamicString;
	oDynamicString.Format( "Barnyard-%d", 228 );
	const TBOOL bDynamicStringReady = oDynamicString.CompareNoCase( "barnyard-228" ) == 0;
	const Toshi::TString8 oJoinedPath = Toshi::TFile::ConcatPath( "local:/data/actors", "../textures/cow.tga" );
	const TBOOL bFilePathReady = oJoinedPath == "local:/data/textures/cow.tga";

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
	       bContainerReady &&
	       bMutexReady &&
	       bStringReady &&
	       bIntegerStringReady &&
	       bDynamicStringReady &&
	       bFilePathReady &&
	       timer.GetRaw64() > 0 &&
	       timer.GetDelta() >= 0.0f;
}
