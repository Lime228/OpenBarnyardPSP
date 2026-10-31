#include <pspkernel.h>

#include <Toshi/Typedefs.h>
#include <Toshi/TDebug.h>
#include <Toshi/THPTimer.h>
#include <Toshi/TScheduler.h>
#include <File/TFile.h>
#include <Platform/PSP/Input/TInputInterface_PSP.h>
#include <Render/TBackend.h>

#include "Frontend/AApplication_PSP.h"
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
static Toshi::T2Texture g_FontTexture_PSP;
static Toshi::T2GUIFont_PSP g_Font_PSP;
static Toshi::TTextureFactory_PSP g_TextureFactory_PSP;
static Toshi::T2GUIFontFactory_PSP g_FontFactory_PSP;
static Toshi::TModelFactory_PSP g_ModelFactory_PSP;
static Toshi::TRenderResourceCache_PSP g_ResourceCache_PSP;
static Toshi::TRenderInterface_PSP g_RenderInterface;
static Toshi::TVertexPoolResource_PSP g_ProbeVertices_PSP;
static Toshi::TIndexPoolResource_PSP  g_ProbeIndices_PSP;
static Toshi::TModel_PSP                g_ProbeModel_PSP;
static TBOOL                            g_bExternalTexture_PSP = TFALSE;
static const Toshi::TRenderMaterial_PSP g_ProbeMaterial_PSP = {
	TTRUE,
	Toshi::TRENDERTEXTUREFUNCTION_PSP_MODULATE,
	Toshi::TRENDERCULLMODE_PSP_BACK,
};

static_assert( sizeof( void* ) == 4, "The PSP port requires a 32-bit ABI" );

static TBOOL LoadProbeTextureFile_PSP()
{
	return g_TextureFactory_PSP.CreateTextureFromFile( g_ProbeTexture, "local:probe.ptex" );
}

static TBOOL LoadFontTextureFile_PSP()
{
	return g_TextureFactory_PSP.CreateTextureFromFile( g_FontTexture_PSP, "local:bootstrap_font.ptex" );
}

static TBOOL LoadFontFile_PSP()
{
	return g_FontFactory_PSP.CreateFontFromFile(
	    g_Font_PSP,
	    &g_RenderInterface,
	    &g_FontTexture_PSP,
	    "local:bootstrap_font.pfnt"
	);
}

