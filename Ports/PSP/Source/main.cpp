#include <pspctrl.h>
#include <pspkernel.h>

#include <Toshi/Typedefs.h>
#include <Toshi/TDebug.h>
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

static_assert( sizeof( void* ) == 4, "The PSP port requires a 32-bit ABI" );

static TBOOL InitialiseProbeTexture()
{
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

	TFLOAT      fXOffset       = 0.0f;
	const TBOOL bToshiReady    = RunToshiPortabilitySmokeTest();
	const TBOOL bTextureReady  = InitialiseProbeTexture();
	const TBOOL bBootstrapReady = bToshiReady && bTextureReady;
	TUINT32     uiClearColor   = bBootstrapReady ? 0xff201810 : 0xff202080;
	TDebug_FinalPrintf( "[PSP render] texture     %s\n", bTextureReady ? "OK" : "FAILED" );

	while ( g_Running )
	{
		SceCtrlData pad{};
		sceCtrlPeekBufferPositive( &pad, 1 );

		const TFLOAT fAnalogX = ( static_cast<TINT>( pad.Lx ) - 128 ) / 128.0f;
		fXOffset += fAnalogX * 2.0f;
		if ( fXOffset < -100.0f ) fXOffset = -100.0f;
		if ( fXOffset > 100.0f ) fXOffset = 100.0f;

		if ( bBootstrapReady )
			uiClearColor = ( pad.Buttons & PSP_CTRL_CROSS ) ? 0xff402010 : 0xff201810;
		g_RenderInterface.BeginScene( uiClearColor );
		g_RenderInterface.DrawColourTriangle( fXOffset );
		if ( bTextureReady )
			g_RenderInterface.DrawTextureSprite( g_ProbeTexture, 176.0f + fXOffset, 72.0f, 128.0f, 128.0f );
		g_RenderInterface.EndScene();
	}

	g_ProbeTexture.Unload();
	g_RenderInterface.DestroyDisplay();
	sceKernelExitGame();
	return 0;
}
