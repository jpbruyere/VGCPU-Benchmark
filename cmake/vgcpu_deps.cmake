# cmake/vgcpu_deps.cmake
# -----------------------------------------------------------------------------
# Centralized Dependency Versions (Blueprint [REQ-99], [DEC-BUILD-05])
# This is the single source of truth for all external dependency versions.
# No floating branches (master/main) are allowed in shipped presets.
# -----------------------------------------------------------------------------

# nlohmann/json - JSON for Modern C++
set(VGCPU_DEP_NLOHMANN_JSON_TAG "v3.11.3")

# PlutoVG - Small C vector graphics library
set(VGCPU_DEP_PLUTOVG_TAG "v1.3.2")

set(VGCPU_DEP_VKVG_COMMIT "e76c8dead9d8334214713d156f85632165f65930")
# AsmJit - JIT assembler (used by Blend2D)
# Pinned to commit SHA for reproducibility (no release tags available)
set(VGCPU_DEP_ASMJIT_COMMIT "c87860217e43e2a06060fcaae5b468f6a55b9963")

# Blend2D - 2D vector graphics engine
# Pinned to commit SHA for reproducibility (rolling release, no version tags)
set(VGCPU_DEP_BLEND2D_COMMIT "6dbc2cefbc996379e07104e34519a440b49b15d7")

# Skia - 2D graphics library (prebuilt binaries from Aseprite)
set(VGCPU_DEP_SKIA_VERSION "m124-08a5439a6b")

# ThorVG - Vector graphics library
set(VGCPU_DEP_THORVG_TAG "v0.15.16")

# AGG - Anti-Grain Geometry
# Pinned to commit SHA (maintained fork, no version tags)
set(VGCPU_DEP_AGG_COMMIT "c4f36b4432142f22c0bf82c6fbdb41567a236be2")

# FreeType - Font rendering library (used by AGG)
set(VGCPU_DEP_FREETYPE_TAG "VER-2-13-2")

# AmanithVG SDK - Vector graphics library
# Pinned to commit SHA (SDK snapshots, no version tags). Bumped 2026-08-30:
# the previously pinned b7f44f6b was no longer reachable in the upstream
# repo (history rewritten upstream, single-branch snapshot convention) --
# updated to the current master HEAD.
set(VGCPU_DEP_AMANITHVG_COMMIT "207f00924dab804909d925f2004b41eb74a575de")

# Cairo - Prebuilt for Windows
set(VGCPU_DEP_CAIRO_PREBUILT_VERSION "1.17.2")

# Corrosion - Rust-CMake bridge. Bumped 2026-08-30 from v0.5.0: that version's
# FindRust.cmake cannot parse `rustup toolchain list --verbose` output from
# rustup >= 1.28.0 ("... (active, default) ..." replaced the old
# "... (default) ..." marker), so it silently discovers zero toolchains and
# fails configure (upstream corrosion-rs#590, fixed in v0.5.1).
set(VGCPU_DEP_CORROSION_TAG "v0.5.2")

# Rust toolchain version. Pinned to stable (DEC-BUILD-06,
# rust_bridge/rust-toolchain.toml). Fixed 2026-08-30: this variable still
# said "nightly" after DEC-BUILD-06 moved to stable, which made Corrosion's
# FindRust look for a "nightly-*" rustup toolchain that no longer exists
# and fail configure outright. Bumped from 1.84.0 to 1.86.0 the same day:
# 1.84.0 cannot build vello_cpu (needs `edition2024`, stable since 1.85.0);
# vello_cpu 0.0.4's own MSRV is 1.86.0, so that is the pin.
if(APPLE)
    set(VGCPU_DEP_RUST_TOOLCHAIN "1.86.0-aarch64-apple-darwin")
elseif(WIN32)
    set(VGCPU_DEP_RUST_TOOLCHAIN "1.86.0-x86_64-pc-windows-msvc")
else()
    set(VGCPU_DEP_RUST_TOOLCHAIN "1.86.0-x86_64-unknown-linux-gnu")
endif()

# -----------------------------------------------------------------------------
# Vendored Dependencies (v1.1) - Integrity Check Constants
# -----------------------------------------------------------------------------
# ssim_lomont.hpp
set(VGCPU_DEP_SSIM_LOMONT_SHA256 "acd47dad46a781a8de112e58ae2c54641e83899f8abc198a55d551fd595f14bc")

# stb_image_write.h
set(VGCPU_DEP_STB_IMAGE_WRITE_SHA256 "cbd5f0ad7a9cf4468affb36354a1d2338034f2c12473cf1a8e32053cb6914a05")

# stb_image.h
set(VGCPU_DEP_STB_IMAGE_READ_SHA256 "594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3")

# -----------------------------------------------------------------------------
# Validate no floating deps function (called at configure time)
# -----------------------------------------------------------------------------
function(vgcpu_validate_no_floating_deps)
    # This function checks that no dependency uses master/main
    # It's called from CMakeLists.txt after all deps are declared
    
    set(FLOATING_BRANCHES "master" "main" "develop" "trunk")
    set(DEPS_TO_CHECK
        "VGCPU_DEP_ASMJIT_COMMIT"
        "VGCPU_DEP_BLEND2D_COMMIT"
        "VGCPU_DEP_AGG_COMMIT"
        "VGCPU_DEP_AMANITHVG_COMMIT"
    )
    
    foreach(dep ${DEPS_TO_CHECK})
        foreach(branch ${FLOATING_BRANCHES})
            if("${${dep}}" STREQUAL "${branch}")
                message(FATAL_ERROR 
                    "[REQ-99] Floating dependency detected: ${dep}=${${dep}}\n"
                    "All dependencies must be pinned to tags or commit SHAs.\n"
                    "Update cmake/vgcpu_deps.cmake with a specific version.")
            endif()
        endforeach()
    endforeach()
    
    message(STATUS "[REQ-99] Dependency pinning validated - no floating branches")
endfunction()
