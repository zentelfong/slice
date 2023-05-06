local ARM_COMPILER_DIR = "../../autodrive-arm/gcc-linaro-6.5.0-2018.12-x86_64_aarch64-linux-gnu"

workspace "slice"
  configurations { "Debug", "Release" }
  platforms { "Win32", "Win64", "Linux", "Arm" }
  location "build"
  targetdir "bin"
  cppdialect "C++14"

filter { "platforms:Win32" }
  system "windows"
  architecture "x32"
  defines {
    "_WIN32", 
    "WIN32",
    "NOMINMAX",
    "_CRT_SECURE_NO_WARNINGS",
    "_WIN32_WINNT=0x0601"
  }
  staticruntime "on"


filter { "platforms:Win64" }
  system "windows"
  architecture "x64"	
  defines { "_WIN32", "WIN32" ,"NOMINMAX","_CRT_SECURE_NO_WARNINGS"}

filter { "platforms:Linux" }
  system "linux"
  architecture "x64"	
  defines { "LINUX", "linux" ,"POSIX"}


filter { "platforms:Arm" }
  system "linux"
  architecture "ARM64"
	gccprefix (ARM_COMPILER_DIR.."/bin/aarch64-linux-gnu-")
	defines { "LINUX", "linux" ,"POSIX" ,"ARM64" , "ARM"}


filter { "platforms:Win*" }
  disablewarnings {
    "4125","4127","4189","4201","4244","4245","4251","4267",
    "4291","4310","4324","4389","4456","4457","4459","4505",
    "4554","4589","4611","4701","4702","4703","4706","4800",
    "4200","5030","4996"
  }

filter "configurations:Debug"
  defines { "DEBUG" , "_DEBUG"}
  symbols "On"
  optimize "Debug"

filter "configurations:Release"
  defines { "NDEBUG" }
  symbols "Off"
  optimize "Speed"


project "slice_test"
  language "C++"
  kind "ConsoleApp"
  includedirs{
    "./include",
    "./"
  }
  files{
    "include/*.h",
    "src/*.cpp",
    "./test.cpp"
  }
  links {
    "gtest",
    "gtest_main",
    "pthread"
  }

