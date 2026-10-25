#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>

#include <Toshi/Typedefs.h>

#include "ToshiSmoke.h"

PSP_MODULE_INFO( "OpenBarnyard PSP Probe", PSP_MODULE_USER, 0, 1 );
PSP_MAIN_THREAD_ATTR( PSP_THREAD_ATTR_USER );
PSP_HEAP_SIZE_KB( -1024 );

static constexpr TINT kScreenWidth  = 480;
static constexpr TINT kScreenHeight = 272;
static constexpr TINT kBufferWidth  = 512;
static constexpr TINT kFrameBytes   = kBufferWidth * kScreenHeight * 4;

alignas( 16 ) static TUINT32 g_DisplayList[ 262144 ];
static volatile TBOOL        g_Running = TTRUE;

struct Vertex_PSP
{
	TUINT32 uiColour;
	TFLOAT  fX;
	TFLOAT  fY;
	TFLOAT  fZ;
};

static_assert( sizeof( void* ) == 4, "The PSP port requires a 32-bit ABI" );
static_assert( sizeof( Vertex_PSP ) == 16, "Unexpected PSP vertex layout" );

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

static void InitialiseGraphics()
{
	sceGuInit();
	sceGuStart( GU_DIRECT, g_DisplayList );
	sceGuDrawBuffer( GU_PSM_8888, reinterpret_cast<void*>( 0 ), kBufferWidth );
	sceGuDispBuffer( kScreenWidth, kScreenHeight, reinterpret_cast<void*>( kFrameBytes ), kBufferWidth );
	sceGuDepthBuffer( reinterpret_cast<void*>( kFrameBytes * 2 ), kBufferWidth );
	sceGuOffset( 2048 - kScreenWidth / 2, 2048 - kScreenHeight / 2 );
	sceGuViewport( 2048, 2048, kScreenWidth, kScreenHeight );
	sceGuDepthRange( 65535, 0 );
	sceGuScissor( 0, 0, kScreenWidth, kScreenHeight );
	sceGuEnable( GU_SCISSOR_TEST );
	sceGuDisable( GU_DEPTH_TEST );
	sceGuDisable( GU_TEXTURE_2D );
	sceGuShadeModel( GU_SMOOTH );
	sceGuFinish();
	sceGuSync( GU_SYNC_FINISH, GU_SYNC_WHAT_DONE );
	sceDisplayWaitVblankStart();
	sceGuDisplay( GU_TRUE );
}

static void DrawFrame( TFLOAT a_fXOffset, TUINT32 a_uiClearColour )
{
	sceGuStart( GU_DIRECT, g_DisplayList );
	sceGuClearColor( a_uiClearColour );
	sceGuClear( GU_COLOR_BUFFER_BIT );

	auto* pVertices = static_cast<Vertex_PSP*>( sceGuGetMemory( 3 * sizeof( Vertex_PSP ) ) );
	pVertices[ 0 ]  = { 0xff5ac8fa, 240.0f + a_fXOffset, 46.0f, 0.0f };
	pVertices[ 1 ]  = { 0xff65d26e, 116.0f + a_fXOffset, 226.0f, 0.0f };
	pVertices[ 2 ]  = { 0xfff0cf65, 364.0f + a_fXOffset, 226.0f, 0.0f };

	sceGuDrawArray(
	    GU_TRIANGLES,
	    GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
	    3,
	    TNULL,
	    pVertices
	);
	sceGuFinish();
	sceGuSync( GU_SYNC_FINISH, GU_SYNC_WHAT_DONE );
	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
}

int main()
{
	SetupCallbacks();
	InitialiseGraphics();
	sceCtrlSetSamplingCycle( 0 );
	sceCtrlSetSamplingMode( PSP_CTRL_MODE_ANALOG );

	TFLOAT        fXOffset     = 0.0f;
	const TBOOL   bToshiReady  = RunToshiPortabilitySmokeTest();
	TUINT32       uiClearColor = bToshiReady ? 0xff201810 : 0xff202080;

	while ( g_Running )
	{
		SceCtrlData pad{};
		sceCtrlPeekBufferPositive( &pad, 1 );

		const TFLOAT fAnalogX = ( static_cast<TINT>( pad.Lx ) - 128 ) / 128.0f;
		fXOffset += fAnalogX * 2.0f;
		if ( fXOffset < -100.0f ) fXOffset = -100.0f;
		if ( fXOffset > 100.0f ) fXOffset = 100.0f;

		if ( bToshiReady )
			uiClearColor = ( pad.Buttons & PSP_CTRL_CROSS ) ? 0xff402010 : 0xff201810;
		DrawFrame( fXOffset, uiClearColor );
	}

	sceGuTerm();
	sceKernelExitGame();
	return 0;
}
