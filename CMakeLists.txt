# Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
# SPDX-License-Identifier: MIT

# Blueprint Reference: Appendix A, §A.2 — Build System (CMake)
cmake_minimum_required(VERSION 3.21)

# Fix for deprecation warnings
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 NEW)
endif()


# Fix for dependencies (e.g. FreeType) using old CMake versions (<3.5)
# which are no longer supported by modern CMake.
set(CMAKE_POLICY_VERSION_MINIMUM "3.5")

project(VGCPU-Benchmark
    VERSION 0.2.0
    DESCRIPTION "CPU-only 2D Vector Graphics Benchmark Suite"
    LANGUAGES CXX
)

# Blueprint Reference: Appendix A, §A.1 — C++20 Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDE/tooling support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type defaults
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()
# -----------------------------------------------------------------------------
# Global MSVC Runtime Library Setting (Must be before any targets)
# Skia prebuilts are static (/MT), so we force everything else to match.
# -----------------------------------------------------------------------------
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# -----------------------------------------------------------------------------
# Include Centralized Modules (Blueprint v1.0)
# -----------------------------------------------------------------------------
include(cmake/vgcpu_deps.cmake)
include(cmake/vgcpu_options.cmake)
include(cmake/vgcpu_sanitizers.cmake)

# -----------------------------------------------------------------------------
# Verify Vendor Dependencies Integrity (v1.1)
# Blueprint Reference: [REQ-155], [DEC-BUILD-21]
# -----------------------------------------------------------------------------
include(cmake/vgcpu_third_party_hashes.cmake)
if(NOT DEFINED VGCPU_VERIFY_THIRD_PARTY_HASHES)
    set(VGCPU_VERIFY_THIRD_PARTY_HASHES ON) 
endif()

if(VGCPU_VERIFY_THIRD_PARTY_HASHES)
    vgcpu_verify_third_party_hashes()
endif()



# -----------------------------------------------------------------------------
# FetchContent for dependency management
# Blueprint Reference: Appendix A, §A.7.3 — Integration
# -----------------------------------------------------------------------------
include(FetchContent)

# nlohmann/json for manifest parsing
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        ${VGCPU_DEP_NLOHMANN_JSON_TAG}
)
FetchContent_MakeAvailable(nlohmann_json)

# -----------------------------------------------------------------------------
# PlutoVG (FetchContent)
# Blueprint Reference: backends/plutovg.md
# -----------------------------------------------------------------------------
if(ENABLE_PLUTOVG)
    FetchContent_Declare(
        plutovg
        GIT_REPOSITORY https://github.com/sammycage/plutovg.git
        GIT_TAG        ${VGCPU_DEP_PLUTOVG_TAG}
    )
    FetchContent_MakeAvailable(plutovg)
    message(STATUS "PlutoVG fetched from GitHub (${VGCPU_DEP_PLUTOVG_TAG})")
endif()

if(ENABLE_VKVG)
    FetchContent_Declare(
        vkvg
        GIT_REPOSITORY https://github.com/jpbruyere/vkvg.git
        GIT_TAG        ${VGCPU_DEP_VKVG_COMMIT}
    )
    FetchContent_MakeAvailable(vkvg)
    message(STATUS "VKVG fetched from GitHub (${VGCPU_DEP_VKVG_COMMIT})")
endif()
# -----------------------------------------------------------------------------
# Cairo (Prebuilt from preshing/cairo-windows)
# -----------------------------------------------------------------------------
if(ENABLE_CAIRO AND WIN32)
    FetchContent_Declare(
        cairo_prebuilt
        URL https://github.com/preshing/cairo-windows/releases/download/1.17.2/cairo-windows-1.17.2.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(cairo_prebuilt)

    if(WIN32)
        add_library(cairo STATIC IMPORTED)
        set_target_properties(cairo PROPERTIES
            IMPORTED_LOCATION "${cairo_prebuilt_SOURCE_DIR}/lib/x64/cairo.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${cairo_prebuilt_SOURCE_DIR}/include"
        )
        
        # Set root for DLL copy logic at the end of file
        set(LOCAL_CAIRO_ROOT "${cairo_prebuilt_SOURCE_DIR}")
        
        # Mark as found and create aliases expected by the rest of the build
        set(CAIRO_FOUND TRUE)
        set(cairo_VERSION "1.17.2")
        if(NOT TARGET PkgConfig::CAIRO)
            add_library(PkgConfig::CAIRO ALIAS cairo)
        endif()
    else()
        # Fallback for non-Windows (though this URL is Windows specific)
        find_package(Cairo REQUIRED)
    endif()
    
    message(STATUS "Cairo fetched (Prebuilt 1.17.2 - Windows)")
endif()
# -----------------------------------------------------------------------------
# Blend2D & AsmJit (FetchContent)
# Blueprint Reference: backends/blend2d.md
# -----------------------------------------------------------------------------
if(ENABLE_BLEND2D)
    # AsmJit (Required by Blend2D)
    # Blueprint Reference: [REQ-99], [REQ-100-01] — Pinned to commit SHA

    # Blend2D - Blueprint Reference: [REQ-99], [REQ-100-02] — Pinned to commit SHA
    FetchContent_Declare(
        blend2d
        GIT_REPOSITORY https://github.com/blend2d/blend2d.git
        GIT_TAG        ${VGCPU_DEP_BLEND2D_COMMIT}
    )

    # Prevent GNUInstallDirs recursion
    set(CMAKE_INSTALL_LIBDIR "lib" CACHE PATH "")
    set(CMAKE_INSTALL_BINDIR "bin" CACHE PATH "")
    set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE PATH "")

    set(ASMJIT_STATIC ON CACHE BOOL "" FORCE)
    set(ASMJIT_INSTALL OFF CACHE BOOL "" FORCE)
    set(BLEND2D_STATIC ON CACHE BOOL "" FORCE)
    set(BLEND2D_INSTALL OFF CACHE BOOL "" FORCE)
    set(BLEND2D_NO_JIT OFF CACHE BOOL "" FORCE)
    set(BLEND2D_EXTERNAL_ASMJIT OFF CACHE BOOL "" FORCE) # Use source directly
    # Populate asmjit (Use command form to avoid deprecation warning and add_subdirectory)
    FetchContent_Populate(
        asmjit
        GIT_REPOSITORY https://github.com/asmjit/asmjit.git
        GIT_TAG        ${VGCPU_DEP_ASMJIT_COMMIT}
    )


    # Point Blend2D to asmjit source
    set(ASMJIT_DIR "${asmjit_SOURCE_DIR}" CACHE PATH "" FORCE)

    # Now build Blend2D (which will add_subdirectory asmjit)
    FetchContent_MakeAvailable(blend2d)
    
    message(STATUS "Blend2D & AsmJit fetched from GitHub")
