#include <pspctrl.h>
#include <pspkernel.h>

#include <Toshi/Typedefs.h>
#include <Toshi/T2Allocator.h>
#include <Toshi/TDebug.h>
#include <File/TFile.h>
#include <Render/TBackend.h>

#include "ToshiSmoke.h"

PSP_MODULE_INFO( "OpenBarnyard PSP Probe", PSP_MODULE_USER, 0, 1 );
PSP_MAIN_THREAD_ATTR( PSP_THREAD_ATTR_USER );
PSP_HEAP_SIZE_KB( -1024 );

static volatile TBOOL g_Running = TTRUE;

struct ProbeTextureData_PSP
{
	Toshi::T2TextureDataHeader_PSP oHeader;
	TUINT32                        aPixels[ 16 * 16 ];
};

alignas( 16 ) static ProbeTextureData_PSP g_ProbeTextureData = {
	{
	    Toshi::T2TextureDataHeader_PSP::MAGIC,
	    Toshi::T2TextureDataHeader_PSP::VERSION,
	    16,
	    16,
	    16,
	    Toshi::T2TEXTUREFORMAT_PSP_RGBA8888,
	},
	{}
};
static Toshi::T2Texture g_ProbeTexture;
static Toshi::TRenderInterface_PSP g_RenderInterface;
static Toshi::TVertexPoolResource_PSP g_ProbeVertices_PSP;
static Toshi::TIndexPoolResource_PSP  g_ProbeIndices_PSP;
static Toshi::TModel_PSP                g_ProbeModel_PSP;
static TBOOL                            g_bExternalTexture_PSP = TFALSE;

static_assert( sizeof( void* ) == 4, "The PSP port requires a 32-bit ABI" );

static void* ReadProbeFile_PSP( const TCHAR* a_pFileName, TSIZE a_uiMinSize, TSIZE a_uiMaxSize, TSIZE& a_rFileSize )
{
	Toshi::TFile* pFile = Toshi::TFile::Create(
	    a_pFileName,
	    Toshi::TFILEMODE_READ | Toshi::TFILEMODE_NOBUFFER
	);
	if ( pFile == TNULL )
		return TNULL;

	void*       pData      = TNULL;
	const TSIZE uiFileSize = pFile->GetSize();
	if ( uiFileSize >= a_uiMinSize && uiFileSize <= a_uiMaxSize )
	{
		pData = Toshi::GetGlobalAllocator()->Malloc( uiFileSize, 16 );
		if ( pData != TNULL && pFile->Read( pData, uiFileSize ) != uiFileSize )
		{
			Toshi::GetGlobalAllocator()->Free( pData );
			pData = TNULL;
		}
	}

	pFile->Destroy();
	if ( pData != TNULL )
		a_rFileSize = uiFileSize;
	return pData;
}

static TBOOL LoadProbeTextureFile_PSP()
{
	TSIZE uiFileSize = 0;
	void* pData = ReadProbeFile_PSP(
	    "local:probe.ptex",
	    sizeof( Toshi::T2TextureDataHeader_PSP ),
	    4 * 1024 * 1024,
	    uiFileSize
	);
	if ( pData == TNULL )
		return TFALSE;

	g_ProbeTexture.SetData( pData, TUINT( uiFileSize ) );
	const TBOOL bLoaded = g_ProbeTexture.Load();
	g_ProbeTexture.SetData( TNULL, 0 );
	Toshi::GetGlobalAllocator()->Free( pData );
	return bLoaded;
}

static TBOOL LoadProbeModelFile_PSP()
{
	TSIZE uiFileSize = 0;
	void* pData = ReadProbeFile_PSP(
	    "local:probe.pmes",
	    sizeof( Toshi::TModelDataHeader_PSP ),
	    2 * 1024 * 1024,
	    uiFileSize
	);
	if ( pData == TNULL )
		return TFALSE;

	g_ProbeModel_PSP.SetData( pData, TUINT( uiFileSize ) );
	const TBOOL bLoaded = g_ProbeModel_PSP.Load();
	g_ProbeModel_PSP.SetData( TNULL, 0 );
	Toshi::GetGlobalAllocator()->Free( pData );
	if ( !bLoaded )
	{
		g_ProbeModel_PSP.Unload();
	}
	return bLoaded;
}

static TBOOL InitialiseProbeTexture( TBOOL a_bLoadExternal )
{
	g_bExternalTexture_PSP = a_bLoadExternal && LoadProbeTextureFile_PSP();
	if ( g_bExternalTexture_PSP )
		return TTRUE;

	for ( TUINT y = 0; y < 16; y++ )
	{
		for ( TUINT x = 0; x < 16; x++ )
		{
			const TBOOL bAlternate = ( ( x / 4 ) + ( y / 4 ) ) % 2 != 0;
			g_ProbeTextureData.aPixels[ y * 16 + x ] = bAlternate ? 0xfff0cf65 : 0xff5ac8fa;
		}
	}

	g_ProbeTexture.SetData( &g_ProbeTextureData, sizeof( g_ProbeTextureData ) );
	return g_ProbeTexture.Load();
}

