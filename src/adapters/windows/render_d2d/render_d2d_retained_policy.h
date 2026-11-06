#ifndef REACH_ADAPTERS_WINDOWS_RENDER_D2D_RETAINED_POLICY_H
#define REACH_ADAPTERS_WINDOWS_RENDER_D2D_RETAINED_POLICY_H

#include "reach/core/render_commands.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

struct reach_d2d_retained_run
{
    size_t begin;
    size_t end;
    uint64_t id;
};

struct reach_d2d_surface_size
{
    uint32_t width;
    uint32_t height;
};

uint64_t reach_d2d_layer_bytes(float width, float height, bool has_paint, bool has_shadow);
reach_d2d_surface_size reach_d2d_surface_capacity(reach_d2d_surface_size capacity,
                                                  reach_d2d_surface_size need, uint64_t budget_left,
                                                  reach_d2d_surface_size growth_limit);
reach_d2d_surface_size reach_d2d_planned_surface(reach_d2d_surface_size capacity,
                                                 reach_d2d_surface_size need);
uint64_t reach_d2d_planned_layer_bytes(float width, float height, bool has_paint, bool has_shadow,
                                       reach_d2d_surface_size capacity);
uint64_t reach_d2d_ungrouped_run_id(uint64_t anchor, uint64_t ordinal);
void reach_d2d_retained_runs(const reach_render_command_buffer *buffer,
                             std::vector<reach_d2d_retained_run> *out);

#endif
