local pspdev = os.getenv( "PSPDEV" )

if pspdev == nil then
	error( "PSPDEV is not set; install PSPDEV and export PSPDEV before generating the PSP project" )
end

premake.api.addAllowed( "architecture", "PSP" )

workspace "OpenBarnyardPSP"
	location "../../Build/PSP"
	configurations { "Debug", "Release", "Final" }
	platforms "PSP"
	architecture "PSP"
	startproject "OpenBarnyardPSPBootstrap"

project "OpenBarnyardPSPBootstrap"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	toolset "gcc"
	gccprefix "psp-"
	targetname "openbarnyard_psp"
	targetextension ".elf"

	targetdir ( "../../bin/PSP/%{cfg.buildcfg}" )
	objdir ( "../../bin-int/PSP/%{cfg.buildcfg}" )

	files
	{
		"Source/**.h",
		"Source/**.cpp",
		"../../Toshi/Source/File/TFile.cpp",
		"../../Toshi/Source/File/TFileManager.cpp",
		"../../Toshi/Source/File/TFileSystem.cpp",
		"../../Toshi/Source/Input/TInputDevice.cpp",
		"../../Toshi/Source/Input/TInputDeviceController.cpp",
		"../../Toshi/Source/Input/TInputInterface.cpp",
		"../../Toshi/Source/Toshi/T2RedBlackTree.cpp",
		"../../Toshi/Source/Toshi/T2String8.cpp",
		"../../Toshi/Source/Toshi/TClass.cpp",
		"../../Toshi/Source/Toshi/TDList.cpp",
		"../../Toshi/Source/Toshi/TEvent.cpp",
		"../../Toshi/Source/Toshi/TObject.cpp",
		"../../Toshi/Source/Toshi/TPriList.cpp",
		"../../Toshi/Source/Toshi/TScheduler.cpp",
		"../../Toshi/Source/Toshi/TString8.cpp",
		"../../Toshi/Source/Toshi/TTask.cpp",
		"../../Toshi/Source/Platform/PSP/**.h",
		"../../Toshi/Source/Platform/PSP/**.cpp"
	}

	includedirs
	{
		".",
		"../../Toshi/Source",
		pspdev .. "/psp/include",
		pspdev .. "/psp/sdk/include"
	}

	libdirs
	{
		pspdev .. "/psp/lib",
		pspdev .. "/psp/sdk/lib"
	}

	defines
	{
		"TOSHI_SKU_PSP",
		"TRENDERINTERFACE_PSP",
		"_PSP_FW_VERSION=660"
	}

	buildoptions
	{
		"-G0",
		"-Wall",
		"-Wextra",
		"-ffunction-sections",
		"-fdata-sections",
		"-fno-exceptions",
		"-fno-rtti"
	}

	linkoptions
	{
		"-G0",
		"-Wl,-zmax-page-size=128"
	}

	links
	{
		"pspaudio",
		"pspgum",
		"pspgu",
		"pspge",
		"pspdisplay",
		"pspctrl",
		"m",
		"pspdebug",
		"pspnet",
		"pspnet_apctl"
	}

	postbuildcommands
	{
		'psp-fixup-imports "%{cfg.buildtarget.abspath}"',
		'psp-strip "%{cfg.buildtarget.abspath}" -o "%{cfg.targetdir}/openbarnyard_psp_strip.elf"',
		'mksfoex -d MEMSIZE=2 "OpenBarnyard PSP" "%{cfg.targetdir}/PARAM.SFO"',
		'pack-pbp "%{cfg.targetdir}/EBOOT.PBP" "%{cfg.targetdir}/PARAM.SFO" "../../Ports/PSP/Assets/ICON0.PNG" NULL NULL "../../Ports/PSP/Assets/PIC1.PNG" NULL "%{cfg.targetdir}/openbarnyard_psp_strip.elf" NULL',
		'cp -f "../../Ports/PSP/Assets/probe.ptex" "%{cfg.targetdir}/probe.ptex"',
		'cp -f "../../Ports/PSP/Assets/probe.pmes" "%{cfg.targetdir}/probe.pmes"',
		'cp -f "../../Ports/PSP/Assets/probe_skinned.pmes" "%{cfg.targetdir}/probe_skinned.pmes"',
		'cp -f "../../Ports/PSP/Assets/probe.pskl" "%{cfg.targetdir}/probe.pskl"',
		'cp -f "../../Ports/PSP/Assets/probe.panm" "%{cfg.targetdir}/probe.panm"',
		'cp -f "../../Ports/PSP/Assets/probe.pwld" "%{cfg.targetdir}/probe.pwld"',
		'cp -f "../../Ports/PSP/Assets/bootstrap_font.ptex" "%{cfg.targetdir}/bootstrap_font.ptex"',
		'cp -f "../../Ports/PSP/Assets/bootstrap_font.pfnt" "%{cfg.targetdir}/bootstrap_font.pfnt"',
		'cp -f "../../Ports/PSP/Assets/bootstrap_locale.ploc" "%{cfg.targetdir}/bootstrap_locale.ploc"',
		'cp -f "../../Ports/PSP/Assets/bootstrap_audio.ppcm" "%{cfg.targetdir}/bootstrap_audio.ppcm"',
		'cp -f "../../Ports/PSP/Assets/bootstrap_audio.pbnk" "%{cfg.targetdir}/bootstrap_audio.pbnk"',
		'cp -f "../../Ports/PSP/Assets/ui_menuclick.ppcm" "%{cfg.targetdir}/ui_menuclick.ppcm"',
		'cp -f "../../Ports/PSP/Assets/ui_menuback.ppcm" "%{cfg.targetdir}/ui_menuback.ppcm"',
		'cp -f "../../Ports/PSP/Assets/ui_menuok.ppcm" "%{cfg.targetdir}/ui_menuok.ppcm"',
		'cp -f "../../Ports/PSP/Assets/bootstrap_stream.ppcm" "%{cfg.targetdir}/bootstrap_stream.ppcm"',
		'cp -f "../../Ports/PSP/Assets/bootstrap_stream.padp" "%{cfg.targetdir}/bootstrap_stream.padp"',
		'rm -f "%{cfg.targetdir}/openbarnyard_psp_strip.elf"'
	}

	filter "configurations:Debug"
		defines "TOSHI_DEBUG"
		symbols "On"

	filter "configurations:Release"
		defines "TOSHI_RELEASE"
		optimize "On"

	filter "configurations:Final"
		defines "TOSHI_FINAL"
		optimize "On"

	filter {}

