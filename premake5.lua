-- premake5.lua
--
-- Workspace layout:
--   Engine   (StaticLib)  - src/engine  - all DSP/config/sequencing/variation code
--   DemoApp  (ConsoleApp) - src/app     - links Engine, plays the demo composition
--   Tests    (ConsoleApp) - tests       - links Engine, doctest-based unit tests
--
-- Target: Visual Studio 2026, Windows/x64 only for this pass. See README.md
-- for notes on premake's VS action support and the vs2022-fallback plan if
-- a literal "vs2026" action isn't available in the installed premake5 build.

workspace "ProceduralMusicGenerator"
    configurations { "Debug", "Release" }
    platforms { "x64" }
    architecture "x86_64"
    location "build"
    startproject "DemoApp"

    filter "configurations:Debug"
        symbols "On"
        optimize "Off"
        defines { "PMG_DEBUG" }

    filter "configurations:Release"
        optimize "On"

    filter {}

-- Shared settings applied identically to every project in this workspace.
-- CPP_DIALECT is a single knob: first-pass engine code deliberately avoids
-- any C++20-exclusive feature (no concepts/modules/jthread) so dropping this
-- to "C++17" is a safe one-line fallback if the VS2026 toolset needs it.
local CPP_DIALECT = "C++20"

local function applyCommonSettings()
    language "C++"
    cppdialect(CPP_DIALECT)
    staticruntime "off"      -- consistent /MD across all projects; avoids CRT-linkage mismatches
    warnings "Extra"
    defines { "NOMINMAX", "WIN32_LEAN_AND_MEAN" }
    includedirs { "src/engine/include", "third_party" }
    targetdir "build/bin/%{cfg.buildcfg}/%{prj.name}"
    objdir "build/obj/%{cfg.buildcfg}/%{prj.name}"
end

project "Engine"
    kind "StaticLib"
    applyCommonSettings()
    files {
        "src/engine/include/engine/**.h",
        "src/engine/src/**.cpp",
    }

project "DemoApp"
    kind "ConsoleApp"
    applyCommonSettings()
    files { "src/app/**.cpp" }
    links { "Engine" }
    debugdir "%{wks.location}/../assets"

project "Tests"
    kind "ConsoleApp"
    applyCommonSettings()
    files { "tests/**.cpp" }
    links { "Engine" }
    debugdir "%{wks.location}/../assets"
