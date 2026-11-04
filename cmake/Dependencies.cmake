# Only the dependencies required by the pinned CommonLibSSE-NG build are populated.
# No vcpkg, examples, benchmarks, test executables, or optional DirectXTK tools.
include(FetchContent)

# CommonLibSSE uses spdlog for its logging. Use its normal compiled static target,
# but none of its optional examples, benchmarks, tests, or install artifacts.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build dependencies as static libraries" FORCE)
set(SPDLOG_BUILD_ALL OFF CACHE BOOL "Build every spdlog artifact" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "Build spdlog examples" FORCE)
set(SPDLOG_BUILD_EXAMPLE_HO OFF CACHE BOOL "Build header-only spdlog example" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "Build spdlog tests" FORCE)
set(SPDLOG_BUILD_TESTS_HO OFF CACHE BOOL "Build header-only spdlog tests" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "Build spdlog benchmarks" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "Generate spdlog install rules" FORCE)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(spdlog)
if(NOT TARGET spdlog::spdlog)
    message(FATAL_ERROR "spdlog was fetched, but spdlog::spdlog is unavailable")
endif()

# CommonLibSSE uses find_package(spdlog CONFIG REQUIRED), including with FetchContent.
file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/spdlogConfig.cmake"
    "if(NOT TARGET spdlog::spdlog)\n  message(FATAL_ERROR \"Missing spdlog::spdlog target\")\nendif()\n"
)

# The pinned CommonLibSSE library unconditionally requests/links DirectXTK.
# Its monolithic library cannot be safely replaced by a fabricated empty target.
# Disable tools, XAudio, alternative input backends, and fuzzing, all of which are
# unrelated to this plugin. Core DirectXTK shaders remain part of that library.
set(BUILD_TOOLS OFF CACHE BOOL "Build DirectXTK XWBTool" FORCE)
set(BUILD_XAUDIO_WIN8 OFF CACHE BOOL "Build DirectXTK XAudio 2.8" FORCE)
set(BUILD_XAUDIO_WIN10 OFF CACHE BOOL "Build DirectXTK XAudio 2.9" FORCE)
set(BUILD_XAUDIO_REDIST OFF CACHE BOOL "Build DirectXTK XAudio redist" FORCE)
set(BUILD_GAMEINPUT OFF CACHE BOOL "Build DirectXTK GameInput" FORCE)
set(BUILD_WGI OFF CACHE BOOL "Build DirectXTK Windows.Gaming.Input" FORCE)
set(BUILD_XINPUT OFF CACHE BOOL "Build DirectXTK XInput" FORCE)
set(BUILD_FUZZING OFF CACHE BOOL "Build DirectXTK fuzzing targets" FORCE)

FetchContent_Declare(
    directxtk
    GIT_REPOSITORY https://github.com/microsoft/DirectXTK.git
    GIT_TAG oct2025
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(directxtk)

if(NOT TARGET Microsoft::DirectXTK)
    if(TARGET DirectXTK)
        add_library(Microsoft::DirectXTK ALIAS DirectXTK)
    else()
        message(FATAL_ERROR "DirectXTK was fetched but no DirectXTK library target exists")
    endif()
endif()

file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/directxtkConfig.cmake"
    "if(NOT TARGET Microsoft::DirectXTK)\n  message(FATAL_ERROR \"Missing Microsoft::DirectXTK target\")\nendif()\n"
)

# rapidcsv is needed for Skyrim VR's Address Library CSV files.
# Fetch its headers only: do not configure its upstream CMakeLists.txt.
FetchContent_Declare(
    rapidcsv
    GIT_REPOSITORY https://github.com/d99kris/rapidcsv.git
    GIT_TAG v9.07
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR __pef_headers_only_no_cmake
)
FetchContent_MakeAvailable(rapidcsv)
if(NOT EXISTS "${rapidcsv_SOURCE_DIR}/src/rapidcsv.h")
    message(FATAL_ERROR "rapidcsv header was not fetched")
endif()
set(RAPIDCSV_INCLUDE_DIRS "${rapidcsv_SOURCE_DIR}/src" CACHE PATH "rapidcsv headers" FORCE)