project "OpenBarnyardPSPGameSlice"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	toolset "gcc"
	gccprefix "psp-"
	targetdir ( "../../bin/PSP/%{cfg.buildcfg}" )
	objdir ( "../../bin-int/PSPGameSlice/%{cfg.buildcfg}" )

	files
	{
		"../../OpenBarnyard/Source/GameInterface/ABYardMenuState.cpp",
		"../../OpenBarnyard/Source/GameInterface/AMovieState.cpp",
		"../../OpenBarnyard/Source/GameInterface/ASlideshowState.cpp",
		"../../OpenBarnyard/Source/GameInterface/AGameState.cpp",
		"../../OpenBarnyard/Source/GUI/AGUI2Button.cpp",
		"../../OpenBarnyard/Source/GUI/AGUI2Dialog.cpp",
		"../../OpenBarnyard/Source/GUI/AGUI2Menu.cpp",
		"../../OpenBarnyard/Source/GUI/AGUI2MenuItem.cpp",
		"../../OpenBarnyard/Source/GUI/AGUI2MouseCursor.cpp",
		"../../OpenBarnyard/Source/GUI/AGUI2TextBox.cpp",
		"../../OpenBarnyard/Source/GUI/AFade.cpp",
		"../../OpenBarnyard/Source/GUI/AFadeManager.cpp",
		"../../OpenBarnyard/Source/GUI/AGUISlideshow.cpp",
		"../../OpenBarnyard/Source/Input/AInputMap.cpp",
		"../../OpenBarnyard/Source/Input/AInputMapManager.cpp",
		"../../OpenBarnyard/Source/Platform/PSP/AGUI2_PSP.cpp",
		"../../OpenBarnyard/Source/Platform/PSP/AGUI2Renderer_PSP.cpp",
		"../../OpenBarnyard/Source/Platform/PSP/AFrontEndTerrain_PSP.cpp",
		"../../OpenBarnyard/Source/Platform/PSP/AFrontEndMainMenuState2_PSP.cpp",
		"../../OpenBarnyard/Source/Platform/PSP/AGUIMatlibPicture_PSP.cpp",
		"../../OpenBarnyard/Source/Platform/PSP/AGUIPicture_PSP.cpp",
		"../../OpenBarnyard/Source/Platform/PSP/AGUISystem_PSP.cpp",
		"../../OpenBarnyard/Source/Platform/PSP/AGameRuntimeReport_PSP.cpp",
		"../../OpenBarnyard/Source/Platform/PSP/AGameStateController_PSP.cpp",
		"../../OpenBarnyard/Source/Platform/PSP/ARenderer_PSP.cpp",
		"../../OpenBarnyard/Source/Platform/PSP/ASoundManager_PSP.cpp",
		"../../OpenBarnyard/Source/Platform/PSP/ADisplayModes_PSP.cpp",
		"../../OpenBarnyard/Source/Platform/PSP/AMoviePlayer_PSP.cpp",
		"../../OpenBarnyard/Source/Platform/PSP/AppBoot_PSP.cpp",
		"../../OpenBarnyard/Source/Platform/PSP/AppMain_PSP.cpp",
		"../../OpenBarnyard/Source/Tasks/ARootTask.cpp"
	}

	includedirs
	{
		"../../OpenBarnyard/Source",
		"../../Shared/Source",
		"../../Toshi/Source",
		pspdev .. "/psp/include",
		pspdev .. "/psp/sdk/include"
	}

	defines
	{
		"TOSHI_SKU_PSP",
		"TRENDERINTERFACE_PSP",
		"BARNYARD_PSP_FRONTEND_SLICE",
		"_PSP_FW_VERSION=660"
	}

	buildoptions
	{
		"-G0",
		"-Wall",
		"-Wextra",
		"-ffunction-sections",
		"-fdata-sections",
		"-fno-exceptions",
		"-fno-rtti"
	}

	filter "configurations:Debug"
		defines "TOSHI_DEBUG"
		symbols "On"

	filter "configurations:Release"
		defines "TOSHI_RELEASE"
		optimize "On"

	filter "configurations:Final"
		defines "TOSHI_FINAL"
		optimize "On"

	filter {}

