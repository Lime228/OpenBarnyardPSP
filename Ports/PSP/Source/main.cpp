#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>

#include <cstdint>

#include "ToshiSmoke.h"

PSP_MODULE_INFO( "OpenBarnyard PSP Probe", PSP_MODULE_USER, 0, 1 );
PSP_MAIN_THREAD_ATTR( PSP_THREAD_ATTR_USER );
PSP_HEAP_SIZE_KB( -1024 );

namespace
{
constexpr int kScreenWidth  = 480;
constexpr int kScreenHeight = 272;
constexpr int kBufferWidth  = 512;
constexpr int kFrameBytes   = kBufferWidth * kScreenHeight * 4;

alignas( 16 ) std::uint32_t g_DisplayList[ 262144 ];
volatile bool               g_Running = true;

struct Vertex
{
	std::uint32_t color;
	float         x;
	float         y;
	float         z;
};

static_assert( sizeof( void* ) == 4, "The PSP port requires a 32-bit ABI" );
static_assert( sizeof( Vertex ) == 16, "Unexpected PSP vertex layout" );

int ExitCallback( int, int, void* )
{
	g_Running = false;
	return 0;
}

int CallbackThread( SceSize, void* )
{
	const int callback = sceKernelCreateCallback( "OpenBarnyardExit", ExitCallback, nullptr );
	sceKernelRegisterExitCallback( callback );
	sceKernelSleepThreadCB();
	return 0;
}

void SetupCallbacks()
{
	const int thread = sceKernelCreateThread(
	    "OpenBarnyardCallbacks",
	    CallbackThread,
	    0x11,
	    0x1000,
	    PSP_THREAD_ATTR_USER,
	    nullptr
	);

	if ( thread >= 0 )
		sceKernelStartThread( thread, 0, nullptr );
}

void InitialiseGraphics()
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

void DrawFrame( float xOffset, std::uint32_t clearColor )
{
	sceGuStart( GU_DIRECT, g_DisplayList );
	sceGuClearColor( clearColor );
	sceGuClear( GU_COLOR_BUFFER_BIT );

	auto* vertices = static_cast<Vertex*>( sceGuGetMemory( 3 * sizeof( Vertex ) ) );
	vertices[ 0 ]  = { 0xff5ac8fa, 240.0f + xOffset, 46.0f, 0.0f };
	vertices[ 1 ]  = { 0xff65d26e, 116.0f + xOffset, 226.0f, 0.0f };
	vertices[ 2 ]  = { 0xfff0cf65, 364.0f + xOffset, 226.0f, 0.0f };

	sceGuDrawArray(
	    GU_TRIANGLES,
	    GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
	    3,
	    nullptr,
	    vertices
	);
	sceGuFinish();
	sceGuSync( GU_SYNC_FINISH, GU_SYNC_WHAT_DONE );
	sceDisplayWaitVblankStart();
	sceGuSwapBuffers();
}
} // namespace

int main()
{
	SetupCallbacks();
	InitialiseGraphics();
	sceCtrlSetSamplingCycle( 0 );
	sceCtrlSetSamplingMode( PSP_CTRL_MODE_ANALOG );

	float         xOffset    = 0.0f;
	const bool    toshiReady = RunToshiPortabilitySmokeTest();
	std::uint32_t clearColor = toshiReady ? 0xff201810 : 0xff202080;

	while ( g_Running )
	{
		SceCtrlData pad{};
		sceCtrlPeekBufferPositive( &pad, 1 );

		const float analogX = ( static_cast<int>( pad.Lx ) - 128 ) / 128.0f;
		xOffset += analogX * 2.0f;
		if ( xOffset < -100.0f ) xOffset = -100.0f;
		if ( xOffset > 100.0f ) xOffset = 100.0f;

		if ( toshiReady )
			clearColor = ( pad.Buttons & PSP_CTRL_CROSS ) ? 0xff402010 : 0xff201810;
		DrawFrame( xOffset, clearColor );
	}

	sceGuTerm();
	sceKernelExitGame();
	return 0;
}
