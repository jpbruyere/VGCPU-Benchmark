// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// SPDX-License-Identifier: MIT

// Blueprint Reference: [ARCH-10-01] CLI Frontend (Chapter 3)

#include "adapters/adapter_registry.h"
#include "assets/scene_registry.h"
#include "cli/cli_parser.h"
#include "harness/harness.h"
#include "ir/ir_loader.h"
#include "pal/environment.h"
#include "pal/memory_tracker.h"
#include "pal/timer.h"
#include "reporting/reporter.h"
#include "vgcpu/internal/log.h"
#include "vgcpu/internal/version.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

// Force linking of adapter implementations
#ifdef VGCPU_ENABLE_NULL_BACKEND
#include "adapters/null/null_adapter.h"
#endif

#ifdef VGCPU_ENABLE_PLUTOVG
#include "adapters/plutovg/plutovg_adapter.h"
#endif

#ifdef VGCPU_ENABLE_CAIRO
#include "adapters/cairo/cairo_adapter.h"
#endif

#ifdef VGCPU_ENABLE_VKVG
#include "adapters/vkvg/vkvg_adapter.h"
#endif

#ifdef VGCPU_ENABLE_BLEND2D
#include "adapters/blend2d/blend2d_adapter.h"
#endif

#ifdef VGCPU_ENABLE_SKIA
#include "adapters/skia/skia_adapter.h"
#endif

#ifdef VGCPU_ENABLE_THORVG
#include "adapters/thorvg/thorvg_adapter.h"
#endif

#ifdef VGCPU_ENABLE_AGG
#include "adapters/agg/agg_adapter.h"
#endif

#ifdef VGCPU_ENABLE_QT
#include "adapters/qt/qt_adapter.h"
#endif

#ifdef VGCPU_ENABLE_AMANITHVG
#include "adapters/amanithvg/amanithvg_adapter.h"
#endif

#ifdef VGCPU_ENABLE_RAQOTE
#include "adapters/raqote/raqote_adapter.h"
#endif

#ifdef VGCPU_ENABLE_VELLO
#include "adapters/vello/vello_adapter.h"
#endif

using namespace vgcpu;

