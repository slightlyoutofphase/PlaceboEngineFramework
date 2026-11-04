# Placebo Engine Framework — dependency acquisition.
#
# No vcpkg. No manifest. No toolchain file. The entire third-party surface is fetched by
# CMake itself and handed to CommonLibSSE-NG's own find_package() calls through
# CMAKE_FIND_PACKAGE_REDIRECTS_DIR (CMake >= 3.24) — the documented mechanism for exactly
# this: satisfying a dependency tree from in-tree FetchContent targets.
#
# What CommonLibSSE-NG needs, and what this file provides:
#
#   find_package(spdlog CONFIG REQUIRED)     -> spdlog (FetchContent)
#   find_package(directxtk CONFIG REQUIRED)  -> DirectXTK (FetchContent)
#   find_path(RAPIDCSV_INCLUDE_DIRS ...)     -> rapidcsv (FetchContent, header-only)
#   hde64                                    -> fetched by CommonLibSSE-NG itself
#   openvr                                   -> a nested submodule of CommonLibSSE-NG
#
# The hde64 and openvr lines above are why `git submodule update --init --recursive`
# is a hard requirement: the dependency graph is honest, which is more than the product
# can say for itself.

include(FetchContent)

# ----------------------------------------------------------------------------------------------
# spdlog — structured logging backend used by CommonLibSSE (and by this plugin, through
# SKSE::log). FetchContent builds the compiled library and defines spdlog::spdlog; the
# redirect file below makes CommonLibSSE's find_package(spdlog CONFIG) resolve to it
# without an install step and without a package manager.
# ----------------------------------------------------------------------------------------------
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(spdlog)

file(
    WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/spdlogConfig.cmake"
    [[if(NOT TARGET spdlog::spdlog)
    message(FATAL_ERROR "spdlog was fetched, but the target spdlog::spdlog does not exist.")
endif()]]
)

# ----------------------------------------------------------------------------------------------
# DirectXTK — texture and audio loading used inside CommonLibSSE. The upstream project
# defines the target `DirectXTK`; CommonLibSSE links `Microsoft::DirectXTK`, so the
# redirect both satisfies find_package(directxtk CONFIG) and exports the namespaced alias
# regardless of which spelling the fetched tree provides.
# ----------------------------------------------------------------------------------------------
set(DIRECTXTK_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_XAUDIO2_REDIST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    directxtk
    GIT_REPOSITORY https://github.com/microsoft/DirectXTK.git
    GIT_TAG oct2025
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(directxtk)

file(
    WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/directxtkConfig.cmake"
    [[if(NOT TARGET Microsoft::DirectXTK)
    if(TARGET DirectXTK)
        add_library(Microsoft::DirectXTK ALIAS DirectXTK)
    else()
        message(FATAL_ERROR "DirectXTK was fetched, but no usable target exists.")
    endif()
endif()]]
)

# ----------------------------------------------------------------------------------------------
# rapidcsv — the address-library database reader inside CommonLibSSE's REL::IDDB. It is
# header-only, so there is nothing to build: CommonLibSSE's find_path() call looks for
# the RAPIDCSV_INCLUDE_DIRS cache variable, and a cache variable that already exists is a
# find_path() that has already won.
# ----------------------------------------------------------------------------------------------
FetchContent_Declare(
    rapidcsv
    GIT_REPOSITORY https://github.com/d99kris/rapidcsv.git
    GIT_TAG v9.07
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(rapidcsv)

set(RAPIDCSV_INCLUDE_DIRS "${rapidcsv_SOURCE_DIR}/src" CACHE PATH "rapidcsv headers (fetched)")
