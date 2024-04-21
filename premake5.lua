project "reactphysics3d"
    kind 'StaticLib'
    language "C++"
    cppdialect "C++20"
    staticruntime "on"
    warnings "off"

    targetdir ("bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("bin-int/" .. OutputDir .. "/%{prj.name}")

    files {
        "/include/**.h",
        "/src/**.cpp"
    }

    sysincludedirs {
        "/include/"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter "configurations:Production"
        runtime "Release"
        optimize "on"
