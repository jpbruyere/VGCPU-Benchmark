# cmake/vgcpu_options.cmake
# -----------------------------------------------------------------------------
# VGCPU-Benchmark Build Options (Blueprint [REQ-95], [REQ-96])
# -----------------------------------------------------------------------------

# Project-level options per [REQ-95]
option(VGCPU_WERROR "Treat warnings as errors" OFF)
option(VGCPU_TIER1_ONLY "Build only Tier-1 backends (null, plutovg, blend2d)" OFF)
option(VGCPU_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(VGCPU_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(VGCPU_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(VGCPU_ENABLE_LINT "Enable clang-tidy linting" OFF)
option(VGCPU_ENABLE_ALLOC_INSTRUMENTATION "Enable allocation instrumentation for testing" OFF)

# -----------------------------------------------------------------------------
# Backend Options (preserve existing ENABLE_* names per [DEC-BUILD-01])
# Tier-1 backends: null, plutovg, blend2d (always ON unless tier1-only mode forces others OFF)
# Optional backends: all others
# -----------------------------------------------------------------------------

# Tier-1 backends (always available)
option(ENABLE_NULL_BACKEND "Enable Null backend (Tier-1)" ON)
option(ENABLE_PLUTOVG "Enable PlutoVG backend (Tier-1)" ON)
option(ENABLE_VKVG "Enable VKVG backend (Tier-1)" ON)
option(ENABLE_BLEND2D "Enable Blend2D backend (Tier-1)" ON)

# Optional backends (default ON for developer convenience, OFF when VGCPU_TIER1_ONLY)
if(VGCPU_TIER1_ONLY)
    # In Tier-1 mode, force optional backends OFF
    set(ENABLE_CAIRO OFF CACHE BOOL "Enable Cairo backend (Optional)" FORCE)
    set(ENABLE_SKIA OFF CACHE BOOL "Enable Skia backend (Optional)" FORCE)
    set(ENABLE_THORVG OFF CACHE BOOL "Enable ThorVG backend (Optional)" FORCE)
    set(ENABLE_AGG OFF CACHE BOOL "Enable AGG backend (Optional)" FORCE)
    set(ENABLE_QT OFF CACHE BOOL "Enable Qt Raster backend (Optional)" FORCE)
    set(ENABLE_AMANITHVG OFF CACHE BOOL "Enable AmanithVG SRE backend (Optional)" FORCE)
    set(ENABLE_RAQOTE OFF CACHE BOOL "Enable Raqote backend (Optional/Rust)" FORCE)
    set(ENABLE_VELLO_CPU OFF CACHE BOOL "Enable vello_cpu backend (Optional/Rust)" FORCE)
    message(STATUS "[DEC-SCOPE-02] VGCPU_TIER1_ONLY=ON: Building Tier-1 backends only (null, plutovg, blend2d)")
else()
    # Developer mode: optional backends default ON
    option(ENABLE_CAIRO "Enable Cairo backend (Optional)" ON)
    option(ENABLE_SKIA "Enable Skia backend (Optional)" ON)
    option(ENABLE_THORVG "Enable ThorVG backend (Optional)" ON)
    option(ENABLE_AGG "Enable AGG backend (Optional)" ON)
    option(ENABLE_QT "Enable Qt Raster backend (Optional)" ON)
    option(ENABLE_AMANITHVG "Enable AmanithVG SRE backend (Optional)" ON)
    option(ENABLE_RAQOTE "Enable Raqote backend (Optional/Rust)" ON)
    option(ENABLE_VELLO_CPU "Enable vello_cpu backend (Optional/Rust)" ON)
endif()
