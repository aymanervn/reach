#include "reach/features/tray.h"

#include "tray_common.h"

#include <math.h>
#include <new>

struct reach_tray
{
    reach_animation_manager manager;
    reach_animation_track tracks[REACH_TRAY_ANIM_COUNT];
    reach_tray_state state;
    reach_rect_f32 pointer_bounds;
    int32_t pointer_bounds_valid;
};

const reach_tray_state *reach_tray_state_ptr(reach_tray *tray)
{
    return tray != nullptr ? &tray->state : nullptr;
}

reach_result reach_tray_create(reach_tray **out_animations)
{
    if (out_animations == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_tray *animations = new (std::nothrow) reach_tray();
    if (animations == nullptr)
    {
        return REACH_ERROR;
    }
    reach_animation_manager_init(&animations->manager, animations->tracks, REACH_TRAY_ANIM_COUNT);
    animations->state.feedback_index = REACH_MAX_TRAY_ITEMS;
    *out_animations = animations;
    return REACH_OK;
}

void reach_tray_destroy(reach_tray *animations)
{
    delete animations;
}

static void reach_tray_tick(reach_tray *animations, double delta_seconds,
                            reach_feature_tick_result *out)
{
    if (out != nullptr)
    {
        *out = {};
    }
    if (animations == nullptr)
    {
        return;
    }

    int32_t feedback_was_active = reach_animation_manager_active(&animations->manager,
                                                                 REACH_TRAY_ANIM_FEEDBACK_OPACITY);
    reach_animation_manager_tick(&animations->manager, delta_seconds);
    int32_t feedback_active = reach_animation_manager_active(&animations->manager,
                                                             REACH_TRAY_ANIM_FEEDBACK_OPACITY);

    if ((feedback_was_active || feedback_active) && out != nullptr)
    {
        out->redraw = 1;
    }

    if (feedback_was_active && !feedback_active && !animations->state.feedback_pressed &&
        reach_animation_manager_value(&animations->manager, REACH_TRAY_ANIM_FEEDBACK_OPACITY) <=
            0.001f)
    {
        reach_animation_manager_set(&animations->manager, REACH_TRAY_ANIM_FEEDBACK_OPACITY, 0.0f);
        animations->state.feedback_index = REACH_MAX_TRAY_ITEMS;
    }
}

int32_t reach_tray_popup_is_open(const reach_tray *tray)
{
    return tray != nullptr && tray->state.popup_open;
}

int32_t reach_tray_set_popup_open(reach_tray *tray, int32_t open)
{
    if (tray == nullptr)
    {
        return 0;
    }
    int32_t next_open = open ? 1 : 0;
    if (tray->state.popup_open == next_open)
    {
        return 0;
    }
    tray->state.popup_open = next_open;
    if (!next_open)
    {
        (void)reach_tray_feedback_release(tray);
    }
    return 1;
}

static void reach_tray_reset(reach_tray *tray)
{
    if (tray != nullptr)
    {
        reach_tray_model_init(&tray->state.model);
        tray->pointer_bounds_valid = 0;
    }
}

reach_result reach_tray_refresh(reach_tray *tray, reach_tray_provider_port *provider)
{
    return tray != nullptr ? reach_tray_model_refresh(&tray->state.model, provider) : REACH_OK;
}

void reach_tray_layout_popup(reach_tray *tray, const reach_theme *theme,
                             const reach_dock_layout *dock_layout, float dpi_scale,
                             reach_rect_f32 *out_bounds)
{
    if (tray != nullptr)
    {
        reach_tray_compute_popup_layout(&tray->state.model, theme, dock_layout, dpi_scale,
                                        out_bounds);
        if (out_bounds != nullptr)
        {
            tray->pointer_bounds = *out_bounds;
            tray->pointer_bounds_valid = 1;
        }
    }
}

int32_t reach_tray_feedback_press(reach_tray *tray, size_t index)
{
    if (tray == nullptr || index >= REACH_MAX_TRAY_ITEMS)
    {
        return 0;
    }
    tray->state.feedback_pressed = 1;
    tray->state.feedback_index = index;
    reach_animation_manager_animate_to(&tray->manager, REACH_TRAY_ANIM_FEEDBACK_OPACITY, 0.50f,
                                       0.055, REACH_EASING_EASE_IN_OUT);
    return 1;
}

int32_t reach_tray_feedback_release(reach_tray *tray)
{
    if (tray == nullptr ||
        (!tray->state.feedback_pressed && tray->state.feedback_index == REACH_MAX_TRAY_ITEMS))
    {
        return 0;
    }
    tray->state.feedback_pressed = 0;
    if (tray->state.feedback_index != REACH_MAX_TRAY_ITEMS)
    {
        reach_animation_manager_animate_to(&tray->manager, REACH_TRAY_ANIM_FEEDBACK_OPACITY, 0.0f,
                                           0.055, REACH_EASING_EASE_IN_OUT);
    }
    return 1;
}

static void reach_tray_capsule_reset(void *capsule)
{
    reach_tray_reset(static_cast<reach_tray *>(capsule));
}

static void reach_tray_capsule_tick(void *capsule, double delta_seconds,
                                    reach_feature_tick_result *out)
{
    reach_tray_tick(static_cast<reach_tray *>(capsule), delta_seconds, out);
}

static int32_t reach_tray_capsule_is_open(const void *capsule)
{
    return reach_tray_popup_is_open(static_cast<const reach_tray *>(capsule));
}

static void reach_tray_capsule_force_close(void *capsule)
{
    (void)reach_tray_set_popup_open(static_cast<reach_tray *>(capsule), 0);
}

static int32_t reach_tray_capsule_needs_frame(const void *capsule)
{
    const reach_tray *tray = static_cast<const reach_tray *>(capsule);
    return tray != nullptr && reach_animation_manager_any_active(&tray->manager);
}

static void reach_tray_capsule_set_action(const reach_tray_feature_action *action,
                                          reach_capsule_pointer_result *out)
{
    if (action == nullptr || out == nullptr ||
        action->type != REACH_TRAY_FEATURE_ACTION_ACTIVATE)
    {
        return;
    }
    out->action.kind = action->provider_action == REACH_TRAY_ACTION_RIGHT_CLICK
                           ? REACH_TRAY_POINTER_ACTION_ACTIVATE_RIGHT
                           : REACH_TRAY_POINTER_ACTION_ACTIVATE_LEFT;
    out->action.index = action->item_index;
    out->action.id = action->item_id;
}

static void reach_tray_capsule_handle_pointer(void *capsule,
                                              const reach_pointer_event *event,
                                              reach_capsule_pointer_result *out)
{
    if (out != nullptr)
    {
        *out = {};
    }
    reach_tray *tray = static_cast<reach_tray *>(capsule);
    if (tray == nullptr || event == nullptr || out == nullptr)
    {
        return;
    }

    reach_tray_hit_result hit = {};
    hit.type = REACH_TRAY_HIT_NONE;
    hit.index = REACH_MAX_TRAY_ITEMS;
    if (tray->state.popup_open && tray->pointer_bounds_valid)
    {
        hit = reach_tray_hit_test_popup(&tray->state.model, tray->pointer_bounds, event->x,
                                        event->y);
    }

    if (event->kind == REACH_POINTER_EVENT_DOWN)
    {
        if (hit.type == REACH_TRAY_HIT_ITEM)
        {
            out->redraw = reach_tray_feedback_press(tray, hit.index);
            out->handled = 1;
        }
        else if (hit.type == REACH_TRAY_HIT_POPUP)
        {
            out->handled = 1;
        }
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_UP)
    {
        out->redraw = reach_tray_feedback_release(tray);
        if (hit.type == REACH_TRAY_HIT_ITEM)
        {
            reach_tray_feature_action action = reach_tray_action_for_hit(
                &tray->state.model, hit, REACH_TRAY_ACTION_LEFT_CLICK);
            reach_tray_capsule_set_action(&action, out);
            out->handled = 1;
        }
        else if (hit.type == REACH_TRAY_HIT_POPUP)
        {
            out->handled = 1;
        }
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_CONTEXT)
    {
        if (hit.type == REACH_TRAY_HIT_ITEM)
        {
            out->redraw = reach_tray_feedback_press(tray, hit.index);
            out->redraw = reach_tray_feedback_release(tray) || out->redraw;
            reach_tray_feature_action action = reach_tray_action_for_hit(
                &tray->state.model, hit, REACH_TRAY_ACTION_RIGHT_CLICK);
            reach_tray_capsule_set_action(&action, out);
            out->handled = 1;
        }
        else if (hit.type == REACH_TRAY_HIT_POPUP)
        {
            out->handled = 1;
        }
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_MIDDLE ||
        event->kind == REACH_POINTER_EVENT_CANCEL)
    {
        out->redraw = reach_tray_feedback_release(tray);
        out->handled = out->redraw;
    }
}

const reach_feature_capsule_ops *reach_tray_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        reach_tray_capsule_reset,  reach_tray_capsule_tick,
        reach_tray_capsule_is_open, reach_tray_capsule_force_close,
        nullptr  , reach_tray_capsule_needs_frame,
        nullptr  ,
        reach_tray_capsule_handle_pointer,
    };
    return &ops;
}