static TBOOL LoadProbeModelFile_PSP()
{
	return g_ModelFactory_PSP.CreateModelFromFile( g_ProbeModel_PSP, "local:probe.pmes" );
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
	Toshi::TInputInterface_PSP oInputInterface;
	Toshi::TScheduler          oScheduler;
	Toshi::THPTimer            oFrameTimer;
	if ( !g_RenderInterface.CreateDisplay() )
	{
		sceKernelExitGame();
		return 1;
	}

	const TBOOL bInputReady = oInputInterface.Initialise();
	const TBOOL bToshiReady     = RunToshiPortabilitySmokeTest();
	const TBOOL bFileReady      = Toshi::TFileManager::Create();
	const TBOOL bTextureReady   = InitialiseProbeTexture( bFileReady );
	const TBOOL bFontTextureReady = bFileReady && LoadFontTextureFile_PSP();
	const TBOOL bFontReady      = bFontTextureReady && LoadFontFile_PSP();
	const TBOOL bModelReady     = bFileReady && LoadProbeModelFile_PSP();
	if ( bFileReady )
		Toshi::TFileManager::Destroy();
	const TBOOL bMeshReady      = InitialiseProbeMesh_PSP();
	const TBOOL bTextureCached  = bTextureReady && g_ResourceCache_PSP.Register(
	    "probe.texture",
	    Toshi::TRENDERRESOURCETYPE_PSP_TEXTURE,
	    &g_ProbeTexture
	);
	const TBOOL bModelCached = bModelReady && g_ResourceCache_PSP.Register(
	    "probe.model",
	    Toshi::TRENDERRESOURCETYPE_PSP_MODEL,
	    &g_ProbeModel_PSP
	);
	const TBOOL bFontTextureCached = bFontTextureReady && g_ResourceCache_PSP.Register(
	    "bootstrap.font.atlas",
	    Toshi::TRENDERRESOURCETYPE_PSP_TEXTURE,
	    &g_FontTexture_PSP
	);
	const TBOOL bFontCached = bFontReady && g_ResourceCache_PSP.Register(
	    "bootstrap.font",
	    Toshi::TRENDERRESOURCETYPE_PSP_FONT,
	    &g_Font_PSP
	);
	auto* pCachedTexture = static_cast<Toshi::T2Texture*>( g_ResourceCache_PSP.Find(
	    "PROBE.TEXTURE",
	    Toshi::TRENDERRESOURCETYPE_PSP_TEXTURE
	) );
	auto* pCachedModel = static_cast<Toshi::TModel_PSP*>( g_ResourceCache_PSP.Find(
	    "PROBE.MODEL",
	    Toshi::TRENDERRESOURCETYPE_PSP_MODEL
	) );
	auto* pCachedFontTexture = static_cast<Toshi::T2Texture*>( g_ResourceCache_PSP.Find(
	    "BOOTSTRAP.FONT.ATLAS",
	    Toshi::TRENDERRESOURCETYPE_PSP_TEXTURE
	) );
	auto* pCachedFont = static_cast<Toshi::T2GUIFont_PSP*>( g_ResourceCache_PSP.Find(
	    "BOOTSTRAP.FONT",
	    Toshi::TRENDERRESOURCETYPE_PSP_FONT
	) );
	const TBOOL bCacheReady = bTextureCached && bModelCached && bFontTextureCached && bFontCached &&
	                          pCachedTexture == &g_ProbeTexture &&
	                          pCachedModel == &g_ProbeModel_PSP &&
	                          pCachedFontTexture == &g_FontTexture_PSP &&
	                          pCachedFont == &g_Font_PSP;
	AFrontendRenderContext_PSP oRenderContext = {
		&g_RenderInterface,
		&g_ProbeVertices_PSP,
		&g_ProbeIndices_PSP,
		pCachedTexture,
		pCachedFont,
		pCachedModel,
		&g_ProbeMaterial_PSP,
		TFALSE,
		bMeshReady,
	};

	AApplication_PSP oApplication( &oInputInterface, &oScheduler, &oFrameTimer );
	const TBOOL bSchedulerReady = oApplication.Create( &oRenderContext );
	const TBOOL bBootstrapReady = bInputReady && bSchedulerReady && bToshiReady &&
	                              bTextureReady && bFontReady && bMeshReady && bModelReady && bCacheReady;
	oRenderContext.bBootstrapReady = bBootstrapReady;
	TDebug_FinalPrintf( "[PSP render] texture     %s\n", bTextureReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP render] source      %s\n", g_bExternalTexture_PSP ? "probe.ptex" : "embedded fallback" );
	TDebug_FinalPrintf( "[PSP render] layout      %s\n", g_ProbeTexture.IsSwizzled() ? "swizzled" : "linear" );
	TDebug_FinalPrintf( "[PSP render] font atlas  %s\n", bFontTextureReady ? "bootstrap_font.ptex" : "FAILED" );
	TDebug_FinalPrintf( "[PSP render] font data   %s\n", bFontReady ? "bootstrap_font.pfnt" : "FAILED" );
	TDebug_FinalPrintf( "[PSP render] index mesh  %s\n", bMeshReady ? "OK" : "FAILED" );
	TDebug_FinalPrintf( "[PSP render] model       %s\n", bModelReady ? "probe.pmes" : "FAILED" );
	TDebug_FinalPrintf( "[PSP render] cache       %s (%u/%u)\n", bCacheReady ? "OK" : "FAILED", g_ResourceCache_PSP.GetCount(), g_ResourceCache_PSP.GetCapacity() );
	TDebug_FinalPrintf( "[PSP frontend] scheduler %s\n", bSchedulerReady ? "OK" : "FAILED" );
	const Toshi::TRenderMemoryStats_PSP oMemoryStats = Toshi::TRenderMemory_PSP::GetStats();
	TDebug_FinalPrintf(
	    "[PSP memory] main %u/%u peak %u, edram %u/%u\n",
	    TUINT( oMemoryStats.uiMainCurrent ),
	    TUINT( oMemoryStats.uiMainBudget ),
	    TUINT( oMemoryStats.uiMainPeak ),
	    TUINT( oMemoryStats.uiEDRAMCurrent ),
	    TUINT( oMemoryStats.uiEDRAMBudget )
	);

	while ( g_Running && oApplication.Update() ) {}

	oApplication.Destroy();
	oInputInterface.Deinitialise();
	g_ResourceCache_PSP.Clear();
	g_ProbeModel_PSP.Unload();
	g_Font_PSP.Destroy();
	g_FontTexture_PSP.Unload();
	g_ProbeTexture.Unload();
	g_ProbeIndices_PSP.Destroy();
	g_ProbeVertices_PSP.Destroy();
	g_RenderInterface.DestroyDisplay();
	sceKernelExitGame();
	return 0;
}
