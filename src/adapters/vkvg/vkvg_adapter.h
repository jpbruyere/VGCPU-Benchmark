// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-07] Backend Adapters (Chapter 3) / [API-06-05] Cairo backend
// (Chapter 4)

#pragma once

#include "adapters/adapter_interface.h"

#include <vkvg.h>

#include <vector>

namespace vgcpu {

/// vkvg backend adapter for CPU-only 2D vector rendering.
/// Uses vkvg Image Surface for pure CPU software rasterization.
class vkvgAdapter : public IBackendAdapter {
   public:
    vkvgAdapter() = default;
    ~vkvgAdapter() override = default;

    // Lifecycle
    Status Initialize(const AdapterArgs& args) override;
    Status Prepare(const PreparedScene& scene) override;
    void Shutdown() override;

    // Metadata
    [[nodiscard]] AdapterInfo GetInfo() const override;
    [[nodiscard]] CapabilitySet GetCapabilities() const override;

    // Rendering
    Status Render(const PreparedScene& scene, const SurfaceConfig& config,
                  std::vector<uint8_t>& output_buffer) override;
    Status RenderLifecycle(const PreparedScene& scene, const SurfaceConfig& config,
                           std::vector<uint8_t>& output_buffer) override;
   private:
    void DestroyPaths();
    void DestroyPaints();

    bool initialized_ = false;
    //std::vector<vkvg_path_t*> prepared_paths_;
    std::vector<VkvgPattern> prepared_patterns_;
    VkvgDevice vkvgDev;
};

/// Register the vkvg adapter with the global registry.
void RegisterVkvgAdapter();

}  // namespace vgcpu
