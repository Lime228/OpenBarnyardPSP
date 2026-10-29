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
	startproject "OpenBarnyardPSP"

project "OpenBarnyardPSP"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	toolset "gcc"
	gccprefix "psp-"
	targetname "openbarnyard_psp_probe"
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
		"../../Toshi/Source/Toshi/T2String16.cpp",
		"../../Toshi/Source/Toshi/T2String8.cpp",
		"../../Toshi/Source/Toshi/TClass.cpp",
		"../../Toshi/Source/Toshi/TDList.cpp",
		"../../Toshi/Source/Toshi/TEvent.cpp",
		"../../Toshi/Source/Toshi/TObject.cpp",
		"../../Toshi/Source/Toshi/TPriList.cpp",
		"../../Toshi/Source/Toshi/TString8.cpp",
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
		'psp-strip "%{cfg.buildtarget.abspath}" -o "%{cfg.targetdir}/openbarnyard_psp_probe_strip.elf"',
		'mksfoex -d MEMSIZE=2 "OpenBarnyard PSP Probe" "%{cfg.targetdir}/PARAM.SFO"',
		'pack-pbp "%{cfg.targetdir}/EBOOT.PBP" "%{cfg.targetdir}/PARAM.SFO" NULL NULL NULL NULL NULL "%{cfg.targetdir}/openbarnyard_psp_probe_strip.elf" NULL',
		'cp -f "../../Ports/PSP/Assets/probe.ptex" "%{cfg.targetdir}/probe.ptex"',
		'cp -f "../../Ports/PSP/Assets/probe.pmes" "%{cfg.targetdir}/probe.pmes"',
		'rm -f "%{cfg.targetdir}/openbarnyard_psp_probe_strip.elf"'
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
