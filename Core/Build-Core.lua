
include "Raylib.lua"

glm_dir = "Vendor/Sources/glm-master"

raygui_dir = "Vendor/Sources/raygui-master"

project "Core"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"
   targetdir "Binaries/%{cfg.buildcfg}"
   staticruntime "off"

   files { "Source/**.h", "Source/**.cpp" }

   includedirs
   {
      "Source"
   }

   targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
   objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")
   dependson {"raylib"}
   links {
          "raylib.lib",
          "winmm"
         }
   
   libdirs {"../Binaries/Dependencies/%{cfg.buildcfg}"}
   
   includedirs {"../" .. raylib_dir .. "/src"}
   includedirs {"../" .. raylib_dir .."/src/external"}
   includedirs {"../" .. raylib_dir .."/src/external/glfw/include"}
   includedirs {"../" .. glm_dir .."/glm"}
   includedirs {"../" .. raygui_dir .."/src"}
   includedirs {"../Vendor/Sources/nlohmann"}
   
   filter "action:vs*"
       defines{"_WINSOCK_DEPRECATED_NO_WARNINGS", "_CRT_SECURE_NO_WARNINGS"}
       characterset ("Unicode")

   filter "system:windows"
       systemversion "latest"
       defines { }

   filter "configurations:Debug"
       defines { "DEBUG" }
       runtime "Debug"
       symbols "On"

   filter "configurations:Release"
       defines { "RELEASE" }
       runtime "Release"
       optimize "On"
       symbols "On"

   filter "configurations:Dist"
       defines { "DIST" }
       runtime "Release"
       optimize "On"
       symbols "Off"