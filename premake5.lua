dependencies = {
	basePath = "./deps"
}

function dependencies.load()
	dir = path.join(dependencies.basePath, "premake/*.lua")
	deps = os.matchfiles(dir)
	for i, dep in pairs(deps) do
		dep = dep:gsub(".lua", "")
		require(dep)
	end
end

function dependencies.imports()
	for i, proj in pairs(dependencies) do
		if type(i) == 'number' then
			proj.import()
		end
	end
end

function dependencies.projects()
    for i, proj in pairs(dependencies) do
        if type(i) == 'number' and proj.project then  -- Check if 'project' function exists
            proj.project()  -- Only call if the project function exists
        end
    end
end


newoption {
	trigger = "copy-to",
	description = "Optional, copy the EXE to a custom folder after build, define the path here if wanted.",
	value = "PATH"
}

newoption {
	trigger = "dev-build",
	description = "Enable development builds of the client."
}

dependencies.load()

-- The target platform is picked by the premake action itself:
--   premake5 vs2022   -> Windows (MSVC, vendored Qt6 from deps/qt6)
--   premake5 gmake2   -> Linux (system Qt6, GCC/Clang)
local is_linux = os.target() == "linux"

-- Define the main project 

workspace "Shield_launcher"

	startproject "launcher"
	location "./build"
	objdir "%{wks.location}/obj"
	targetdir "%{wks.location}/bin/%{cfg.platform}/%{cfg.buildcfg}"

	configurations {"Debug", "Release"}

	language "C++"
	cppdialect "C++20"

	architecture "x86_64"
	platforms "x64"

	symbols "On"
	editandcontinue "Off"
	warnings "Extra"

	if _OPTIONS["dev-build"] then
		defines {"DEV_BUILD"}
	end

	if os.getenv("CI") then
		defines {"CI"}
	end

	-- legacy MSVC-only knobs, present in some premake forks but not in stock
	-- premake5; call them only when the API exposes them
	if incrementallink then incrementallink "Off" end
	if minimalrebuild then minimalrebuild "Off" end
	if multiprocessorcompile then multiprocessorcompile "On" end
	if enable64bitchecks then enable64bitchecks "Off" end

	filter "system:windows"
		defines {"_WINDOWS", "WIN32"}
		staticruntime "off"
		runtime "Release"
		systemversion "latest"
		characterset "ASCII"
	filter {}

	filter "system:linux"
		defines {"LINUX_PORT"}
		buildoptions { "-fPIC", "-fno-omit-frame-pointer" }
	filter {}

	filter {"system:windows", "configurations:Release"}
		optimize "Size"
		buildoptions {"/GL", "/Zc:__cplusplus"}
		linkoptions { "/IGNORE:4702", "/LTCG" }
		defines {"NDEBUG"}
	filter {}

	filter {"system:windows", "configurations:Debug"}
		--optimize "Debug"
		--defines {"DEBUG", "_DEBUG"}
                runtime "Debug"
		defines { "_DEBUG" }
                buildoptions { "/Zc:__cplusplus" }
   	        symbols "On"
	filter {}

	filter {"system:linux", "configurations:Release"}
		optimize "Speed"
		buildoptions { "-O2" }
		defines {"NDEBUG"}
	filter {}

	filter {"system:linux", "configurations:Debug"}
		optimize "Debug"
		defines { "_DEBUG" }
	filter {}

	
-- Define the utlis project
project "utilities"
	kind "StaticLib"
	language "C++"

	files {"./source/utilities/**.hpp", "./source/utilities/**.cpp"}

	includedirs {"./source/utilities", "%{prj.location}/source"}

	-- nt.cpp / io.cpp / configuration.cpp are Win32 NT + registry helpers only
	-- used by the Windows updater.
	filter "system:linux"
		removefiles {"./source/utilities/nt.cpp", "./source/utilities/io.cpp", "./source/utilities/configuration.cpp"}
	filter {}

	dependencies.imports()
	
	
-- Define the server project
project "launcher"
    kind "WindowedApp"
    language "C++"
    targetname "Shield_Launcher"
    cppdialect "C++20"

    pchheader "std_include.hpp"
    pchsource "source/launcher/std_include.cpp"
    
    includedirs
    {
        "source/launcher",
        "source/utilities", 
	    "build/src/", -- for pre generated qt moc files [NEEDED]
        "%{prj.location}/source"
    }

    files 
    {
        "./source/launcher/resources.rc",
        "./source/launcher/**.hpp",
        "./source/launcher/**.cpp",
        "./source/launcher/**.h",
        "./source/launcher/**.ui",
        "./source/launcher/**.qrc",
        "./source/launcher/**.ico",
        "./source/launcher/**.manifest",
        "build/src/**.cpp"
    }

    -- Windows resource scripts and manifests have no meaning on Linux; the
    -- Linux build uses the system Qt6 instead of the vendored Windows one.
    filter "system:linux"
        removefiles {"./source/launcher/resources.rc", "./source/launcher/**.manifest", "./source/launcher/**.ico"}
        includedirs
        {
            path.getabsolute("build/src/server/gui/tabs"),
            "/usr/include/qt6",
            "/usr/include/qt6/QtCore",
            "/usr/include/qt6/QtGui",
            "/usr/include/qt6/QtWidgets",
            "/usr/include/qt6/QtMultimedia",
        }
        links { "Qt6Core", "Qt6Gui", "Qt6Widgets", "Qt6Multimedia" }
    filter {}

    links {
        "utilities",  -- Links with utilities
    }
	
    postbuildcommands {
 
    }

    -- Apply precompiled headers to moc files
    --filter "files:src/launcher/launcher/**.cpp"
    filter "files:src/launcher/main/**.cpp"
    -- pchheader "std_include.hpp"
    -- pchsource "src/launcher/std_include.cpp"

    filter {}
	
	
	qt6.run_moc()  -- Func to run moc NEEDED this builds the moc file (moc is in qt.lua)

	
    dependencies.imports()

group "Dependencies"
    dependencies.projects()

-- Define the launcher shortcut project (Windows only: it is a tiny Win32 stub
-- that launches Shield_Launcher.exe from the game folder)
if not is_linux then
project "launcher_shortcut"
    kind "WindowedApp"
    language "C++"
    targetname "Launch Project BO4"
    cppdialect "C++20"
    
    files 
    {
        "./source/launcher_shortcut/**.cpp",
        "./source/launcher_shortcut/**.rc",
        "./source/launcher_shortcut/**.ico"
    }
    
    includedirs
    {
        "source/launcher_shortcut"
    }
    
    -- Copy the shortcut executable to the root directory after build
    postbuildcommands {
        "{COPY} \"%{cfg.buildtarget.abspath}\" \"%{wks.location}/../../\""
    }
    
    filter "configurations:Release"
        optimize "Size"
        defines {"NDEBUG"}
    filter {}
    
    filter "configurations:Debug"
        defines {"_DEBUG"}
        symbols "On"
    filter {}
end
