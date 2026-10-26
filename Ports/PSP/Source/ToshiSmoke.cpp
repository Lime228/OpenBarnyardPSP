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
#include <Thread/TSemaphore.h>
#include <Thread/TThread.h>

class TSmokeThread_PSP : public Toshi::TThread
{
public:
	TSmokeThread_PSP( Toshi::TSemaphore& a_rSemaphore, TBOOL& a_rRan )
	    : m_rSemaphore( a_rSemaphore )
	    , m_rRan( a_rRan )
	{
	}

	virtual void Main() OVERRIDE
	{
		m_rRan = TTRUE;
		m_rSemaphore.Signal();
	}

private:
	Toshi::TSemaphore& m_rSemaphore;
	TBOOL&              m_rRan;
};

static TBOOL RunThreadSmokeTest()
{
	Toshi::TSemaphore oCompletionSemaphore;
	if ( !oCompletionSemaphore.Create( 0, 1 ) )
		return TFALSE;

	TBOOL            bRan = TFALSE;
	TSmokeThread_PSP oThread( oCompletionSemaphore, bRan );
	TBOOL bThreadReady = oThread.Create(
	    0x4000,
	    Toshi::TThread::THREAD_PRIORITY_NORMAL,
	    Toshi::TThread::FLAGS_NONE
	);

	bThreadReady = bThreadReady &&
	               oCompletionSemaphore.Wait() &&
	               bRan;

	for ( TINT i = 0; i < 100 && oThread.m_iThreadID >= 0; i++ )
		Toshi::ThreadSleep( 1 );

	bThreadReady = oThread.m_iThreadID < 0 && bThreadReady;
	if ( oThread.m_iThreadID >= 0 )
		oThread.Destroy();

	oCompletionSemaphore.Destroy();
	return bThreadReady;
}

static TBOOL RunFileIOSmokeTest()
{
	if ( !Toshi::TFileManager::Create() )
		return TFALSE;

	static const TCHAR s_FileName[] = "toshi_psp_smoke.tmp";
	static const TCHAR s_Contents[] = "OpenBarnyard PSP";
	TCHAR              aReadBuffer[ TARRAYSIZE( s_Contents ) ] = {};
	TBOOL              bFileReady = TFALSE;

	Toshi::TFile* pFile = Toshi::TFile::Create(
	    "local:toshi_psp_smoke.tmp",
	    Toshi::TFILEMODE_READWRITE | Toshi::TFILEMODE_CREATENEW | Toshi::TFILEMODE_NOBUFFER
	);

	if ( pFile != TNULL )
	{
		bFileReady = pFile->Write( s_Contents, sizeof( s_Contents ) ) == sizeof( s_Contents ) &&
		             pFile->GetSize() == sizeof( s_Contents ) &&
		             pFile->Seek( 0, Toshi::TSEEK_SET ) &&
		             pFile->Read( aReadBuffer, sizeof( aReadBuffer ) ) == sizeof( aReadBuffer ) &&
		             Toshi::T2String8::Compare( aReadBuffer, s_Contents ) == 0;
		pFile->Destroy();
	}

	Toshi::TFileSystem* pLocalFileSystem = Toshi::TFileManager::GetSingletonSafe()->FindFileSystem( "local" );
	if ( pLocalFileSystem != TNULL )
	{
		Toshi::TString8 oFoundFile;
		TBOOL bFoundFile = pLocalFileSystem->GetFirstFile( "", oFoundFile, Toshi::TFINDFILE_FILE );
		while ( bFoundFile && oFoundFile != s_FileName )
			bFoundFile = pLocalFileSystem->GetNextFile( oFoundFile, Toshi::TFINDFILE_FILE );

		bFileReady = bFoundFile && pLocalFileSystem->RemoveFile( s_FileName ) && bFileReady;
	}

	Toshi::TFileManager::Destroy();
	return bFileReady;
}

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
	Toshi::TSemaphore oSemaphore;
	TBOOL bSemaphoreReady = oSemaphore.Create( 0, 1 ) &&
	                       !oSemaphore.Poll() &&
	                       oSemaphore.Signal() &&
	                       oSemaphore.Poll() &&
	                       oSemaphore.Signal() &&
	                       oSemaphore.Wait();
	bSemaphoreReady = oSemaphore.Destroy() && bSemaphoreReady;
	const TBOOL bThreadReady = RunThreadSmokeTest();
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
	const TBOOL bFileIOReady = RunFileIOSmokeTest();

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
	       bSemaphoreReady &&
	       bThreadReady &&
	       bStringReady &&
	       bIntegerStringReady &&
	       bDynamicStringReady &&
	       bFilePathReady &&
	       bFileIOReady &&
	       timer.GetRaw64() > 0 &&
	       timer.GetDelta() >= 0.0f;
}
