project "reactphysics3d"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "on"
    warnings "off"

    targetdir ("bin/" .. OutputDir .. "/%{prj.name}")
    objdir ("bin-int/" .. OutputDir .. "/%{prj.name}")

    files
    {
        "include/**.h",
        "src/**.cpp"
    }

    includedirs 
    {
        "include/"
    }


    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        defines "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"