reach_animation_manager *reach_tray_animation_manager(reach_tray *animations)
{
    return animations != nullptr ? &animations->manager : nullptr;
}

static size_t reach_tray_min_size(size_t a, size_t b)
{
    return a < b ? a : b;
}

void reach_tray_compute_popup_layout(reach_tray_model *model, const reach_theme *theme,
                                     const reach_dock_layout *dock_layout, float dpi_scale,
                                     reach_rect_f32 *out_bounds)
{
    if (model == nullptr || theme == nullptr || dock_layout == nullptr || out_bounds == nullptr)
    {
        return;
    }

    float slot_size = reach_theme_tray_slot_size(theme, dock_layout->bounds.height);
    float gap = slot_size * 0.22f;
    float padding = slot_size * 0.58f;
    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    float notch_height = reach_popup_notch_height_scaled(scale);
    size_t visual_count = model->item_count > 0 ? model->item_count : 1;
    size_t columns = reach_tray_min_size(visual_count, 5);
    size_t rows = (visual_count + 4) / 5;
    float content_width = padding * 2.0f + (float)columns * slot_size + (float)(columns - 1) * gap;
    float content_height = padding * 2.0f + (float)rows * slot_size + (float)(rows - 1) * gap;

    out_bounds->width = ceilf(content_width);
    out_bounds->height = ceilf(content_height + notch_height);
    out_bounds->x = dock_layout->tray_button.x + dock_layout->tray_button.width * 0.5f -
                    out_bounds->width * 0.5f;
    out_bounds->y = dock_layout->bounds.y - out_bounds->height - 8.0f * scale;

    float grid_height = (float)rows * slot_size + (float)(rows - 1) * gap;
    float grid_y = out_bounds->y + (content_height - grid_height) * 0.5f;
    for (size_t index = 0; index < model->item_count; ++index)
    {
        size_t row = index / 5;
        size_t column = index % 5;
        size_t row_start = row * 5;
        size_t row_remaining = model->item_count - row_start;
        size_t row_columns = reach_tray_min_size(row_remaining, 5);
        float row_width = (float)row_columns * slot_size + (float)(row_columns - 1) * gap;
        float row_x = out_bounds->x + (out_bounds->width - row_width) * 0.5f;
        model->item_slots[index].x = row_x + (float)column * (slot_size + gap);
        model->item_slots[index].y = grid_y + (float)row * (slot_size + gap);
        model->item_slots[index].width = slot_size;
        model->item_slots[index].height = slot_size;
    }
}