static TBOOL InitialiseProbeMesh_PSP()
{
	if ( !g_ProbeVertices_PSP.Create( TUINT16( sizeof( Toshi::TRenderColourVertex_PSP ) ), 4 ) ||
	     !g_ProbeIndices_PSP.Create( 6 ) )
	{
		g_ProbeVertices_PSP.Destroy();
		g_ProbeIndices_PSP.Destroy();
		return TFALSE;
	}

	auto* pVertices = reinterpret_cast<Toshi::TRenderColourVertex_PSP*>( g_ProbeVertices_PSP.Lock() );
	auto* pIndices  = g_ProbeIndices_PSP.Lock();
	if ( pVertices == TNULL || pIndices == TNULL )
	{
		if ( pVertices != TNULL ) g_ProbeVertices_PSP.Unlock( 0 );
		if ( pIndices != TNULL ) g_ProbeIndices_PSP.Unlock( 0 );
		g_ProbeVertices_PSP.Destroy();
		g_ProbeIndices_PSP.Destroy();
		return TFALSE;
	}

	pVertices[ 0 ] = { 0xff303060, 152.0f, 48.0f, 0.0f };
	pVertices[ 1 ] = { 0xff603030, 328.0f, 48.0f, 0.0f };
	pVertices[ 2 ] = { 0xff306030, 328.0f, 224.0f, 0.0f };
	pVertices[ 3 ] = { 0xff606030, 152.0f, 224.0f, 0.0f };
	pIndices[ 0 ] = 0;
	pIndices[ 1 ] = 1;
	pIndices[ 2 ] = 2;
	pIndices[ 3 ] = 0;
	pIndices[ 4 ] = 2;
	pIndices[ 5 ] = 3;
	g_ProbeVertices_PSP.Unlock( 4 );
	g_ProbeIndices_PSP.Unlock( 6 );
	return TTRUE;
}

static TINT ExitCallback( TINT, TINT, void* )
{
	g_Running = TFALSE;
	return 0;
}

static TINT CallbackThread( SceSize, void* )
{
	const TINT iCallback = sceKernelCreateCallback( "OpenBarnyardExit", ExitCallback, TNULL );
	sceKernelRegisterExitCallback( iCallback );
	sceKernelSleepThreadCB();
	return 0;
}

static void SetupCallbacks()
{
	const TINT iThread = sceKernelCreateThread(
	    "OpenBarnyardCallbacks",
	    CallbackThread,
	    0x11,
	    0x1000,
	    PSP_THREAD_ATTR_USER,
	    TNULL
	);

	if ( iThread >= 0 )
		sceKernelStartThread( iThread, 0, TNULL );
}

int main()
{
	SetupCallbacks();
	if ( !g_RenderInterface.CreateDisplay() )
	{
		sceKernelExitGame();
		return 1;
	}
	sceCtrlSetSamplingCycle( 0 );
	sceCtrlSetSamplingMode( PSP_CTRL_MODE_ANALOG );

	TFLOAT      fXOffset        = 0.0f;
	TFLOAT      fModelRotation  = 0.0f;
	const TBOOL bToshiReady     = RunToshiPortabilitySmokeTest();
	const TBOOL bFileReady      = Toshi::TFileManager::Create();
	const TBOOL bTextureReady   = InitialiseProbeTexture( bFileReady );
	const TBOOL bModelReady     = bFileReady && LoadProbeModelFile_PSP();
	if ( bFileReady )
		Toshi::TFileManager::Destroy();
	const TBOOL bMeshReady      = InitialiseProbeMesh_PSP();
	const TBOOL bBootstrapReady = bToshiReady && bTextureReady && bMeshReady && bModelReady;
	TUINT32     uiClearColor   = bBootstrapReady ? 0xff201810 : 0xff202080;
	TDebug_FinalPrintf( "[PSP render] texture     %s\n", bTextureReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP render] source      %s\n", g_bExternalTexture_PSP ? "probe.ptex" : "embedded fallback" );
	TDebug_FinalPrintf( "[PSP render] index mesh  %s\n", bMeshReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP render] model       %s\n", bModelReady ? "probe.pmes" : "FAILED" );

	while ( g_Running )
	{
		SceCtrlData pad{};
		sceCtrlPeekBufferPositive( &pad, 1 );

		const TFLOAT fAnalogX = ( static_cast<TINT>( pad.Lx ) - 128 ) / 128.0f;
		fXOffset += fAnalogX * 2.0f;
		if ( fXOffset < -100.0f ) fXOffset = -100.0f;
		if ( fXOffset > 100.0f ) fXOffset = 100.0f;
		fModelRotation += 0.015f;

		if ( bBootstrapReady )
			uiClearColor = ( pad.Buttons & PSP_CTRL_CROSS ) ? 0xff402010 : 0xff201810;
		g_RenderInterface.BeginScene( uiClearColor );
		g_RenderInterface.DrawColourTriangle( fXOffset );
		if ( bMeshReady )
			g_RenderInterface.DrawIndexedColourTriangles( g_ProbeVertices_PSP, g_ProbeIndices_PSP );
		if ( bTextureReady )
			g_RenderInterface.DrawTextureSprite( g_ProbeTexture, 176.0f + fXOffset, 72.0f, 128.0f, 128.0f );
		if ( bModelReady )
			g_RenderInterface.DrawModel( g_ProbeModel_PSP, fModelRotation, &g_ProbeTexture );
		g_RenderInterface.EndScene();
	}

	g_ProbeModel_PSP.Unload();
	g_ProbeTexture.Unload();
	g_ProbeIndices_PSP.Destroy();
	g_ProbeVertices_PSP.Destroy();
	g_RenderInterface.DestroyDisplay();
	sceKernelExitGame();
	return 0;
}