endif()

# -----------------------------------------------------------------------------
# Skia (Prebuilt binaries from Aseprite m124)
# Blueprint Reference: backends/skia.md
# Supports: Windows x64, Linux x64, macOS x64/arm64
# -----------------------------------------------------------------------------
if(ENABLE_SKIA)
    # Determine platform-specific URL for Skia prebuilt binaries
    if(WIN32)
        set(SKIA_URL "https://github.com/aseprite/skia/releases/download/m124-08a5439a6b/Skia-Windows-Release-x64.zip")
        set(SKIA_OUT_DIR "out/Release-x64")
        set(SKIA_LIB_NAME "skia.lib")
    elseif(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(SKIA_URL "https://github.com/aseprite/skia/releases/download/m124-08a5439a6b/Skia-macOS-Release-arm64.zip")
            set(SKIA_OUT_DIR "out/Release-arm64")
        else()
            set(SKIA_URL "https://github.com/aseprite/skia/releases/download/m124-08a5439a6b/Skia-macOS-Release-x64.zip")
            set(SKIA_OUT_DIR "out/Release-x64")
        endif()
        set(SKIA_LIB_NAME "libskia.a")
    else()  # Linux
        # Blueprint Reference: Chapter 3, §3.1.1 — x86_64 and ARM64 only
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            message(WARNING "Skia: Prebuilt binaries not available for Linux aarch64. Disabling Skia backend.")
            set(ENABLE_SKIA OFF CACHE BOOL "" FORCE)
        else()
            set(SKIA_URL "https://github.com/aseprite/skia/releases/download/m124-08a5439a6b/Skia-Linux-Release-x64.zip")
            set(SKIA_OUT_DIR "out/Release-x64")
            set(SKIA_LIB_NAME "libskia.a")
        endif()
    endif()

    FetchContent_Declare(
        skia_binaries
        URL ${SKIA_URL}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(skia_binaries)

    add_library(skia STATIC IMPORTED)
    set_target_properties(skia PROPERTIES
        IMPORTED_LOCATION "${skia_binaries_SOURCE_DIR}/${SKIA_OUT_DIR}/${SKIA_LIB_NAME}"
        INTERFACE_INCLUDE_DIRECTORIES "${skia_binaries_SOURCE_DIR}"
    )

    if(MSVC)
        target_link_libraries(skia INTERFACE 
            user32.lib gdi32.lib opengl32.lib ole32.lib oleaut32.lib uuid.lib advapi32.lib
            usp10.lib FontSub.lib msimg32.lib
            "${skia_binaries_SOURCE_DIR}/${SKIA_OUT_DIR}/skia.lib"
            "${skia_binaries_SOURCE_DIR}/${SKIA_OUT_DIR}/harfbuzz.lib"
            "${skia_binaries_SOURCE_DIR}/${SKIA_OUT_DIR}/icu.lib"
            "${skia_binaries_SOURCE_DIR}/${SKIA_OUT_DIR}/libpng.lib"
            "${skia_binaries_SOURCE_DIR}/${SKIA_OUT_DIR}/libjpeg.lib"
            "${skia_binaries_SOURCE_DIR}/${SKIA_OUT_DIR}/zlib.lib" 
        )
        # Ensure symbols are pulled from zlib
        target_link_options(skia INTERFACE 
            "/WHOLEARCHIVE:${skia_binaries_SOURCE_DIR}/${SKIA_OUT_DIR}/zlib.lib"
            "/ALTERNATENAME:inflateInit2_=Cr_z_inflateInit2_"
            "/ALTERNATENAME:inflateEnd=Cr_z_inflateEnd"
            "/ALTERNATENAME:inflateReset=Cr_z_inflateReset"
            "/ALTERNATENAME:inflate=Cr_z_inflate"
        )
        # set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    if(APPLE)
        find_library(CORE_FOUNDATION CoreFoundation)
        find_library(CORE_GRAPHICS CoreGraphics)
        find_library(CORE_TEXT CoreText)
        find_library(CORE_SERVICES CoreServices)
        
        find_package(ZLIB REQUIRED)

        target_link_libraries(skia INTERFACE 
            ${CORE_FOUNDATION} 
            ${CORE_GRAPHICS} 
            ${CORE_TEXT} 
            ${CORE_SERVICES}
            ZLIB::ZLIB
        )
    endif()

    if(UNIX AND NOT APPLE)  # Linux
        find_package(Fontconfig REQUIRED)
        find_package(Freetype REQUIRED)
        find_package(ZLIB REQUIRED)
        
        target_link_libraries(skia INTERFACE 
            Fontconfig::Fontconfig
            Freetype::Freetype
            ZLIB::ZLIB
            GL
            dl
        )
    endif()

    message(STATUS "Skia fetched (Prebuilt m124 - ${CMAKE_SYSTEM_NAME})")
endif()

# -----------------------------------------------------------------------------
# Anti-Grain Geometry (AGG)
# Blueprint Reference: backends/agg.md
# -----------------------------------------------------------------------------
if(ENABLE_AGG)
    # Freetype (Required by AGG)
    FetchContent_Declare(
        freetype
        GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
        GIT_TAG        ${VGCPU_DEP_FREETYPE_TAG}
    )
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(freetype)

    # Create alias for compatibility
    if(TARGET freetype AND NOT TARGET Freetype::Freetype)
        add_library(Freetype::Freetype ALIAS freetype)
    endif()

    # Blueprint Reference: [REQ-99], [REQ-100-03] — Pinned to commit SHA
    FetchContent_Declare(
        agg
        GIT_REPOSITORY https://github.com/ghaerr/agg-2.6.git
        GIT_TAG        ${VGCPU_DEP_AGG_COMMIT}
    )
    FetchContent_MakeAvailable(agg)

    if(NOT TARGET agg)
        file(GLOB AGG_SOURCES 
            "${agg_SOURCE_DIR}/agg-src/src/*.cpp"
            "${agg_SOURCE_DIR}/agg-src/src/ctrl/*.cpp" 
        )
        add_library(agg STATIC ${AGG_SOURCES})
        target_include_directories(agg PUBLIC 
            "${agg_SOURCE_DIR}/agg-src/include"
            "${agg_SOURCE_DIR}/agg-src/font_freetype"
        )
        # find_package(Freetype REQUIRED) -- We build it ourselves now
        target_link_libraries(agg PUBLIC Freetype::Freetype)
        
        target_compile_definitions(agg PUBLIC 
            AGG_BMP_READ_SUPPORT
            AGG_BMP_WRITE_SUPPORT
        )
    endif()
    message(STATUS "AGG 2.6 fetched from GitHub")
endif()

# -----------------------------------------------------------------------------
# ThorVG (FetchContent - SW Engine Built From Source)
# Blueprint Reference: backends/thorvg.md
# Note: ThorVG uses Meson, not CMake, so we compile sources directly
# -----------------------------------------------------------------------------
if(ENABLE_THORVG)
    FetchContent_Declare(
        thorvg
        GIT_REPOSITORY https://github.com/thorvg/thorvg.git
        GIT_TAG        ${VGCPU_DEP_THORVG_TAG}
    )
    # Populate thorvg
    FetchContent_MakeAvailable(thorvg)


    # Create ThorVG library from source (SW engine only, no loaders)
    set(THORVG_SOURCES
        # Core renderer
        ${thorvg_SOURCE_DIR}/src/renderer/tvgAccessor.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/tvgAnimation.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/tvgCanvas.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/tvgFill.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/tvgInitializer.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/tvgLoader.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/tvgPaint.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/tvgPicture.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/tvgRender.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/tvgSaver.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/tvgScene.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/tvgShape.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/tvgSwCanvas.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/tvgTaskScheduler.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/tvgText.cpp
        # SW Engine
        ${thorvg_SOURCE_DIR}/src/renderer/sw_engine/tvgSwFill.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/sw_engine/tvgSwImage.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/sw_engine/tvgSwMath.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/sw_engine/tvgSwMemPool.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/sw_engine/tvgSwPostEffect.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/sw_engine/tvgSwRaster.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/sw_engine/tvgSwRenderer.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/sw_engine/tvgSwRle.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/sw_engine/tvgSwShape.cpp
        ${thorvg_SOURCE_DIR}/src/renderer/sw_engine/tvgSwStroke.cpp
        # Common
        ${thorvg_SOURCE_DIR}/src/common/tvgCompressor.cpp
        ${thorvg_SOURCE_DIR}/src/common/tvgMath.cpp
        ${thorvg_SOURCE_DIR}/src/common/tvgStr.cpp
        # Raw Loader (required, unconditionally included by tvgLoader.cpp)
        ${thorvg_SOURCE_DIR}/src/loaders/raw/tvgRawLoader.cpp
    )

    # Generate config.h for ThorVG (normally created by Meson)
    set(THORVG_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}/thorvg-config")
    file(MAKE_DIRECTORY ${THORVG_CONFIG_DIR})
    file(WRITE "${THORVG_CONFIG_DIR}/config.h" "
    // Auto-generated config for ThorVG CMake build
    // Note: ThorVG uses #ifdef, so we only define features we WANT enabled
    #ifndef _TVG_CONFIG_H_
    #define _TVG_CONFIG_H_

    #define THORVG_VERSION_STRING \"0.15.16\"
    #define THORVG_SW_RASTER_SUPPORT 1
    #define THORVG_THREAD_SUPPORT 1

    // Note: We intentionally do NOT define:
    // - THORVG_GL_RASTER_SUPPORT
    // - THORVG_WG_RASTER_SUPPORT
    // - THORVG_SVG_LOADER_SUPPORT
    // - THORVG_PNG_LOADER_SUPPORT
    // - THORVG_JPG_LOADER_SUPPORT
    // - THORVG_LOTTIE_LOADER_SUPPORT
    // - THORVG_TTF_LOADER_SUPPORT
    // - THORVG_WEBP_LOADER_SUPPORT
    // - THORVG_TVG_LOADER_SUPPORT
    // - THORVG_TVG_SAVER_SUPPORT
    // - THORVG_GIF_SAVER_SUPPORT
    // - THORVG_LOG_ENABLED
    // because ThorVG uses #ifdef (not #if)

    // Enable SIMD on ARM64
    #if defined(__aarch64__) || defined(_M_ARM64)
    #define THORVG_NEON_VECTOR_SUPPORT 1
    #endif

    // Enable AVX on x86_64
    #if defined(__x86_64__) || defined(_M_X64)
    #define THORVG_AVX_VECTOR_SUPPORT 1
    #endif

    #endif // _TVG_CONFIG_H_
    ")

    add_library(thorvg STATIC ${THORVG_SOURCES})
    
    target_include_directories(thorvg PUBLIC
        ${thorvg_SOURCE_DIR}/inc
    )
    target_include_directories(thorvg PRIVATE
        ${THORVG_CONFIG_DIR}
        ${thorvg_SOURCE_DIR}/src/common
        ${thorvg_SOURCE_DIR}/src/renderer
        ${thorvg_SOURCE_DIR}/src/renderer/sw_engine
        ${thorvg_SOURCE_DIR}/src/loaders/raw
    )
    
    target_compile_definitions(thorvg PUBLIC TVG_STATIC)
    
    # Suppress warnings in third-party code
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(thorvg PRIVATE -w)
        
        # Blueprint Reference: Chapter 10, §10.1 — SIMD Optimization
        # Add SIMD flags for Linux x64 to fix AVX inlining errors
        if(UNIX AND NOT APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
            target_compile_options(thorvg PRIVATE -mavx -msse4.1)
        endif()
    elseif(MSVC)
        target_compile_options(thorvg PRIVATE /w)
    endif()

    # Blueprint Reference: Appendix D, §D.1 — Language Standards
    # ThorVG uses 'identity' in a way that conflicts with std::identity in C++20.
    # We downgrade it to C++17 to avoid this conflict since we are building it as a separate target.
    set_target_properties(thorvg PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    
    message(STATUS "ThorVG built from source (v0.15.16, SW engine only, C++17)")
endif()

# -----------------------------------------------------------------------------
# AmanithVG SRE (SDK from GitHub)
# Blueprint Reference: backends/amanithvg.md
# -----------------------------------------------------------------------------
if(ENABLE_AMANITHVG)
    # Blueprint Reference: [REQ-99], [REQ-100-04] — Pinned to commit SHA
    FetchContent_Declare(
        amanithvg_sdk
        GIT_REPOSITORY https://github.com/Mazatech/amanithvg-sdk.git
        GIT_TAG        ${VGCPU_DEP_AMANITHVG_COMMIT}
        GIT_SHALLOW    ON
    )
    FetchContent_MakeAvailable(amanithvg_sdk)

    # Detect platform directory for prebuilt libraries
    if(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(AVG_PLATFORM_DIR "macosx/ub")  # Universal Binary for Apple Silicon
        else()
            set(AVG_PLATFORM_DIR "macosx/ub")
        endif()
        set(AVG_LIB_NAME "libAmanithVG.6.dylib")
    elseif(WIN32)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(AVG_PLATFORM_DIR "windows/x86_64")
        else()
            # Blueprint Reference: Chapter 3, §3.1.1 — x86_64 and ARM64 only (no 32-bit)
            message(FATAL_ERROR "windows/aarch64 is unsupported.")
        endif()
        set(AVG_LIB_NAME "libAmanithVG.dll")
    else()  # Linux
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(AVG_PLATFORM_DIR "linux/aarch64")
        else()
            set(AVG_PLATFORM_DIR "linux/x86_64")
        endif()
        set(AVG_LIB_NAME "libAmanithVG.so.6")
    endif()

    # SDK structure: lib/<platform>/sre/standalone/
    set(AVG_LIB_PATH "${amanithvg_sdk_SOURCE_DIR}/lib/${AVG_PLATFORM_DIR}/sre/standalone")

    # Create imported target for AmanithVG SRE
    add_library(amanithvg SHARED IMPORTED)
    set_target_properties(amanithvg PROPERTIES
        IMPORTED_LOCATION "${AVG_LIB_PATH}/${AVG_LIB_NAME}"
        INTERFACE_INCLUDE_DIRECTORIES "${amanithvg_sdk_SOURCE_DIR}/include"
        INTERFACE_COMPILE_DEFINITIONS "AM_SRE=1"
    )

    # On Windows, also need IMPORTED_IMPLIB
    if(WIN32)
        set_target_properties(amanithvg PROPERTIES
            IMPORTED_IMPLIB "${AVG_LIB_PATH}/libAmanithVG.lib"
        )
    endif()

    # On macOS, handle rpath
    if(APPLE)
        set_target_properties(amanithvg PROPERTIES
            IMPORTED_SONAME "@rpath/libAmanithVG.6.dylib"
        )
    endif()

    message(STATUS "AmanithVG SRE SDK fetched from GitHub")
endif()

# -----------------------------------------------------------------------------
# Corrosion (Rust-CMake Bridge)
# Blueprint Reference: backends/raqote.md, backends/vello.md, [DEC-BUILD-06]
# -----------------------------------------------------------------------------
if(ENABLE_RAQOTE OR ENABLE_VELLO_CPU)
    set(Rust_TOOLCHAIN ${VGCPU_DEP_RUST_TOOLCHAIN})
    FetchContent_Declare(
        Corrosion
        GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
        GIT_TAG ${VGCPU_DEP_CORROSION_TAG}
    )
    FetchContent_MakeAvailable(Corrosion)
    message(STATUS "Corrosion (Rust-CMake bridge) fetched (${VGCPU_DEP_CORROSION_TAG}) using toolchain ${Rust_TOOLCHAIN}")
endif()

if(ENABLE_RAQOTE OR ENABLE_VELLO_CPU)
    corrosion_import_crate(
        MANIFEST_PATH "${CMAKE_SOURCE_DIR}/rust_bridge/Cargo.toml"
    )
    message(STATUS "Rust bridge workspace imported via Corrosion using toolchain ${VGCPU_DEP_RUST_TOOLCHAIN}")
endif()

# -----------------------------------------------------------------------------
# Cairo (System library via PkgConfig)
# Blueprint Reference: backends/cairo.md
# -----------------------------------------------------------------------------
if(ENABLE_CAIRO)
    # 1. Try PkgConfig (standard on Linux/macOS)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(CAIRO IMPORTED_TARGET cairo>=1.16.0)
    endif()

    # 2. If not found, try CMake Config (vcpkg on Windows)
    if(NOT CAIRO_FOUND)
        find_package(cairo CONFIG QUIET)
        if(cairo_FOUND)
            if(TARGET cairo::cairo)
                # Alias to PkgConfig::CAIRO so the rest of the build works transparently
                if(NOT TARGET PkgConfig::CAIRO)
                    add_library(PkgConfig::CAIRO ALIAS cairo::cairo)
                endif()
                set(CAIRO_FOUND TRUE)
            endif()
        endif()
    endif()

    # 3. If still not found, try Local Deps (Preshing binaries)
    if(NOT CAIRO_FOUND)
        set(LOCAL_CAIRO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/deps/cairo/cairo-windows-1.17.2")
        if(EXISTS "${LOCAL_CAIRO_ROOT}/include/cairo.h")
            message(STATUS "Found local Cairo at: ${LOCAL_CAIRO_ROOT}")
            
            # Create imported target
            if(NOT TARGET cairo::cairo)
                add_library(cairo::cairo SHARED IMPORTED)
                
                if(MSVC)
                    set_target_properties(cairo::cairo PROPERTIES
                        IMPORTED_IMPLIB "${LOCAL_CAIRO_ROOT}/lib/x64/cairo.lib"
                        IMPORTED_LOCATION "${LOCAL_CAIRO_ROOT}/lib/x64/cairo.dll"
                        INTERFACE_INCLUDE_DIRECTORIES "${LOCAL_CAIRO_ROOT}/include"
                    )
                else()
                    # Assume x64 for now, adapt if needed
                    set_target_properties(cairo::cairo PROPERTIES
                        IMPORTED_IMPLIB "${LOCAL_CAIRO_ROOT}/lib/x64/cairo.lib"
                        IMPORTED_LOCATION "${LOCAL_CAIRO_ROOT}/lib/x64/cairo.dll"
                        INTERFACE_INCLUDE_DIRECTORIES "${LOCAL_CAIRO_ROOT}/include"
                    )
                endif()
            endif()

            # Alias
            if(NOT TARGET PkgConfig::CAIRO)
                add_library(PkgConfig::CAIRO ALIAS cairo::cairo)
            endif()
            
            set(CAIRO_FOUND TRUE)
            set(cairo_VERSION "1.17.2")
            
            # Save DLL location for copy step later
            set(LOCAL_CAIRO_DLL "${LOCAL_CAIRO_ROOT}/lib/x64/cairo.dll")
        endif()
    endif()

    if(CAIRO_FOUND)
        message(STATUS "Cairo found (Version: ${cairo_VERSION})")
    else()
        message(FATAL_ERROR "Cairo not found. Install libcairo2-dev (Linux), use Homebrew (macOS), or vcpkg/deps (Windows).")
    endif()
endif()

# -----------------------------------------------------------------------------
# Qt (System library via find_package)
# Blueprint Reference: backends/qt.md
# NOTE: Qt6 cannot be FetchContent'd per Qt documentation. Requires system install.
# Install via: brew install qt (macOS) or apt install qt6-base-dev (Linux)
# -----------------------------------------------------------------------------
if(ENABLE_QT)
    find_package(Qt6 COMPONENTS Gui QUIET)
    if(Qt6Gui_FOUND)
        message(STATUS "Qt6 Gui found (Version: ${Qt6Gui_VERSION})")
    else()
        message(WARNING "Qt6 Gui not found. Qt backend will be disabled.")
        message(STATUS "  Install Qt6: brew install qt (macOS) or apt install qt6-base-dev (Linux)")
        set(ENABLE_QT OFF CACHE BOOL "" FORCE)
    endif()
endif()

# -----------------------------------------------------------------------------
# Compiler Warnings
# Blueprint Reference: Appendix D, §D.1 — Language and Compilation Standards
# -----------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        # add_compile_options(-Werror)
    endif()
elseif(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/WX)
    endif()
endif()

# -----------------------------------------------------------------------------
# MinGW Static Linking (Fix for missing libgcc/libstdc++ DLLs)
# -----------------------------------------------------------------------------
if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

# -----------------------------------------------------------------------------
# Core Library (common types, PAL, IR, Assets)
# -----------------------------------------------------------------------------
add_library(vgcpu_core STATIC
    src/common/status.cpp
    src/pal/timer.cpp
    src/pal/environment.cpp
    src/pal/memory_tracker.cpp
    src/ir/ir_loader.cpp
    src/ir/prepared_scene.cpp
    src/assets/scene_registry.cpp
    src/common/alloc_tracker.cpp
)
vgcpu_apply_sanitizers(vgcpu_core)

target_include_directories(vgcpu_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(vgcpu_core SYSTEM PUBLIC
    ${nlohmann_json_SOURCE_DIR}/include
)

target_link_libraries(vgcpu_core PUBLIC nlohmann_json::nlohmann_json)

if(VGCPU_ENABLE_ALLOC_INSTRUMENTATION)
    target_compile_definitions(vgcpu_core PUBLIC VGCPU_ENABLE_ALLOC_INSTRUMENTATION)
endif()

# -----------------------------------------------------------------------------
# Adapter Library
# -----------------------------------------------------------------------------
set(ADAPTER_SOURCES "")

if(ENABLE_NULL_BACKEND)
    list(APPEND ADAPTER_SOURCES src/adapters/null/null_adapter.cpp)
    target_compile_definitions(vgcpu_core PUBLIC VGCPU_ENABLE_NULL_BACKEND)
endif()

if(ENABLE_PLUTOVG)
    list(APPEND ADAPTER_SOURCES src/adapters/plutovg/plutovg_adapter.cpp)
    target_compile_definitions(vgcpu_core PUBLIC VGCPU_ENABLE_PLUTOVG)
endif()

if(ENABLE_CAIRO)
    list(APPEND ADAPTER_SOURCES src/adapters/cairo/cairo_adapter.cpp)
    target_compile_definitions(vgcpu_core PUBLIC VGCPU_ENABLE_CAIRO)
endif()

if(ENABLE_VKVG)
    list(APPEND ADAPTER_SOURCES src/adapters/vkvg/vkvg_adapter.cpp)
    target_compile_definitions(vgcpu_core PUBLIC VGCPU_ENABLE_VKVG)
endif()

if(ENABLE_BLEND2D)
    list(APPEND ADAPTER_SOURCES src/adapters/blend2d/blend2d_adapter.cpp)
    target_compile_definitions(vgcpu_core PUBLIC VGCPU_ENABLE_BLEND2D)
endif()

if(ENABLE_SKIA)
    list(APPEND ADAPTER_SOURCES src/adapters/skia/skia_adapter.cpp)
    target_compile_definitions(vgcpu_core PUBLIC VGCPU_ENABLE_SKIA)
endif()

if(ENABLE_THORVG)
    list(APPEND ADAPTER_SOURCES src/adapters/thorvg/thorvg_adapter.cpp)
    target_compile_definitions(vgcpu_core PUBLIC VGCPU_ENABLE_THORVG)
endif()

if(ENABLE_AGG)
    list(APPEND ADAPTER_SOURCES src/adapters/agg/agg_adapter.cpp)
    target_compile_definitions(vgcpu_core PUBLIC VGCPU_ENABLE_AGG)
endif()

if(ENABLE_QT)
    list(APPEND ADAPTER_SOURCES src/adapters/qt/qt_adapter.cpp)
    target_compile_definitions(vgcpu_core PUBLIC VGCPU_ENABLE_QT)
endif()

if(ENABLE_AMANITHVG)
    list(APPEND ADAPTER_SOURCES src/adapters/amanithvg/amanithvg_adapter.cpp)
    target_compile_definitions(vgcpu_core PUBLIC VGCPU_ENABLE_AMANITHVG)
endif()

if(ENABLE_RAQOTE)
    list(APPEND ADAPTER_SOURCES src/adapters/raqote/raqote_adapter.cpp)
    target_compile_definitions(vgcpu_core PUBLIC VGCPU_ENABLE_RAQOTE)
endif()

if(ENABLE_VELLO_CPU)
    list(APPEND ADAPTER_SOURCES src/adapters/vello/vello_adapter.cpp)
    target_compile_definitions(vgcpu_core PUBLIC VGCPU_ENABLE_VELLO)
endif()

add_library(vgcpu_adapters STATIC
    src/adapters/adapter_registry.cpp
    ${ADAPTER_SOURCES}
)

target_link_libraries(vgcpu_adapters PUBLIC vgcpu_core)
vgcpu_apply_sanitizers(vgcpu_adapters)

# Link backend libraries
if(ENABLE_PLUTOVG)
    target_link_libraries(vgcpu_adapters PUBLIC plutovg)
endif()

if(ENABLE_CAIRO)
    target_link_libraries(vgcpu_adapters PUBLIC PkgConfig::CAIRO)
    if(UNIX AND NOT APPLE)
        find_package(Fontconfig QUIET)
        if(Fontconfig_FOUND)
            target_link_libraries(vgcpu_adapters PUBLIC Fontconfig::Fontconfig)
        endif()
    endif()
endif()

if(ENABLE_VKVG)
    target_link_libraries(vgcpu_adapters PUBLIC vkvg)
endif()

if(ENABLE_BLEND2D)
    target_link_libraries(vgcpu_adapters PUBLIC blend2d)
    target_include_directories(vgcpu_adapters SYSTEM PUBLIC ${blend2d_SOURCE_DIR})
endif()

if(ENABLE_SKIA)
    target_link_libraries(vgcpu_adapters PUBLIC skia)
endif()

if(ENABLE_THORVG)
    target_link_libraries(vgcpu_adapters PUBLIC thorvg)
    target_include_directories(vgcpu_adapters SYSTEM PUBLIC ${thorvg_SOURCE_DIR}/inc)
endif()

if(ENABLE_AGG)
    target_link_libraries(vgcpu_adapters PUBLIC agg)
endif()

if(ENABLE_QT)
    target_link_libraries(vgcpu_adapters PUBLIC Qt6::Gui)
endif()

if(ENABLE_AMANITHVG)
    target_link_libraries(vgcpu_adapters PUBLIC amanithvg)
endif()

if(ENABLE_RAQOTE)
    target_link_libraries(vgcpu_adapters PUBLIC raqote_ffi)
endif()

if(ENABLE_VELLO_CPU)
    target_link_libraries(vgcpu_adapters PUBLIC vello_ffi)
endif()

# -----------------------------------------------------------------------------
# Artifacts Module (v1.1)
# -----------------------------------------------------------------------------
add_library(vgcpu_artifacts STATIC
    src/artifacts/stb_image_write_impl.cpp
    src/artifacts/stb_image_read_impl.cpp
    src/artifacts/png_writer.cpp
    src/artifacts/png_reader.cpp
    src/artifacts/ssim_compare.cpp
    src/artifacts/naming.cpp
)
target_include_directories(vgcpu_artifacts PUBLIC
    include
    src
)
target_include_directories(vgcpu_artifacts SYSTEM PRIVATE
    third_party
)
target_link_libraries(vgcpu_artifacts PUBLIC vgcpu_core)
vgcpu_apply_sanitizers(vgcpu_artifacts)

# -----------------------------------------------------------------------------
# Harness Library
# -----------------------------------------------------------------------------
add_library(vgcpu_harness STATIC
    src/harness/harness.cpp
    src/harness/statistics.cpp
)

target_link_libraries(vgcpu_harness PUBLIC vgcpu_adapters vgcpu_artifacts)
vgcpu_apply_sanitizers(vgcpu_harness)

# -----------------------------------------------------------------------------
# Reporting Library
# -----------------------------------------------------------------------------
add_library(vgcpu_reporting STATIC
    src/reporting/json_writer.cpp
    src/reporting/csv_writer.cpp
    src/reporting/summary_writer.cpp
)

target_link_libraries(vgcpu_reporting PUBLIC vgcpu_core)
vgcpu_apply_sanitizers(vgcpu_reporting)

# -----------------------------------------------------------------------------
# CLI Executable
# Blueprint Reference: Chapter 7, §7.2.1 — Module cli
# -----------------------------------------------------------------------------
add_executable(vgcpu-benchmark
    src/cli/main.cpp
    src/cli/cli_parser.cpp
)

target_link_libraries(vgcpu-benchmark PUBLIC
    vgcpu_harness
    vgcpu_reporting
)
vgcpu_apply_sanitizers(vgcpu-benchmark)

if(ENABLE_SKIA AND MSVC)
    target_link_libraries(vgcpu-benchmark PRIVATE "${skia_binaries_SOURCE_DIR}/out/Release-x64/zlib.lib")
endif()

if(UNIX AND NOT APPLE)
    find_package(Fontconfig QUIET)
    find_package(Freetype QUIET)
    find_package(ZLIB QUIET)
    if(Fontconfig_FOUND AND Freetype_FOUND AND ZLIB_FOUND)
        target_link_libraries(vgcpu-benchmark PRIVATE -Wl,--no-as-needed fontconfig freetype z -Wl,--as-needed)
    endif()
endif()

# Handle DLL deployment for Windows
if(MSVC)
    # Cairo DLL
    set(LOCAL_CAIRO_DLL "${LOCAL_CAIRO_ROOT}/lib/x64/cairo.dll")
    if(EXISTS "${LOCAL_CAIRO_DLL}")
        add_custom_command(TARGET vgcpu-benchmark POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LOCAL_CAIRO_DLL}"
            $<TARGET_FILE_DIR:vgcpu-benchmark>
            COMMENT "Copying Cairo DLL to output directory"
        )
    endif()

    # AmanithVG DLL
    if(ENABLE_AMANITHVG)
        set(AMANITH_DLL "${amanithvg_sdk_SOURCE_DIR}/lib/windows/x86_64/sre/standalone/libAmanithVG.dll")
        if(EXISTS "${AMANITH_DLL}")
            add_custom_command(TARGET vgcpu-benchmark POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${AMANITH_DLL}"
                $<TARGET_FILE_DIR:vgcpu-benchmark>
                COMMENT "Copying AmanithVG DLL to output directory"
            )
        endif()
    endif()

    # Qt Deployment (windeployqt)
    if(ENABLE_QT)
        find_program(WINDEPLOYQT_EXE windeployqt HINTS "${Qt6_DIR}/../../../bin")
        if(WINDEPLOYQT_EXE)
            add_custom_command(TARGET vgcpu-benchmark POST_BUILD
                COMMAND ${WINDEPLOYQT_EXE} --release --no-translations --no-compiler-runtime --dir $<TARGET_FILE_DIR:vgcpu-benchmark> $<TARGET_FILE:vgcpu-benchmark>
                COMMENT "Running windeployqt to deploy Qt dependencies..."
            )
            # Manually deploy qoffscreen.dll as windeployqt doesn't always include it by default
            add_custom_command(TARGET vgcpu-benchmark POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:vgcpu-benchmark>/platforms
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${Qt6_DIR}/../../../plugins/platforms/qoffscreen.dll"
                "$<TARGET_FILE_DIR:vgcpu-benchmark>/platforms/qoffscreen.dll"
                COMMENT "Copying qoffscreen.dll..."
            )
        else()
            message(WARNING "windeployqt not found! Qt backend may fail to run.")
        endif()
    endif()
endif()

# -----------------------------------------------------------------------------
# Installation
# -----------------------------------------------------------------------------
install(TARGETS vgcpu-benchmark RUNTIME DESTINATION bin)

# -----------------------------------------------------------------------------
# Testing Infrastructure (Blueprint [TASK-04.01], [TEST-01])
# -----------------------------------------------------------------------------
enable_testing()

# Fetch doctest header-only testing framework
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
)
FetchContent_MakeAvailable(doctest)

# Build test executable
add_executable(vgcpu_tests
    tests/test_main.cpp
    tests/test_pal.cpp
    tests/test_registry.cpp
    tests/test_concurrency.cpp
    tests/test_hotpath.cpp
    tests/test_artifacts.cpp
    tests/test_correctness_oracle.cpp
)

target_include_directories(vgcpu_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${doctest_SOURCE_DIR}/doctest
)

target_link_libraries(vgcpu_tests PRIVATE
    vgcpu_adapters
    vgcpu_artifacts
)

# VGCPU_ENABLE_* (all backends) and VGCPU_ENABLE_ALLOC_INSTRUMENTATION are
# already propagated here as PUBLIC compile definitions transitively via
# vgcpu_adapters -> vgcpu_core (see target_compile_definitions(vgcpu_core
# PUBLIC ...) above); no target-local redefinition needed. Fixed
# 2026-08-30 (ADR-0004): a redundant PRIVATE redefinition limited to
# null/plutovg/blend2d used to shadow-appear here, which read as if tests
# were Tier-1-only by design -- they were not, tests/test_main.cpp already
# registers every backend the build enables.

# Apply sanitizers to test executable
vgcpu_apply_sanitizers(vgcpu_tests)

# Register tests with CTest
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(vgcpu_tests)

message(STATUS "Tests enabled: vgcpu_tests")



# -----------------------------------------------------------------------------
# Quality Gate Targets (Blueprint [REQ-103])
# -----------------------------------------------------------------------------

# Find Python for script execution
find_package(Python3 COMPONENTS Interpreter QUIET)

# Target: check_no_temp_dbg - Fails if TEMP-DBG markers exist [REQ-105]
if(Python3_FOUND)
    add_custom_target(check_no_temp_dbg
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_no_temp_dbg.py ${CMAKE_SOURCE_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "[REQ-105] Checking for TEMP-DBG markers..."
    )

    # Target: check_includes - Enforces include boundary layering [TASK-03.03]
    add_custom_target(check_includes
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_includes.py ${CMAKE_SOURCE_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "[REQ-41] Checking include boundary violations..."
    )
else()
    add_custom_target(check_no_temp_dbg
        COMMAND ${CMAKE_COMMAND} -E echo "Warning: Python3 not found, skipping TEMP-DBG check"
    )
    add_custom_target(check_includes
        COMMAND ${CMAKE_COMMAND} -E echo "Warning: Python3 not found, skipping include check"
    )
endif()

# Target: format_check - Verifies code formatting [REQ-116]
add_custom_target(format_check
    COMMAND ${CMAKE_SOURCE_DIR}/tools/format_check.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "[REQ-116] Checking code formatting..."
)

# Target: format - Applies code formatting
add_custom_target(format
    COMMAND ${CMAKE_SOURCE_DIR}/tools/format_all.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Formatting all source files..."
)

# Target: lint - Runs clang-tidy (when enabled) [REQ-103-02]
if(VGCPU_ENABLE_LINT)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        add_custom_target(lint
            COMMAND ${CLANG_TIDY_EXE} -p ${CMAKE_BINARY_DIR} 
                ${CMAKE_SOURCE_DIR}/src/**/*.cpp
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "[REQ-103-02] Running clang-tidy..."
        )
    else()
        add_custom_target(lint
            COMMAND ${CMAKE_COMMAND} -E echo "Warning: clang-tidy not found"
        )
    endif()
else()
    add_custom_target(lint
        COMMAND ${CMAKE_COMMAND} -E echo "Lint disabled (set VGCPU_ENABLE_LINT=ON to enable)"
    )
endif()

# Validate dependency pinning at configure time [REQ-99]
vgcpu_validate_no_floating_deps()

# -----------------------------------------------------------------------------
# Packaging Target (Blueprint [TASK-07.02])
# Creates a self-contained release package with executable, assets, and docs
# -----------------------------------------------------------------------------
set(VGCPU_PACKAGE_DIR "${CMAKE_BINARY_DIR}/package")

add_custom_target(package_release
    DEPENDS vgcpu-benchmark
    COMMAND ${CMAKE_COMMAND} -E make_directory "${VGCPU_PACKAGE_DIR}/bin"
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:vgcpu-benchmark> "${VGCPU_PACKAGE_DIR}/bin/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "${VGCPU_PACKAGE_DIR}/assets"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/README.md" "${VGCPU_PACKAGE_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/LICENSE" "${VGCPU_PACKAGE_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md" "${VGCPU_PACKAGE_DIR}/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/docs/QUICKSTART.md" "${VGCPU_PACKAGE_DIR}/"
    COMMENT "[TASK-07.02] Creating release package in ${VGCPU_PACKAGE_DIR}"
)

# Create archive target (Unix only for now)
if(UNIX)
    add_custom_target(package_archive
        DEPENDS package_release
        COMMAND ${CMAKE_COMMAND} -E tar czvf 
            "vgcpu-benchmark-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}.tar.gz"
            --format=gnutar
            "${VGCPU_PACKAGE_DIR}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Creating release archive..."
    )
endif()

message(STATUS "  Packaging:")
message(STATUS "    package_release, package_archive")


# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------
message(STATUS "")
message(STATUS "VGCPU-Benchmark Configuration Summary:")
message(STATUS "  Version:        ${PROJECT_VERSION}")
message(STATUS "  Build Type:     ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:   ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:       ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "")
message(STATUS "  Enabled Backends:")
message(STATUS "    Null (Debug): ${ENABLE_NULL_BACKEND}")
message(STATUS "    PlutoVG:      ${ENABLE_PLUTOVG}")
message(STATUS "    Cairo:        ${ENABLE_CAIRO}")
message(STATUS "    vkvg:         ${ENABLE_VKVG}")
message(STATUS "    Blend2D:      ${ENABLE_BLEND2D}")
message(STATUS "    Skia:         ${ENABLE_SKIA}")
message(STATUS "    ThorVG:       ${ENABLE_THORVG}")
message(STATUS "    AGG:          ${ENABLE_AGG}")
message(STATUS "    Qt:           ${ENABLE_QT}")
message(STATUS "    AmanithVG:    ${ENABLE_AMANITHVG}")
message(STATUS "    Raqote:       ${ENABLE_RAQOTE}")
message(STATUS "    vello_cpu:    ${ENABLE_VELLO_CPU}")
message(STATUS "")
message(STATUS "  Quality Gates:")
message(STATUS "    check_no_temp_dbg, format_check, format, lint")
message(STATUS "")

