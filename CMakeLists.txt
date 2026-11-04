cmake_minimum_required(VERSION 3.25)

project(
    PlaceboEngineFramework
    VERSION 4.2.0.1
    DESCRIPTION "The first fully instrumented Skyrim engine mod that provably does nothing."
    LANGUAGES CXX
)

if(NOT WIN32)
    message(FATAL_ERROR "PlaceboEngineFramework targets Windows only.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# This is an SKSE DLL, not a collection of test programs or installable SDKs.
set(CMAKE_SKIP_INSTALL_RULES ON)
set(BUILD_TESTS OFF CACHE BOOL "Build dependency unit tests (Catch2)" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Build dependency tests" FORCE)

# CommonLibSSE features the plugin actually uses:
# - SE/AE/VR are all part of the advertised runtime support.
# - Trampoline patch-safety diagnostics are optional; the plugin scans E8 call sites itself.
# - Xbyak and the REX INI/JSON/TOML parser implementations are unused.
set(ENABLE_SKYRIM_SE ON CACHE BOOL "Support Skyrim SE" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "Support Skyrim AE" FORCE)
set(ENABLE_SKYRIM_VR ON CACHE BOOL "Support Skyrim VR" FORCE)
set(SKSE_SUPPORT_XBYAK OFF CACHE BOOL "Enable optional Xbyak hooks" FORCE)
option(PEF_PATCH_SAFETY "Enable CommonLibSSE's optional instruction-boundary diagnostics (fetches hde64)" OFF)
set(SKSE_SUPPORT_PATCH_SAFETY "${PEF_PATCH_SAFETY}" CACHE BOOL "Trampoline instruction-boundary diagnostics" FORCE)
set(REX_OPTION_INI OFF CACHE BOOL "Enable CommonLib REX INI parser" FORCE)
set(REX_OPTION_JSON OFF CACHE BOOL "Enable CommonLib REX JSON parser" FORCE)
set(REX_OPTION_TOML OFF CACHE BOOL "Enable CommonLib REX TOML parser" FORCE)

set(_commonlib_dir "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG")
if(NOT EXISTS "${_commonlib_dir}/CMakeLists.txt")
    message(FATAL_ERROR
        "CommonLibSSE-NG is missing. Run: git submodule update --init --recursive"
    )
endif()
if(NOT EXISTS "${_commonlib_dir}/extern/openvr/headers/openvr.h")
    message(FATAL_ERROR
        "The CommonLibSSE-NG openvr submodule is missing. Run: git submodule update --init --recursive"
    )
endif()

# Provide only CommonLibSSE-NG's unavoidable third-party dependencies.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Dependencies.cmake")

# EXCLUDE_FROM_ALL prevents unrelated subproject targets from entering the default build.
# CommonLibSSE itself still builds because the plugin links it.
add_subdirectory("${_commonlib_dir}" EXCLUDE_FROM_ALL)

# The pinned CommonLibSSE-NG source-build path does not include this helper itself.
# Without the include, add_commonlibsse_plugin() is an unknown CMake command.
include("${_commonlib_dir}/cmake/CommonLibSSE.cmake")

add_commonlibsse_plugin(
    PlaceboEngineFramework
    NAME "Placebo Engine Framework"
    AUTHOR "PEF Consortium"
    VERSION "${PROJECT_VERSION}"
    SOURCES
        src/Main.cpp
        src/Plugin.cpp
        src/Config/Settings.cpp
        src/Diagnostics/Log.cpp
        src/Diagnostics/Telemetry.cpp
        src/Hooks/CallSiteScanner.cpp
        src/Hooks/Installer.cpp
        src/Crash/AlibiHandler.cpp
        src/Audit/AuditThread.cpp
        src/Report/ReportWriter.cpp
)

target_include_directories(PlaceboEngineFramework PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(PlaceboEngineFramework PRIVATE cxx_std_23)

option(PEF_WARNINGS_AS_ERRORS "Treat warnings in PlaceboEngineFramework as errors" ON)
if(MSVC)
    target_compile_options(
        PlaceboEngineFramework PRIVATE
        /W4
        /permissive-
        /utf-8
        /MP
        /Zc:lambda
        "$<$<BOOL:${PEF_WARNINGS_AS_ERRORS}>:/WX>"
    )
endif()

# The three marketing profiles must remain byte-identical.
add_custom_target(
    marketing_parity ALL
    COMMAND "${CMAKE_COMMAND}" -E compare_files
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/profiles/Performance.ini"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/profiles/Balanced.ini"
    COMMAND "${CMAKE_COMMAND}" -E compare_files
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/profiles/Performance.ini"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/profiles/Quality.ini"
    COMMENT "Verifying marketing parity of all three profiles"
    VERBATIM
)