reach_result reach_tray_append_render_commands(reach_tray *tray,
                                               const reach_tray_render_context *ctx,
                                               reach_render_command_buffer *out_commands)
{
    if (tray == nullptr || ctx == nullptr || out_commands == nullptr || ctx->theme == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_tray_state *state = &tray->state;

    reach_tray_render_input input = {};
    input.theme = ctx->theme;
    input.model = &state->model;
    input.bounds = ctx->bounds;
    input.dock_height = ctx->dock_height;
    input.dpi_scale = ctx->dpi_scale;
    input.click_feedback_index = state->feedback_index;
    input.click_feedback_opacity = reach_animation_manager_value(
        reach_tray_animation_manager(tray), REACH_TRAY_ANIM_FEEDBACK_OPACITY);
    input.text_alignment_center = REACH_TEXT_ALIGNMENT_CENTER;

    return reach_tray_build_render_commands(&input, out_commands);
}

size_t reach_tray_item_count(reach_tray *tray)
{
    return tray != nullptr ? tray->state.model.item_count : 0;
}

uint64_t reach_tray_item_icon_id(reach_tray *tray, size_t index)
{
    if (tray == nullptr || index >= tray->state.model.item_count)
    {
        return 0;
    }
    return tray->state.model.items[index].icon_id;
}