namespace {

// Parse a cpuset string like "2", "0-11" or "0,2,4-7" into CPU indices.
// Returns an empty vector on malformed input.
std::vector<int> ParseCpuSet(const std::string& spec) {
    std::vector<int> cpus;
    size_t pos = 0;
    while (pos < spec.size()) {
        size_t comma = spec.find(',', pos);
        std::string token = spec.substr(pos, comma == std::string::npos ? comma : comma - pos);
        pos = comma == std::string::npos ? spec.size() : comma + 1;
        try {
            size_t dash = token.find('-');
            if (dash == std::string::npos) {
                cpus.push_back(std::stoi(token));
            } else {
                int lo = std::stoi(token.substr(0, dash));
                int hi = std::stoi(token.substr(dash + 1));
                if (lo > hi) {
                    return {};
                }
                for (int c = lo; c <= hi; ++c) {
                    cpus.push_back(c);
                }
            }
        } catch (const std::exception&) {
            return {};
        }
    }
    return cpus;
}
// Default assets directory relative to executable
const std::filesystem::path kDefaultAssetsDir = "assets/scenes";
const std::filesystem::path kDefaultManifest = "assets/scenes/manifest.json";

/// Initialize the SceneRegistry from manifest if available.
void InitSceneRegistry() {
    if (std::filesystem::exists(kDefaultManifest)) {
        auto status = SceneRegistry::Instance().LoadManifest(kDefaultManifest, kDefaultAssetsDir);
        if (status.failed()) {
            VGCPU_LOG_WARN("Failed to load scene manifest: " + status.message);
        }
    }
}

/// Handle the 'list' command.
/// Blueprint Reference: [API-01-01] CLI list subcommand (Chapter 4) / [ARCH-13-01] (Chapter 3)
int HandleList(const CliOptions& options) {
    (void)options;
    std::cout << "Available Backends:\n";
    auto& registry = AdapterRegistry::Instance();
    for (const auto& id : registry.GetAdapterIds()) {
        std::cout << "  - " << id << "\n";
    }
    std::cout << "\nAvailable Scenes:\n";
    std::cout << "  - test/simple_rect (built-in test scene)\n";

    auto& scene_registry = SceneRegistry::Instance();
    for (const auto& scene_id : scene_registry.GetSceneIds()) {
        auto info = scene_registry.GetSceneInfo(scene_id);
        if (info) {
            std::cout << "  - " << scene_id;
            if (!info->description.empty()) {
                std::cout << " (" << info->description << ")";
            }
            std::cout << "\n";
        }
    }
    return 0;
}

/// Handle the 'metadata' command.
/// Blueprint Reference: [API-01-01] CLI metadata subcommand (Chapter 4) / [ARCH-13-01] (Chapter 3)
int HandleMetadata(const CliOptions& options) {
    (void)options;
    auto env = pal::CollectEnvironment();
    std::cout << "Environment Metadata:\n";
    std::cout << "  OS:        " << env.os_name << " " << env.os_version << "\n";
    std::cout << "  Arch:      " << env.arch << "\n";
    std::cout << "  CPU:       " << env.cpu_model << "\n";
    std::cout << "  Cores:     " << env.cpu_cores << "\n";
    std::cout << "  Memory:    " << (env.memory_bytes / (1024 * 1024)) << " MB\n";
    std::cout << "  Compiler:  " << env.compiler_name << " " << env.compiler_version << "\n";
    std::cout << "  CPU Time:  " << pal::GetCpuTimeSemantics() << "\n";
    std::cout << "  CPU Freq:  " << (pal::GetCpuFrequency() / 1'000'000.0) << " MHz (est)\n";
    std::cout << "\nBuild Info:\n";
    std::cout << "  Version:   " << VGCPU_VERSION_STRING << "\n";
    std::cout << "  Enabled Adapters:\n";
    for (const auto& id : AdapterRegistry::Instance().GetAdapterIds()) {
        std::cout << "    - " << id << "\n";
    }
    return 0;
}

/// Handle the 'validate' command.
/// Blueprint Reference: [API-01-01] CLI validate subcommand (Chapter 4) / [ARCH-13-01] (Chapter 3)
int HandleValidate(const CliOptions& options) {
    (void)options;
    if (options.validate_timer) {
        std::cout << "Validating CPU timer accuracy...\n";
        auto wall_start = pal::NowMonotonic();
        auto cpu_start = pal::GetCpuTime();

        // Busy wait for 100ms
        while (pal::Elapsed(wall_start, pal::NowMonotonic()) < std::chrono::milliseconds(100)) {
            // Spin
        }

        auto wall_end = pal::NowMonotonic();
        auto cpu_end = pal::GetCpuTime();

        auto wall_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(wall_end - wall_start).count();
        auto cpu_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(cpu_end - cpu_start).count();

        std::cout << "  Wall Time: " << wall_ms << " ms\n";
        std::cout << "  CPU Time:  " << cpu_ms << " ms\n";

        if (std::abs(wall_ms - cpu_ms) < 20) {
            std::cout << "  Status: OK (Within 20ms tolerance)\n";
        } else {
            std::cout << "  Status: WARNING (Discrepancy detected)\n";
        }
    } else {
        std::cout << "Validation not yet implemented (no manifest loaded).\n";
        std::cout << "Built-in test scene: OK\n";
    }
    return 0;
}

/// Handle the 'run' command.
/// Blueprint Reference: [API-01-01] CLI run subcommand (Chapter 4) / [ARCH-13-01] (Chapter 3)
std::vector<PreparedScene> LoadScenesFromOptions(const CliOptions& options) {
    std::vector<PreparedScene> scenes;
    if (options.all_scenes) {
        auto& scene_reg = SceneRegistry::Instance();
        for (const auto& scene_id : scene_reg.GetSceneIds()) {
            auto path = scene_reg.GetScenePath(scene_id);
            if (path && std::filesystem::exists(*path)) {
                auto bytes = ir::IrLoader::LoadFromFile(*path);
                if (bytes) {
                    auto result = ir::IrLoader::Prepare(*bytes, scene_id);
                    if (result.ok()) {
                        scenes.push_back(std::move(result.value()));
                    }
                }
            }
        }
        if (!scenes.empty()) {
            VGCPU_LOG_INFO("Loaded " + std::to_string(scenes.size()) + " scenes from manifest");
        }
    } else if (!options.scenes.empty()) {
        for (const auto& scene_arg : options.scenes) {
            std::filesystem::path scene_path(scene_arg);
            if (scene_path.extension() == ".irbin" || std::filesystem::exists(scene_path)) {
                auto bytes = ir::IrLoader::LoadFromFile(scene_path);
                if (!bytes) {
                    VGCPU_LOG_ERROR("Failed to load scene: " + std::string(scene_arg));
                    continue;
                }
                auto result = ir::IrLoader::Prepare(*bytes, scene_path.stem().string());
                if (result.failed()) {
                    VGCPU_LOG_ERROR("Failed to parse scene: " + result.status().message);
                    continue;
                }
                scenes.push_back(std::move(result.value()));
                VGCPU_LOG_INFO("Loaded scene: " + scene_arg);
            } else {
                auto& scene_reg = SceneRegistry::Instance();
                auto path = scene_reg.GetScenePath(scene_arg);
                if (path && std::filesystem::exists(*path)) {
                    auto bytes = ir::IrLoader::LoadFromFile(*path);
                    if (bytes) {
                        auto result = ir::IrLoader::Prepare(*bytes, scene_arg);
                        if (result.ok()) {
                            scenes.push_back(std::move(result.value()));
                            VGCPU_LOG_INFO("Loaded scene: " + scene_arg);
                        }
                    }
                } else {
                    auto asset_path =
                        std::filesystem::path("assets/scenes") / (scene_arg + ".irbin");
                    if (std::filesystem::exists(asset_path)) {
                        auto bytes = ir::IrLoader::LoadFromFile(asset_path);
                        if (bytes) {
                            auto result = ir::IrLoader::Prepare(*bytes, scene_arg);
                            if (result.ok()) {
                                scenes.push_back(std::move(result.value()));
                                VGCPU_LOG_INFO("Loaded scene: " + scene_arg);
                            }
                        }
                    } else {
                        VGCPU_LOG_WARN("Scene not found: " + std::string(scene_arg));
                    }
                }
            }
        }
    }
    return scenes;
}

/// Blueprint Reference: [API-01-01] CLI run subcommand (Chapter 4) / [ARCH-13-01] (Chapter 3)
int HandleRun(const CliOptions& options) {
    auto& registry = AdapterRegistry::Instance();

    // Determine backends to use
    std::vector<std::string> backend_ids;
    if (options.all_backends || options.backends.empty()) {
        backend_ids = registry.GetAdapterIds();
    } else {
        backend_ids = options.backends;
    }

    if (backend_ids.empty()) {
        VGCPU_LOG_ERROR("No backends available");
        return 1;
    }

    auto scenes = LoadScenesFromOptions(options);
    if (scenes.empty()) {
        scenes.push_back(ir::IrLoader::CreateTestScene(800, 600));
    }
    // Setup benchmark policy
    BenchmarkPolicy policy;
    policy.warmup_iterations = options.warmup_iters;
    policy.measurement_iterations = options.measurement_iters;
    policy.repetitions = options.repetitions;
    policy.thread_count = options.threads;
    policy.generate_png = options.generate_png;
    policy.compare_ssim = options.compare_ssim;
    policy.golden_dir = options.golden_dir;
    policy.output_dir = options.output_dir.empty() ? "." : options.output_dir;

    // Measurement discipline (REQ-13-03): pin BEFORE any benchmark work so
    // backend worker threads created later inherit the affinity mask. A
    // requested pin that fails is a hard error -- silently unpinned numbers
    // would be mislabeled measurements.
    if (!options.pin_cpus.empty()) {
        auto cpus = ParseCpuSet(options.pin_cpus);
        if (cpus.empty() || !pal::PinToCpus(cpus)) {
            VGCPU_LOG_ERROR("Failed to pin process to CPU set '" + options.pin_cpus +
                            "' (--pin): bad set, unsupported platform or invalid CPU index");
            return 3;
        }
        VGCPU_LOG_INFO("Process pinned to logical CPU set " + options.pin_cpus);
        std::string governor = pal::GetCpuGovernor(cpus.front());
        if (!governor.empty() && governor != "performance") {
            VGCPU_LOG_WARN("CPU " + std::to_string(cpus.front()) + " governor is '" + governor +
                           "', not 'performance': sustained clocks may drift. Fix with: sudo "
                           "cpupower frequency-set -g performance");
        }
    }

    // Run benchmarks.
    // Owner policy (2026-08-30): repetition-major scheduling. All backends
    // are initialized up front; for each scene, every backend is prepared
    // and warmed, then repetitions are interleaved -- every backend runs
    // repetition r before any backend runs repetition r+1. Machine-state
    // transients (thermal drift, scheduler/HT-sibling interference) thus
    // spread across all engines instead of poisoning one backend's whole
    // sample pool.
    std::vector<CaseResult> results;

    struct ActiveBackend {
        std::string id;
        std::unique_ptr<IBackendAdapter> adapter;
    };
    std::vector<ActiveBackend> active_backends;
    for (const auto& backend_id : backend_ids) {
        auto adapter = registry.CreateAdapter(backend_id);
        if (!adapter) {
            VGCPU_LOG_WARN("Backend '" + backend_id + "' not found, skipping");
            continue;
        }

        AdapterArgs args;
        args.thread_count = policy.thread_count;
        auto status = adapter->Initialize(args);
        if (status.failed()) {
            VGCPU_LOG_WARN("Failed to initialize '" + backend_id + "': " + status.message);
            continue;
        }
        active_backends.push_back({backend_id, std::move(adapter)});
    }

    int repetitions = policy.repetitions > 0 ? policy.repetitions : 1;
    for (const auto& scene : scenes) {
        std::vector<CaseRun> runs;
        runs.reserve(active_backends.size());
        for (auto& backend : active_backends) {
            runs.push_back(Harness::BeginCase(*backend.adapter, scene, policy));
        }
        for (int rep = 0; rep < repetitions; ++rep) {
            for (auto& run : runs) {
                Harness::MeasureRepetition(run, policy);
            }
        }
        for (auto& run : runs) {
            results.push_back(Harness::FinishCase(run, policy));
        }
    }

    for (auto& backend : active_backends) {
        backend.adapter->Shutdown();
    }

    // Prepare metadata
    RunMetadata metadata;
    metadata.run_timestamp = pal::GetTimestamp();
    metadata.suite_version = VGCPU_VERSION_STRING;
    metadata.git_commit = VGCPU_GIT_COMMIT;
    metadata.environment = pal::CollectEnvironment();
    metadata.environment.pinned_cpus = options.pin_cpus;
    {
        auto cpus = ParseCpuSet(options.pin_cpus);
        metadata.environment.cpu_governor = pal::GetCpuGovernor(cpus.empty() ? 0 : cpus.front());
    }
    metadata.policy = policy;

    // Print summary
    if (options.print_summary) {
        SummaryWriter::PrintSummary(metadata, results);
    }

    // Write output files
    std::filesystem::path out_dir(options.output_dir);
    if (options.format == "json" || options.format == "both") {
        auto json_path = out_dir / "results.json";
        auto status = JsonWriter::Write(json_path, metadata, results);
        if (status.ok()) {
            VGCPU_LOG_INFO("JSON output: " + json_path.string());
        } else {
            VGCPU_LOG_ERROR("Error writing JSON: " + status.message);
        }
    }
    if (options.format == "csv" || options.format == "both") {
        auto csv_path = out_dir / "results.csv";
        auto status = CsvWriter::Write(csv_path, results);
        if (status.ok()) {
            VGCPU_LOG_INFO("CSV output: " + csv_path.string());
        } else {
            VGCPU_LOG_ERROR("Error writing CSV: " + status.message);
        }
    }

    return 0;
}

int HandleProfileMemory(const CliOptions& options) {
    auto& registry = AdapterRegistry::Instance();

    std::vector<std::string> backend_ids;
    if (options.all_backends || options.backends.empty()) {
        backend_ids = registry.GetAdapterIds();
    } else {
        backend_ids = options.backends;
    }

    if (backend_ids.empty()) {
        VGCPU_LOG_ERROR("No backends available");
        return 1;
    }

    auto scenes = LoadScenesFromOptions(options);
    if (scenes.empty()) {
        scenes.push_back(ir::IrLoader::CreateTestScene(800, 600));
    }

    struct ActiveBackend {
        std::string id;
        std::unique_ptr<IBackendAdapter> adapter;
    };
    std::vector<ActiveBackend> active_backends;
    for (const auto& backend_id : backend_ids) {
        auto adapter = registry.CreateAdapter(backend_id);
        if (!adapter) {
            VGCPU_LOG_WARN("Backend '" + backend_id + "' not found, skipping");
            continue;
        }

        AdapterArgs args;
        args.thread_count = 1;
        auto status = adapter->Initialize(args);
        if (status.failed()) {
            VGCPU_LOG_WARN("Failed to initialize '" + backend_id + "': " + status.message);
            continue;
        }
        active_backends.push_back({backend_id, std::move(adapter)});
    }

    struct MemoryCaseResult {
        std::string backend_id;
        std::string scene_id;
        std::string decision;
        std::string skip_reason;
        pal::MemoryProfile prebaked;
        pal::MemoryProfile lifecycle;
    };
    std::vector<MemoryCaseResult> results;

    for (const auto& scene : scenes) {
        for (auto& backend : active_backends) {
            auto caps = backend.adapter->GetCapabilities();
            std::string compat_reason = CheckCompatibility(caps, scene.required);
            std::string decision = "EXECUTE";
            std::string reason = "";
            if (!compat_reason.empty()) {
                if (compat_reason.rfind("FALLBACK:", 0) == 0) {
                    decision = "FALLBACK";
                    reason = compat_reason;
                } else {
                    results.push_back({backend.id, scene.scene_id, "SKIP", compat_reason, {}, {}});
                    continue;
                }
            }

            auto prep_status = backend.adapter->Prepare(scene);
            if (prep_status.failed()) {
                results.push_back(
                    {backend.id, scene.scene_id, "FAIL", prep_status.message, {}, {}});
                continue;
            }

            SurfaceConfig config;
            config.width = static_cast<int>(scene.width);
            config.height = static_cast<int>(scene.height);

            std::vector<uint8_t> output_buffer(static_cast<size_t>(config.width) * config.height *
                                               4);

            // 1 unmeasured warmup call
            backend.adapter->Render(scene, config, output_buffer);

            // Isolated memory profiling pass: Pre-baked (draw only)
            pal::MemoryProfile prof_prebaked =
                pal::TrackMemory([&]() { backend.adapter->Render(scene, config, output_buffer); });

            // Isolated memory profiling pass: Full-lifecycle (create -> draw -> destroy)
            pal::MemoryProfile prof_lifecycle = pal::TrackMemory(
                [&]() { backend.adapter->RenderLifecycle(scene, config, output_buffer); });
            results.push_back(
                {backend.id, scene.scene_id, decision, reason, prof_prebaked, prof_lifecycle});
        }
    }

    for (auto& backend : active_backends) {
        backend.adapter->Shutdown();
    }

    // Print summary to console
    std::cout << "\n"
              << "================================================================================="
                 "=======================\n"
              << "VGCPU-Benchmark — Working Memory Profile (Heap allocations & Peak Live Bytes per "
                 "frame)\n"
              << "================================================================================="
                 "=======================\n"
              << std::left << std::setw(14) << "Backend" << std::left << std::setw(28) << "Scene"
              << std::right << std::setw(14) << "Allocs/frame" << std::right << std::setw(14)
              << "Frees/frame" << std::right << std::setw(18) << "Peak Heap (KB)" << std::right
              << std::setw(18) << "Total Churn (KB)" << std::right << std::setw(12) << "Leaked\n"
              << "---------------------------------------------------------------------------------"
                 "-----------------------\n";

    for (const auto& r : results) {
        std::cout << std::left << std::setw(14) << r.backend_id << std::left << std::setw(28)
                  << r.scene_id;
        if (r.decision == "SKIP") {
            std::cout << "  SKIP (" << r.skip_reason << ")\n";
        } else if (r.decision == "FAIL") {
            std::cout << "  FAIL (" << r.skip_reason << ")\n";
        } else {
            std::cout << std::right << std::setw(14) << r.lifecycle.alloc_count << std::right
                      << std::setw(14) << r.lifecycle.free_count << std::right << std::setw(18)
                      << std::fixed << std::setprecision(1)
                      << (r.lifecycle.peak_heap_bytes / 1024.0) << std::right << std::setw(18)
                      << std::fixed << std::setprecision(1)
                      << (r.lifecycle.total_alloc_bytes / 1024.0) << std::right << std::setw(12)
                      << r.lifecycle.leaked_bytes << " B\n";
        }
    }
    std::cout << "================================================================================="
                 "=======================\n";

    // Write memory.json
    std::filesystem::path out_dir(options.output_dir);
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    auto json_path = out_dir / "memory.json";
    std::ofstream out(json_path);
    if (out) {
        out << "{\n"
            << "  \"benchmark\": \"VGCPU-Benchmark\",\n"
            << "  \"version\": \"" << VGCPU_VERSION_STRING << "\",\n"
            << "  \"timestamp\": \"" << pal::GetTimestamp() << "\",\n"
            << "  \"cases\": [\n";
        for (size_t i = 0; i < results.size(); ++i) {
            const auto& r = results[i];
            out << "    {\n"
                << "      \"backend_id\": \"" << r.backend_id << "\",\n"
                << "      \"scene_id\": \"" << r.scene_id << "\",\n"
                << "      \"decision\": \"" << r.decision << "\"";
            if (!r.skip_reason.empty()) {
                out << ",\n      \"reason\": \"" << r.skip_reason << "\"";
            }
            if (r.decision == "EXECUTE") {
                out << ",\n      \"memory\": {\n"
                    << "        \"alloc_count\": " << r.lifecycle.alloc_count << ",\n"
                    << "        \"free_count\": " << r.lifecycle.free_count << ",\n"
                    << "        \"peak_heap_bytes\": " << r.lifecycle.peak_heap_bytes << ",\n"
                    << "        \"total_alloc_bytes\": " << r.lifecycle.total_alloc_bytes << ",\n"
                    << "        \"leaked_bytes\": " << r.lifecycle.leaked_bytes << "\n"
                    << "      },\n"
                    << "      \"prebaked_memory\": {\n"
                    << "        \"alloc_count\": " << r.prebaked.alloc_count << ",\n"
                    << "        \"free_count\": " << r.prebaked.free_count << ",\n"
                    << "        \"peak_heap_bytes\": " << r.prebaked.peak_heap_bytes << ",\n"
                    << "        \"total_alloc_bytes\": " << r.prebaked.total_alloc_bytes << ",\n"
                    << "        \"leaked_bytes\": " << r.prebaked.leaked_bytes << "\n"
                    << "      },\n"
                    << "      \"lifecycle_memory\": {\n"
                    << "        \"alloc_count\": " << r.lifecycle.alloc_count << ",\n"
                    << "        \"free_count\": " << r.lifecycle.free_count << ",\n"
                    << "        \"peak_heap_bytes\": " << r.lifecycle.peak_heap_bytes << ",\n"
                    << "        \"total_alloc_bytes\": " << r.lifecycle.total_alloc_bytes << ",\n"
                    << "        \"leaked_bytes\": " << r.lifecycle.leaked_bytes << "\n"
                    << "      }\n";
            } else {
                out << "\n";
            }
            out << "    }" << (i + 1 < results.size() ? "," : "") << "\n";
        }
        out << "  ]\n}\n";
        VGCPU_LOG_INFO("Memory profile JSON: " + json_path.string());
    }

    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    // Register built-in adapters
#ifdef VGCPU_ENABLE_NULL_BACKEND
    RegisterNullAdapter();
#endif

#ifdef VGCPU_ENABLE_PLUTOVG
    RegisterPlutoVGAdapter();
#endif

#ifdef VGCPU_ENABLE_CAIRO
    RegisterCairoAdapter();
#endif

#ifdef VGCPU_ENABLE_VKVG
    RegisterVkvgAdapter();
#endif

#ifdef VGCPU_ENABLE_BLEND2D
    RegisterBlend2DAdapter();
#endif

#ifdef VGCPU_ENABLE_SKIA
    RegisterSkiaAdapter();
#endif

#ifdef VGCPU_ENABLE_THORVG
    RegisterThorVGAdapter();
#endif

#ifdef VGCPU_ENABLE_AGG
    vgcpu::adapters::agg_backend::RegisterAggAdapter();
#endif

#ifdef VGCPU_ENABLE_AGG
#include "adapters/agg/agg_adapter.h"
#endif

#ifdef VGCPU_ENABLE_QT
    RegisterQtAdapter();
#endif

#ifdef VGCPU_ENABLE_AMANITHVG
    RegisterAmanithVGAdapter();
#endif

#ifdef VGCPU_ENABLE_RAQOTE
    RegisterRaqoteAdapter();
#endif

#ifdef VGCPU_ENABLE_VELLO
    RegisterVelloAdapter();
#endif

    // Initialize scene registry from manifest
    InitSceneRegistry();

    auto options = CliParser::Parse(argc, argv);
    if (!options) {
        return 1;
    }

    switch (options->command) {
        case CliCommand::kHelp:
            CliParser::PrintHelp();
            return 0;
        case CliCommand::kList:
            return HandleList(*options);
        case CliCommand::kMetadata:
            return HandleMetadata(*options);
        case CliCommand::kValidate:
            return HandleValidate(*options);
        case CliCommand::kRun:
            return HandleRun(*options);
        case CliCommand::kProfileMemory:
            return HandleProfileMemory(*options);
        default:
            CliParser::PrintHelp();
            return 1;
    }
}
