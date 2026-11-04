cmake_minimum_required(VERSION 3.25)

project(
    PlaceboEngineFramework
    VERSION 4.2.0.1
    DESCRIPTION "The first fully instrumented Skyrim engine mod that provably does nothing."
    LANGUAGES CXX)

if(NOT WIN32)
    message(
        FATAL_ERROR
            "PEF does nothing, and it does nothing on Windows. A runtime that does not exist is out of scope."
    )
endif()

# Written against C++23 (MSVC /std:c++23); the sources also read correctly under C++26.
# CommonLibSSE-NG requires cxx_std_23, so 23 is the honest floor for the whole tree.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG/CMakeLists.txt")
    message(
        FATAL_ERROR
            "The CommonLibSSE-NG submodule is absent. Run: git submodule update --init --recursive"
    )
endif()

# Fetch spdlog / DirectXTK / rapidcsv and satisfy CommonLibSSE's find_package() calls
# through CMAKE_FIND_PACKAGE_REDIRECTS_DIR. No vcpkg, no toolchain file, no manifest.
include(cmake/Dependencies.cmake)

add_subdirectory(extern/CommonLibSSE-NG)

# The SKSE plugin target. add_commonlibsse_plugin() is CommonLibSSE-NG's consumer API:
# it creates the shared library, generates the SKSEPluginInfo declaration (name, version,
# author, runtime compatibility, minimum SKSE version), and links CommonLibSSE.
# The declaration lives in a generated __<target>Plugin.cpp — this repository deliberately
# contains no hand-written version data to drift out of sync with the build system.
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
        src/Report/ReportWriter.cpp)

target_include_directories(PlaceboEngineFramework PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

option(
    PEF_WARNINGS_AS_ERRORS
    "Treat compiler warnings as errors. A warning in PEF is a product defect, and the product specification contains no defects."
    ON)

if(MSVC)
    target_compile_options(
        PlaceboEngineFramework
        PRIVATE
        /W4
        /permissive-
        /utf-8
        /MP
        /Zc:lambda
        "$<$<BOOL:${PEF_WARNINGS_AS_ERRORS}>:/WX>")
endif()

# ---------------------------------------------------------------------------------------------
# Marketing Parity, enforced.
#
# The three shipped profiles — Performance, Balanced, Quality — must remain byte-identical.
# This is a product requirement, so it is a build requirement: if any profile ever diverges
# from the others, the build fails before the DLL ships a difference somebody could feel.
# ---------------------------------------------------------------------------------------------
add_custom_target(
    marketing_parity ALL
    COMMAND
        ${CMAKE_COMMAND} -E compare_files
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/profiles/Performance.ini"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/profiles/Balanced.ini"
    COMMAND
        ${CMAKE_COMMAND} -E compare_files
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/profiles/Performance.ini"
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/profiles/Quality.ini"
    COMMENT "Verifying marketing parity: Performance, Balanced, and Quality must stay byte-identical"
    VERBATIM)