project "OpenBarnyardPSPGame"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	toolset "gcc"
	gccprefix "psp-"
	targetname "openbarnyard_psp_game"
	targetextension ".elf"

	targetdir ( "../../bin/PSP/%{cfg.buildcfg}" )
	objdir ( "../../bin-int/PSPGame/%{cfg.buildcfg}" )

	files
	{
		"../../OpenBarnyard/Source/AOptions.cpp",
		"../../OpenBarnyard/Source/APooledStrings.cpp",
		"../../OpenBarnyard/Source/ARandom.cpp",
		"../../OpenBarnyard/Source/CVar/AVarType.cpp",
		"../../OpenBarnyard/Source/Locale/ALocale.cpp",
		"../../OpenBarnyard/Source/Locale/ALocaleManager.cpp",
		"../../OpenBarnyard/Source/Memory/AMemory.cpp",
		"../../OpenBarnyard/Source/Memory/AMemoryPoolAllocator.cpp",
		"../../OpenBarnyard/Source/Movie/AMoviePlayer.cpp",
		"../../OpenBarnyard/Source/Platform/PSP/Input/AInputHandler_PSP.cpp",
		"../../OpenBarnyard/Source/Tasks/ADummyTask.cpp",
		"../../Toshi/Source/Core/TMemory.cpp",
		"../../Toshi/Source/File/TFile.cpp",
		"../../Toshi/Source/File/TFileLexer.cpp",
		"../../Toshi/Source/File/TFileManager.cpp",
		"../../Toshi/Source/File/TFileSystem.cpp",
		"../../Toshi/Source/File/TLogFile.cpp",
		"../../Toshi/Source/File/TTRB.cpp",
		"../../Toshi/Source/File/TTSF.cpp",
		"../../Toshi/Source/File/TCompress_Decompress.cpp",
		"../../Toshi/Source/Input/TInputDevice.cpp",
		"../../Toshi/Source/Input/TInputDeviceController.cpp",
		"../../Toshi/Source/Input/TInputDeviceKeyboard.cpp",
		"../../Toshi/Source/Input/TInputDeviceMouse.cpp",
		"../../Toshi/Source/Input/TInputInterface.cpp",
		"../../Toshi/Source/GUI/T2GUIContext.cpp",
		"../../Toshi/Source/GUI/T2GUIElement.cpp",
		"../../Toshi/Source/GUI/T2GUIFont.cpp",
		"../../Toshi/Source/GUI/T2GUIFontManager.cpp",
		"../../Toshi/Source/GUI/T2GUIPolygon.cpp",
		"../../Toshi/Source/GUI/T2GUIRectangle.cpp",
		"../../Toshi/Source/GUI/T2GUITextureSection.cpp",
		"../../Toshi/Source/GUI/T2GUITextureSectionManager.cpp",
		"../../Toshi/Source/GUI/T2GUITransform.cpp",
		"../../Toshi/Source/Plugins/PPropertyParser/PProperties.cpp",
		"../../Toshi/Source/Plugins/PPropertyParser/PPropertyBlock.cpp",
		"../../Toshi/Source/Plugins/PPropertyParser/PPropertyName.cpp",
		"../../Toshi/Source/Plugins/PPropertyParser/PPropertyValue.cpp",
		"../../Toshi/Source/Plugins/PPropertyParser/PPropertyValueArray.cpp",
		"../../Toshi/Source/Plugins/PLexerTRB.cpp",
		"../../Toshi/Source/Math/**.cpp",
		"../../Toshi/Source/Render/**.cpp",
		"../../Toshi/Source/T2Locale/T2Locale.cpp",
		"../../Toshi/Source/Toshi/T2Allocator.cpp",
		"../../Toshi/Source/Toshi/T2RedBlackTree.cpp",
		"../../Toshi/Source/Toshi/T2String8.cpp",
		"../../Toshi/Source/Toshi/TApplication.cpp",
		"../../Toshi/Source/Toshi/TClass.cpp",
		"../../Toshi/Source/Toshi/TDList.cpp",
		"../../Toshi/Source/Toshi/TEvent.cpp",
		"../../Toshi/Source/Toshi/TFreeList.cpp",
		"../../Toshi/Source/Toshi/TObject.cpp",
		"../../Toshi/Source/Toshi/TPriList.cpp",
		"../../Toshi/Source/Toshi/TPString8.cpp",
		"../../Toshi/Source/Toshi/TScheduler.cpp",
		"../../Toshi/Source/Toshi/TString8.cpp",
		"../../Toshi/Source/Toshi/TSystem.cpp",
		"../../Toshi/Source/Toshi/TTask.cpp",
		"../../Toshi/Source/Toshi/TUtil.cpp",
		"../../Toshi/Source/ToshiTools/T2DynamicVector.cpp",
		"../../Toshi/Source/Platform/PSP/**.h",
		"../../Toshi/Source/Platform/PSP/**.cpp"
	}

	removefiles
	{
		"../../Toshi/Source/Platform/PSP/T2Allocator_PSP.cpp",
		"../../Toshi/Source/Platform/PSP/TMemory_PSP.cpp"
	}

	includedirs
	{
		"../../OpenBarnyard/Source",
		"../../Shared/Source",
		"../../Toshi/Source",
		pspdev .. "/psp/include",
		pspdev .. "/psp/sdk/include"
	}

	libdirs
	{
		pspdev .. "/psp/lib",
		pspdev .. "/psp/sdk/lib"
	}

	defines
	{
		"TOSHI_SKU_PSP",
		"TRENDERINTERFACE_PSP",
		"BARNYARD_PSP_FRONTEND_SLICE",
		"TOSHI_NO_LOGS",
		"_PSP_FW_VERSION=660"
	}

	buildoptions
	{
		"-G0",
		"-Wall",
		"-Wextra",
		"-ffunction-sections",
		"-fdata-sections",
		"-fno-exceptions",
		"-fno-rtti"
	}

	linkoptions
	{
		"-G0",
		"-Wl,--gc-sections",
		"-Wl,-T,../../Ports/PSP/psp-retain-imports.ld",
		"-Wl,-zmax-page-size=128",
		"-Wl,-Map=%{cfg.targetdir}/openbarnyard_psp_game.map"
	}

	links
	{
		"OpenBarnyardPSPGameSlice",
		"pspaudio",
		"pspgum",
		"pspgu",
		"pspge",
		"pspdisplay",
		"pspctrl",
		"m",
		"pspdebug"
	}

	postbuildcommands
	{
		'psp-fixup-imports "%{cfg.buildtarget.abspath}"',
		'psp-strip "%{cfg.buildtarget.abspath}" -o "%{cfg.targetdir}/openbarnyard_psp_game_strip.elf"',
		'mksfoex -d MEMSIZE=2 "OpenBarnyard PSP" "%{cfg.targetdir}/PARAM_GAME.SFO"',
		'pack-pbp "%{cfg.targetdir}/EBOOT_GAME.PBP" "%{cfg.targetdir}/PARAM_GAME.SFO" "../../Ports/PSP/Assets/ICON0.PNG" NULL NULL "../../Ports/PSP/Assets/PIC1.PNG" NULL "%{cfg.targetdir}/openbarnyard_psp_game_strip.elf" NULL',
		'rm -f "%{cfg.targetdir}/openbarnyard_psp_game_strip.elf"'
	}

	filter "configurations:Debug"
		defines "TOSHI_DEBUG"
		symbols "On"

	filter "configurations:Release"
		defines "TOSHI_RELEASE"
		optimize "On"

	filter "configurations:Final"
		defines "TOSHI_FINAL"
		optimize "On"

	filter {}
