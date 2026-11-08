// Copyright (c) 2025 Michele Fabbri (fabbri.michele@gmail.com)
// Copyright (c) 2026 JP Bruyere (jp_bruyere@hotmail.com)
// SPDX-License-Identifier: MIT

#include "adapters/vkvg/vkvg_adapter.h"

#include "adapters/adapter_registry.h"
#include "ir/ir_format.h"
#include "ir/prepared_scene.h"

#include <vkvg.h>

namespace vgcpu {

namespace {

// Set the vkvg source from an IR paint. Returns a pattern the caller must
VkvgPattern CreatevkvgPattern(const Paint& paint) {
    if (paint.type == ir::PaintType::kSolid) {
        double r = static_cast<double>((paint.color >> 0) & 0xFF) / 255.0;
        double g = static_cast<double>((paint.color >> 8) & 0xFF) / 255.0;
        double b = static_cast<double>((paint.color >> 16) & 0xFF) / 255.0;
        double a = static_cast<double>((paint.color >> 24) & 0xFF) / 255.0;
        return NULL;// vkvg_pattern_create_(b, g, r, a);  // R<->B swap: ARGB32 output
    }
    VkvgPattern pat = nullptr;
    if (paint.type == ir::PaintType::kLinear) {
        pat = vkvg_pattern_create_linear(paint.linear_start_x, paint.linear_start_y,
                                          paint.linear_end_x, paint.linear_end_y);
    } else {
        pat = vkvg_pattern_create_radial(paint.radial_center_x, paint.radial_center_y, 0.0,
                                          paint.radial_center_x, paint.radial_center_y,
                                          paint.radial_radius);
    }
    for (const auto& s : paint.stops) {
        double r = static_cast<double>((s.color >> 0) & 0xFF) / 255.0;
        double g = static_cast<double>((s.color >> 8) & 0xFF) / 255.0;
        double b = static_cast<double>((s.color >> 16) & 0xFF) / 255.0;
        double a = static_cast<double>((s.color >> 24) & 0xFF) / 255.0;
        vkvg_pattern_add_color_stop(pat, s.offset, r, g, b, a);  // R<->B swap
    }
    return pat;
}

// Rebuild the vkvg current path from IR path data.
void BuildPath(VkvgContext cr, const Path& path) {
    vkvg_new_path(cr);
    size_t pt_idx = 0;
    for (auto verb : path.verbs) {
        switch (verb) {
            case ir::PathVerb::kMoveTo:
                if (pt_idx + 1 <= path.points.size() / 2) {
                    vkvg_move_to(cr, path.points[pt_idx * 2], path.points[pt_idx * 2 + 1]);
                    pt_idx++;
                }
                break;
            case ir::PathVerb::kLineTo:
                if (pt_idx + 1 <= path.points.size() / 2) {
                    vkvg_line_to(cr, path.points[pt_idx * 2], path.points[pt_idx * 2 + 1]);
                    pt_idx++;
                }
                break;
            case ir::PathVerb::kQuadTo:
                // vkvg doesn't have native quad bezier, convert to cubic
                if (pt_idx + 2 <= path.points.size() / 2) {
                    float x0, y0;
                    vkvg_get_current_point(cr, &x0, &y0);
                    double x1 = path.points[pt_idx * 2];
                    double y1 = path.points[pt_idx * 2 + 1];
                    double x2 = path.points[(pt_idx + 1) * 2];
                    double y2 = path.points[(pt_idx + 1) * 2 + 1];
                    // Quad to cubic: P1 = P0 + 2/3*(C - P0), P2 = P2 + 2/3*(C - P2)
                    double cx1 = x0 + (2.0 / 3.0) * (x1 - x0);
                    double cy1 = y0 + (2.0 / 3.0) * (y1 - y0);
                    double cx2 = x2 + (2.0 / 3.0) * (x1 - x2);
                    double cy2 = y2 + (2.0 / 3.0) * (y1 - y2);
                    vkvg_curve_to(cr, cx1, cy1, cx2, cy2, x2, y2);
                    pt_idx += 2;
                }
                break;
            case ir::PathVerb::kCubicTo:
                if (pt_idx + 3 <= path.points.size() / 2) {
                    vkvg_curve_to(cr, path.points[pt_idx * 2], path.points[pt_idx * 2 + 1],
                                   path.points[(pt_idx + 1) * 2], path.points[(pt_idx + 1) * 2 + 1],
                                   path.points[(pt_idx + 2) * 2],
                                   path.points[(pt_idx + 2) * 2 + 1]);
                    pt_idx += 3;
                }
                break;
            case ir::PathVerb::kClose:
                vkvg_close_path(cr);
                break;
        }
    }
}

}  // namespace

Status vkvgAdapter::Initialize(const AdapterArgs& args) {
    (void)args;
    vkvg_device_create_info_t info{0};
    vkvgDev = vkvg_device_create(&info);
    initialized_ = true;
    return Status::Ok();
}

Status vkvgAdapter::Prepare(const PreparedScene& scene) {
    if (!initialized_) {
        return Status::Fail("vkvgAdapter not initialized");
    }
    DestroyPaths();
    DestroyPaints();

/*    VkvgSurface temp_surf = vkvg_surface_create(vkvgDev, 1, 1);
    VkvgContext temp_cr = vkvg_create(temp_surf);
    prepared_paths_.reserve(scene.paths.size());
    for (const auto& p : scene.paths) {
        vkvg_new_path(temp_cr);
        BuildPath(temp_cr, p);
        prepared_paths_.push_back(vkvg_copy_path(temp_cr));
    }
    vkvg_destroy(temp_cr);
    vkvg_surface_destroy(temp_surf);*/

    prepared_patterns_.reserve(scene.paints.size());
    for (const auto& p : scene.paints) {
        prepared_patterns_.push_back(CreatevkvgPattern(p));
    }
    return Status::Ok();
}

void vkvgAdapter::DestroyPaths() {
    /*for (auto p : prepared_paths_) {
        if (p)
            vkvg_path_destroy(p);
    }
    prepared_paths_.clear();*/
}

void vkvgAdapter::DestroyPaints() {
    for (auto p : prepared_patterns_) {
        if (p)
            vkvg_pattern_destroy(p);
    }
    prepared_patterns_.clear();
}

void vkvgAdapter::Shutdown() {
    DestroyPaths();
    DestroyPaints();
    vkvg_device_destroy(vkvgDev);
    initialized_ = false;
}

AdapterInfo vkvgAdapter::GetInfo() const {
    return AdapterInfo{.id = "vkvg",
                       .detailed_name = "vkvg (Vulkan Surface, GPU Rasterizer)",
                       .version = "0.4",
                       .is_cpu_only = true};
}

CapabilitySet vkvgAdapter::GetCapabilities() const {
    // vkvg supports all basic features
    return CapabilitySet::All();
}

namespace {

Status ExecutevkvgCommands(const PreparedScene& scene, const SurfaceConfig& config,
                            const std::vector<VkvgPattern>& patterns, VkvgContext cr) {
    const uint8_t* cmd = scene.command_stream.data();
    const uint8_t* end = cmd + scene.command_stream.size();

    uint16_t current_paint_id = 0;
    ir::FillRule current_fill_rule = ir::FillRule::kNonZero;
    uint16_t current_stroke_paint_id = 0;
    float current_stroke_width = 1.0f;
    ir::StrokeCap current_stroke_cap = ir::StrokeCap::kButt;
    ir::StrokeJoin current_stroke_join = ir::StrokeJoin::kMiter;
    std::vector<float> dash_lengths;
    float dash_phase = 0.0;

    while (cmd < end) {
        ir::Opcode opcode = static_cast<ir::Opcode>(*cmd++);

        switch (opcode) {
            case ir::Opcode::kEnd:
                goto done;

            case ir::Opcode::kClear: {
                if (cmd + 4 > end)
                    goto done;
                uint32_t rgba = *reinterpret_cast<const uint32_t*>(cmd);
                cmd += 4;

                double r = static_cast<double>((rgba >> 0) & 0xFF) / 255.0;
                double g = static_cast<double>((rgba >> 8) & 0xFF) / 255.0;
                double b = static_cast<double>((rgba >> 16) & 0xFF) / 255.0;
                double a = static_cast<double>((rgba >> 24) & 0xFF) / 255.0;

                vkvg_save(cr);
                vkvg_identity_matrix(cr);
                vkvg_rectangle(cr, 0, 0, config.width, config.height);
                vkvg_set_source_rgba(cr, b, g, r, a);
                vkvg_set_operator(cr, VKVG_OPERATOR_SOURCE);
                vkvg_fill(cr);
                vkvg_restore(cr);
                vkvg_set_operator(cr, VKVG_OPERATOR_OVER);
                break;
            }

            case ir::Opcode::kSetFill: {
                if (cmd + 3 > end)
                    goto done;
                current_paint_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;
                current_fill_rule = static_cast<ir::FillRule>(*cmd++);

                const auto& paint = scene.paints[current_paint_id];
                if (paint.type == ir::PaintType::kSolid) {
                    double r = static_cast<double>((paint.color >> 0) & 0xFF) / 255.0;
                    double g = static_cast<double>((paint.color >> 8) & 0xFF) / 255.0;
                    double b = static_cast<double>((paint.color >> 16) & 0xFF) / 255.0;
                    double a = static_cast<double>((paint.color >> 24) & 0xFF) / 255.0;
                    vkvg_set_source_rgba(cr, r, g, b, a);
                } else if (current_paint_id < patterns.size() && patterns[current_paint_id] != nullptr) {
                    vkvg_set_source(cr, patterns[current_paint_id]);
                }
                vkvg_set_fill_rule(cr, (current_fill_rule == ir::FillRule::kEvenOdd)
                                            ? VKVG_FILL_RULE_EVEN_ODD
                                            : VKVG_FILL_RULE_NON_ZERO);
                break;
            }

            case ir::Opcode::kSetStroke: {
                if (cmd + 7 > end)
                    goto done;
                current_stroke_paint_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;
                current_stroke_width = *reinterpret_cast<const float*>(cmd);
                cmd += 4;
                uint8_t opts = *cmd++;
                current_stroke_cap = ir::UnpackStrokeCap(opts);
                current_stroke_join = ir::UnpackStrokeJoin(opts);

                if (current_stroke_paint_id < patterns.size() &&
                    patterns[current_stroke_paint_id] != nullptr) {
                    vkvg_set_source(cr, patterns[current_stroke_paint_id]);
                }
                vkvg_set_line_width(cr, current_stroke_width);
                vkvg_line_cap_t cap = VKVG_LINE_CAP_BUTT;
                switch (current_stroke_cap) {
                    case ir::StrokeCap::kButt:
                        cap = VKVG_LINE_CAP_BUTT;
                        break;
                    case ir::StrokeCap::kRound:
                        cap = VKVG_LINE_CAP_ROUND;
                        break;
                    case ir::StrokeCap::kSquare:
                        cap = VKVG_LINE_CAP_SQUARE;
                        break;
                }
                vkvg_set_line_cap(cr, cap);
                vkvg_line_join_t join = VKVG_LINE_JOIN_MITER;
                switch (current_stroke_join) {
                    case ir::StrokeJoin::kMiter:
                        join = VKVG_LINE_JOIN_MITER;
                        break;
                    case ir::StrokeJoin::kRound:
                        join = VKVG_LINE_JOIN_ROUND;
                        break;
                    case ir::StrokeJoin::kBevel:
                        join = VKVG_LINE_JOIN_BEVEL;
                        break;
                }
                vkvg_set_line_join(cr, join);
                break;
            }

            case ir::Opcode::kFillPath: {
                if (cmd + 2 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                if (path_id < scene.paths.size()) {
                    BuildPath(cr, scene.paths[path_id]);
                    vkvg_fill(cr);
                }
                break;
            }

            case ir::Opcode::kStrokePath: {
                if (cmd + 2 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;

                if (path_id < scene.paths.size()) {
                    BuildPath(cr, scene.paths[path_id]);
                    vkvg_stroke(cr);
                }
                break;
            }

            case ir::Opcode::kSetMatrix: {
                if (cmd + 24 > end)
                    goto done;
                const float* m = reinterpret_cast<const float*>(cmd);
                cmd += 24;
                vkvg_matrix_t mtx;
                vkvg_matrix_init(&mtx, m[0], m[2], m[1], m[3], m[4], m[5]);
                vkvg_set_matrix(cr, &mtx);
                break;
            }

            case ir::Opcode::kConcatMatrix: {
                if (cmd + 24 > end)
                    goto done;
                const float* m = reinterpret_cast<const float*>(cmd);
                cmd += 24;
                vkvg_matrix_t mtx;
                vkvg_matrix_init(&mtx, m[0], m[2], m[1], m[3], m[4], m[5]);
                vkvg_transform(cr, &mtx);
                break;
            }

            case ir::Opcode::kSave:
                vkvg_save(cr);
                break;

            case ir::Opcode::kRestore:
                vkvg_restore(cr);
                break;

            case ir::Opcode::kSetDash: {
                if (cmd + 5 > end)
                    goto done;
                uint8_t count = *cmd++;
                dash_phase = static_cast<float>(*reinterpret_cast<const float*>(cmd));
                cmd += 4;
                if (cmd + 4 * count > end)
                    goto done;
                const float* lengths = reinterpret_cast<const float*>(cmd);
                cmd += 4 * count;
                dash_lengths.assign(lengths, lengths + count);
                vkvg_set_dash(cr, dash_lengths.empty() ? nullptr : dash_lengths.data(),
                               static_cast<int>(dash_lengths.size()), dash_phase);
                break;
            }

            case ir::Opcode::kClipPush: {
                if (cmd + 3 > end)
                    goto done;
                uint16_t path_id = *reinterpret_cast<const uint16_t*>(cmd);
                cmd += 2;
                ir::FillRule rule = static_cast<ir::FillRule>(*cmd++);

                vkvg_save(cr);
                if (path_id < scene.paths.size()) {
                    BuildPath(cr, scene.paths[path_id]);
                    vkvg_set_fill_rule(cr, rule == ir::FillRule::kEvenOdd
                                               ? VKVG_FILL_RULE_EVEN_ODD
                                               : VKVG_FILL_RULE_NON_ZERO);
                    vkvg_clip(cr);
                }
                break;
            }

            case ir::Opcode::kClipPop:
                vkvg_restore(cr);
                break;

            default:
                goto done;
        }
    }

done:
    return Status::Ok();
}

}  // namespace

Status vkvgAdapter::Render(const PreparedScene& scene, const SurfaceConfig& config,
                            std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("vkvgAdapter not initialized");
    if (!scene.IsValid())
        return Status::InvalidArg("Invalid scene");
    if (config.width <= 0 || config.height <= 0)
        return Status::InvalidArg("Invalid surface configuration");

    //int stride = vkvg_format_stride_for_width(vkvg_FORMAT_ARGB32, config.width);
    VkvgSurface surface = vkvg_surface_create(vkvgDev, config.width, config.height);
    if (vkvg_surface_status(surface) != VKVG_STATUS_SUCCESS) {
        return Status::Fail("Failed to create vkvg surface");
    }

    VkvgContext cr = vkvg_create(surface);
    if (vkvg_status(cr) != VKVG_STATUS_SUCCESS) {
        vkvg_surface_destroy(surface);
        return Status::Fail("Failed to create vkvg context");
    }
    //vkvg_set_antialias(cr, vkvg_ANTIALIAS_BEST);

    Status s = ExecutevkvgCommands(scene, config, prepared_patterns_, cr);

    vkvg_destroy(cr);
    vkvg_surface_write_to_memory(surface, output_buffer.data());
    vkvg_surface_destroy(surface);
    return s;
}
Status vkvgAdapter::RenderLifecycle(const PreparedScene& scene, const SurfaceConfig& config,
                                     std::vector<uint8_t>& output_buffer) {
    if (!initialized_)
        return Status::Fail("vkvgAdapter not initialized");
    if (!scene.IsValid())
        return Status::InvalidArg("Invalid scene");
    if (config.width <= 0 || config.height <= 0)
        return Status::InvalidArg("Invalid surface configuration");

    VkvgSurface surface = vkvg_surface_create_ms(vkvgDev, config.width, config.height, VK_SAMPLE_COUNT_8_BIT);
    if (vkvg_surface_status(surface) != VKVG_STATUS_SUCCESS) {
        return Status::Fail("Failed to create vkvg surface");
    }

    VkvgContext cr = vkvg_create(surface);
    if (vkvg_status(cr) != VKVG_STATUS_SUCCESS) {
        vkvg_surface_destroy(surface);
        return Status::Fail("Failed to create vkvg context");
    }
//    vkvg_set_antialias(cr, vkvg_ANTIALIAS_BEST);

    std::vector<VkvgPattern> patterns;
    patterns.reserve(scene.paints.size());
    for (const auto& p : scene.paints) {
        patterns.push_back(CreatevkvgPattern(p));
    }

    Status s = ExecutevkvgCommands(scene, config, patterns, cr);


    for (auto p : patterns) {
        if (p)
            vkvg_pattern_destroy(p);
    }
    patterns.clear();

    vkvg_destroy(cr);
    vkvg_surface_write_to_memory(surface, output_buffer.data());
    vkvg_surface_destroy(surface);
    return s;
}

// Explicit registration function
void RegisterVkvgAdapter() {
    AdapterRegistry::Instance().Register("vkvg", "vkvg (Image Surface, CPU Rasterizer)",
                                         []() { return std::make_unique<vkvgAdapter>(); });
}

}  // namespace vgcpu
