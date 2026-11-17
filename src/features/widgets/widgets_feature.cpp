#include "reach/features/widgets.h"
#include "widgets_layout.h"
#include "widgets_timers.h"
#include "reach/core/typography.h"
#include "reach/features/common/pressable.h"
#include "reach/features/common/now_playing_artwork.h"

#include <algorithm>
#include <cwchar>
#include <cstring>
#include <cmath>
#include <new>

struct reach_widgets
{
    reach_clock *clock;
    reach_system_stats *stats;
    reach_now_playing_service *now_playing;
    reach_widgets_routes routes;
    reach_clock_snapshot clock_snapshot;
    reach_system_stats_snapshot stats_snapshot;
    reach_now_playing_snapshot media_snapshot;
    reach_rect_f32 desktop_bounds;
    reach_rect_f32 cards[REACH_MAX_DESKTOP_WIDGETS];
    reach_config_widget_placement placements[REACH_MAX_DESKTOP_WIDGETS];
    float dpi_scale;
    int32_t drag_index;
    int32_t resizing;
    int32_t moved;
    int32_t calendar_month_offset;
    float drag_x;
    float drag_y;
    reach_rect_f32 drag_origin;
    reach_rect_f32 drag_destination;
    bool drag_has_destination;
    bool layout_dirty;
    uint32_t enabled_mask;
    reach_pressable control_pressable;
    double stopwatch_elapsed;
    double stopwatch_started;
    double stopwatch_deadline;
    int32_t stopwatch_running;
    reach_pomodoro pomodoro;
    double productivity_deadline;
    bool pomodoro_completed;
};

static int32_t reach_widgets_contains(reach_rect_f32 rect, float x, float y)
{
    return x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height;
}

static void reach_widgets_reset(void *capsule)
{
    reach_widgets *widgets = static_cast<reach_widgets *>(capsule);
    if (widgets == nullptr)
    {
        return;
    }
    reach_pressable_reset(&widgets->control_pressable, nullptr);
    (void)reach_widgets_set_enabled(widgets, 0);
    widgets->drag_index = -1;
}

static void reach_widgets_tick(void *capsule, double, reach_feature_tick_result *out)
{
    reach_widgets *widgets = static_cast<reach_widgets *>(capsule);
    if (widgets == nullptr || out == nullptr)
    {
        return;
    }
    if (widgets->enabled_mask == 0)
        return;
    double now = reach_motion_now_seconds();
    if (widgets->stopwatch_running && (widgets->enabled_mask & (1u << REACH_WIDGET_STOPWATCH)) &&
        now >= widgets->stopwatch_deadline)
    {
        widgets->stopwatch_deadline = now + 0.1;
        out->redraw = 1;
    }
    if (widgets->pomodoro.running && (widgets->enabled_mask & (1u << REACH_WIDGET_POMODORO)) &&
        now >= widgets->productivity_deadline)
    {
        widgets->pomodoro_completed = reach_pomodoro_tick(widgets->pomodoro, now);
        widgets->productivity_deadline = now + 1.0;
        out->redraw = 1;
    }
    reach_clock_snapshot clock = {};
    reach_system_stats_snapshot stats = {};
    reach_now_playing_snapshot media = {};
    if (widgets->clock != nullptr &&
        (widgets->enabled_mask & ((1u << REACH_WIDGET_CLOCK) | (1u << REACH_WIDGET_CALENDAR))))
    {
        reach_clock_snapshot_take(widgets->clock, &clock);
    }
    if (widgets->stats != nullptr && (widgets->enabled_mask & (1u << REACH_WIDGET_SYSTEM)))
    {
        reach_system_stats_snapshot_take(widgets->stats, &stats);
    }
    if (widgets->now_playing != nullptr &&
        (widgets->enabled_mask & (1u << REACH_WIDGET_NOW_PLAYING)))
    {
        reach_now_playing_service_snapshot(widgets->now_playing, &media);
    }
    if (clock.valid != widgets->clock_snapshot.valid ||
        clock.minute != widgets->clock_snapshot.minute ||
        clock.hour != widgets->clock_snapshot.hour || clock.day != widgets->clock_snapshot.day ||
        clock.month != widgets->clock_snapshot.month || clock.year != widgets->clock_snapshot.year ||
        stats.valid != widgets->stats_snapshot.valid ||
        stats.cpu_percent != widgets->stats_snapshot.cpu_percent ||
        stats.memory_percent != widgets->stats_snapshot.memory_percent ||
        stats.gpu_percent != widgets->stats_snapshot.gpu_percent ||
        stats.gpu_available != widgets->stats_snapshot.gpu_available ||
        stats.gpu_temperature_available != widgets->stats_snapshot.gpu_temperature_available ||
        stats.gpu_temperature_c != widgets->stats_snapshot.gpu_temperature_c ||
        media.generation != widgets->media_snapshot.generation)
    {
        widgets->clock_snapshot = clock;
        widgets->stats_snapshot = stats;
        widgets->media_snapshot = media;
        out->redraw = 1;
    }
}

static int32_t reach_widgets_desktop_open(const void *capsule)
{
    const auto *widgets = static_cast<const reach_widgets *>(capsule);
    return widgets != nullptr && widgets->enabled_mask != 0;
}

static void reach_widgets_desktop_geometry(const void *capsule, reach_feature_surface_geometry *out)
{
    const reach_widgets *widgets = static_cast<const reach_widgets *>(capsule);
    if (widgets == nullptr || out == nullptr)
    {
        return;
    }
    *out = {};
    out->visible_bounds = widgets->desktop_bounds;
    out->envelope_bounds = widgets->desktop_bounds;
    auto deadline = [&](double value)
    {
        if (value > 0 && (out->deadline_seconds <= 0 || value < out->deadline_seconds))
            out->deadline_seconds = value;
    };
    if (widgets->pomodoro.running && (widgets->enabled_mask & (1u << REACH_WIDGET_POMODORO)))
        deadline(widgets->productivity_deadline);

    if (widgets->stopwatch_running && (widgets->enabled_mask & (1u << REACH_WIDGET_STOPWATCH)))
        deadline(widgets->stopwatch_deadline);
}

static size_t reach_widgets_desktop_input_regions(const void *capsule, reach_rect_f32 *out_regions,
                                                  size_t max_regions)
{
    const reach_widgets *widgets = static_cast<const reach_widgets *>(capsule);
    if (widgets == nullptr || out_regions == nullptr)
    {
        return 0;
    }
    size_t count = 0;
    for (size_t index = 0; index < REACH_MAX_DESKTOP_WIDGETS && count < max_regions; ++index)
    {
        if (widgets->enabled_mask & (1u << index))
        {
            out_regions[count++] = widgets->cards[index];
        }
    }
    return count;
}

static int32_t reach_widgets_desktop_tracking(const void *capsule)
{
    const reach_widgets *widgets = static_cast<const reach_widgets *>(capsule);
    return widgets != nullptr && widgets->drag_index >= 0;
}

enum reach_widget_control
{
    REACH_WIDGET_CONTROL_STOPWATCH_START = 1,
    REACH_WIDGET_CONTROL_STOPWATCH_RESET,
    REACH_WIDGET_CONTROL_POMODORO_START,
    REACH_WIDGET_CONTROL_POMODORO_SKIP,
    REACH_WIDGET_CONTROL_POMODORO_RESET,
    REACH_WIDGET_CONTROL_MEDIA_PREVIOUS,
    REACH_WIDGET_CONTROL_MEDIA_TOGGLE,
    REACH_WIDGET_CONTROL_MEDIA_NEXT,
    REACH_WIDGET_CONTROL_CALENDAR_PREVIOUS,
    REACH_WIDGET_CONTROL_CALENDAR_NEXT
};

static reach_rect_f32 reach_widgets_control_bounds(reach_rect_f32 card, int32_t widget,
                                                   int32_t button, float scale)
{
    if (widget == REACH_WIDGET_CALENDAR)
    {
        const float x = button == 0 ? card.x + 16.0f * scale
                                   : card.x + card.width - 36.0f * scale;
        return {x, card.y + 14.0f * scale, 20.0f * scale, 25.0f * scale};
    }
    if (widget == REACH_WIDGET_NOW_PLAYING)
    {
        const float cell = card.width / 3.0f;
        return {card.x + cell * (button + 0.5f) - 18.0f * scale,
                card.y + card.height - 48.0f * scale, 36.0f * scale, 30.0f * scale};
    }
    const bool stopwatch = widget == REACH_WIDGET_STOPWATCH;
    const float cell = card.width / (stopwatch ? 2.0f : 3.0f);
    const float inset = (stopwatch ? 16.0f : 12.0f) * scale;
    return {card.x + button * cell + inset,
            card.y + card.height - (stopwatch ? 48.0f : 46.0f) * scale,
            cell - 2.0f * inset, 30.0f * scale};
}

static uint64_t reach_widgets_control_target(const reach_widgets *widgets, float x, float y)
{
    const int32_t index = widgets->drag_index;
    if (index < 0 || widgets->resizing)
    {
        return REACH_PRESSABLE_TARGET_NONE;
    }

    uint64_t first;
    int32_t count;
    switch (index)
    {
    case REACH_WIDGET_STOPWATCH:
        first = REACH_WIDGET_CONTROL_STOPWATCH_START;
        count = 2;
        break;
    case REACH_WIDGET_POMODORO:
        first = REACH_WIDGET_CONTROL_POMODORO_START;
        count = 3;
        break;
    case REACH_WIDGET_NOW_PLAYING:
        if (!widgets->media_snapshot.has_session)
        {
            return REACH_PRESSABLE_TARGET_NONE;
        }
        first = REACH_WIDGET_CONTROL_MEDIA_PREVIOUS;
        count = 3;
        break;
    case REACH_WIDGET_CALENDAR:
        first = REACH_WIDGET_CONTROL_CALENDAR_PREVIOUS;
        count = 2;
        break;
    default:
        return REACH_PRESSABLE_TARGET_NONE;
    }
    for (int32_t button = 0; button < count; ++button)
    {
        if (reach_widgets_contains(
                reach_widgets_control_bounds(widgets->cards[index], index, button,
                                             widgets->dpi_scale), x, y))
        {
            return first + button;
        }
    }
    return REACH_PRESSABLE_TARGET_NONE;
}

static void reach_widgets_update_drag(reach_widgets *widgets, float x, float y, float scale)
{
    const float dx = x - widgets->drag_x;
    const float dy = y - widgets->drag_y;
    if (!widgets->moved && dx * dx + dy * dy <= 16.0f * scale * scale)
    {
        return;
    }
    widgets->moved = 1;
    reach_pressable_result ignored = {};
    reach_pressable_disarm(&widgets->control_pressable, nullptr, &ignored);

    const int32_t index = widgets->drag_index;
    const reach_rect_f32 desktop = {0, 0, widgets->desktop_bounds.width,
                                    widgets->desktop_bounds.height};
    auto card = widgets->drag_origin;
    if (widgets->resizing)
    {
        const auto limits = reach_config_widget_size_limits(index);
        const float maximum_width =
            std::min(limits.max_width * scale, desktop.width - card.x);
        const float maximum_height =
            std::min(limits.max_height * scale, desktop.height - card.y);
        card.width = std::clamp(card.width + dx,
                                std::min(limits.min_width * scale, maximum_width), maximum_width);
        card.height = std::clamp(card.height + dy,
                                 std::min(limits.min_height * scale, maximum_height), maximum_height);
    }
    else
    {
        card.x = std::clamp(card.x + dx, 0.0f, std::max(0.0f, desktop.width - card.width));
        card.y = std::clamp(card.y + dy, 0.0f, std::max(0.0f, desktop.height - card.height));
    }
    widgets->cards[index] = card;
    widgets->drag_has_destination = reach_widgets_find_drop(
        desktop, card, widgets->cards, REACH_MAX_DESKTOP_WIDGETS, (size_t)index,
        widgets->resizing != 0, 14.0f * scale, scale, &widgets->drag_destination);
}

static void reach_widgets_desktop_pointer(void *capsule, const reach_pointer_event *event,
                                          reach_capsule_pointer_result *out)
{
    if (out == nullptr)
    {
        return;
    }
    *out = {};
    reach_widgets *widgets = static_cast<reach_widgets *>(capsule);
    if (widgets == nullptr || event == nullptr)
    {
        return;
    }
    float scale = widgets->dpi_scale > 0.0f ? widgets->dpi_scale : 1.0f;
    float x = (float)event->x;
    float y = (float)event->y;
    if (event->coordinate_space == REACH_POINTER_COORDINATE_SCREEN)
    {
        x -= widgets->desktop_bounds.x;
        y -= widgets->desktop_bounds.y;
    }
    if (event->kind == REACH_POINTER_EVENT_DOWN && event->button == REACH_POINTER_BUTTON_PRIMARY)
    {
        for (int32_t index = REACH_MAX_DESKTOP_WIDGETS - 1; index >= 0; --index)
        {
            if ((widgets->enabled_mask & (1u << index)) == 0 ||
                !reach_widgets_contains(widgets->cards[index], x, y))
            {
                continue;
            }
            widgets->drag_index = index;
            widgets->drag_x = x;
            widgets->drag_y = y;
            widgets->drag_origin = widgets->cards[index];
            widgets->resizing =
                x > widgets->cards[index].x + widgets->cards[index].width - 22.0f * scale &&
                y > widgets->cards[index].y + widgets->cards[index].height - 22.0f * scale;
            widgets->moved = 0;
            widgets->drag_has_destination = false;
            if (!widgets->resizing)
            {
                reach_pressable_result press = {};
                reach_pressable_press(&widgets->control_pressable, event->button,
                                      reach_widgets_control_target(widgets, x, y),
                                      REACH_PRESSABLE_FEEDBACK_NONE, nullptr, &press);
            }
            out->handled = 1;
            out->capture = 1;
            out->sync_pointer_subscriptions = 1;
            return;
        }
    }
    if (widgets->drag_index < 0)
    {
        return;
    }
    int32_t index = widgets->drag_index;
    if (event->kind == REACH_POINTER_EVENT_MOVE)
    {
        reach_pressable_result press = {};
        reach_pressable_update(&widgets->control_pressable,
                               reach_widgets_control_target(widgets, x, y), &press);
        reach_widgets_update_drag(widgets, x, y, scale);
        out->redraw = widgets->moved;
        out->handled = 1;
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_UP || event->kind == REACH_POINTER_EVENT_CANCEL)
    {
        reach_pressable_result press = {};
        if (event->kind == REACH_POINTER_EVENT_UP)
        {
            reach_widgets_update_drag(widgets, x, y, scale);
            reach_pressable_release(&widgets->control_pressable, REACH_POINTER_BUTTON_PRIMARY,
                                    reach_widgets_control_target(widgets, x, y), nullptr, &press);
        }
        else
        {
            reach_pressable_cancel(&widgets->control_pressable, nullptr, &press);
        }
        if (press.activated)
        {
            double now = reach_motion_now_seconds();
            if (press.activated_target == REACH_WIDGET_CONTROL_STOPWATCH_RESET)
            {
                widgets->stopwatch_elapsed = 0.0;
                widgets->stopwatch_running = 0;
                widgets->stopwatch_deadline = 0.0;
            }
            else if (press.activated_target == REACH_WIDGET_CONTROL_STOPWATCH_START && widgets->stopwatch_running)
            {
                widgets->stopwatch_elapsed += now - widgets->stopwatch_started;
                widgets->stopwatch_running = 0;
                widgets->stopwatch_deadline = 0.0;
            }
            else if (press.activated_target == REACH_WIDGET_CONTROL_STOPWATCH_START)
            {
                widgets->stopwatch_started = now;
                widgets->stopwatch_deadline = now + 0.1;
                widgets->stopwatch_running = 1;
            }
            else if (press.activated_target >= REACH_WIDGET_CONTROL_POMODORO_START &&
                     press.activated_target <= REACH_WIDGET_CONTROL_POMODORO_RESET)
            {
                if (press.activated_target == REACH_WIDGET_CONTROL_POMODORO_RESET)
                    widgets->pomodoro = {};
                else if (press.activated_target == REACH_WIDGET_CONTROL_POMODORO_SKIP)
                    reach_pomodoro_advance(widgets->pomodoro, false);
                else
                    reach_pomodoro_toggle(widgets->pomodoro, now);
                widgets->productivity_deadline = now + 1;
                widgets->pomodoro_completed = false;
            }
            else if (press.activated_target >= REACH_WIDGET_CONTROL_MEDIA_PREVIOUS &&
                     press.activated_target <= REACH_WIDGET_CONTROL_MEDIA_NEXT)
            {
                static const reach_now_playing_action actions[] = {
                    REACH_NOW_PLAYING_ACTION_PREVIOUS, REACH_NOW_PLAYING_ACTION_PLAY_PAUSE,
                    REACH_NOW_PLAYING_ACTION_NEXT};
                out->action.kind = REACH_FEATURE_ACTION_MEDIA_CONTROL;
                out->action.id = actions[press.activated_target - REACH_WIDGET_CONTROL_MEDIA_PREVIOUS];
            }
            else if (press.activated_target == REACH_WIDGET_CONTROL_CALENDAR_PREVIOUS ||
                     press.activated_target == REACH_WIDGET_CONTROL_CALENDAR_NEXT)
            {
                const int32_t delta =
                    press.activated_target == REACH_WIDGET_CONTROL_CALENDAR_PREVIOUS ? -1 : 1;
                widgets->calendar_month_offset =
                    std::clamp(widgets->calendar_month_offset + delta, -1200, 1200);
            }
        }
        if (widgets->moved)
        {
            const bool placed = event->kind == REACH_POINTER_EVENT_UP && widgets->drag_has_destination;
            auto &card = widgets->cards[index];
            card = placed ? widgets->drag_destination : widgets->drag_origin;
            if (placed && !reach_rect_equal(card, widgets->drag_origin))
            {
                const reach_config_widget_placement placement = {
                    (int32_t)std::lround(card.x / scale), (int32_t)std::lround(card.y / scale),
                    (int32_t)std::lround(card.width / scale), (int32_t)std::lround(card.height / scale)};
                widgets->placements[index] = placement;
                if (widgets->routes.placement_changed != nullptr)
                {
                    widgets->routes.placement_changed(widgets->routes.user, (size_t)index, placement);
                }
            }
        }
        widgets->drag_index = -1;
        out->relayout = widgets->layout_dirty;
        out->handled = 1;
        out->capture = -1;
        out->sync_pointer_subscriptions = 1;
        out->redraw = 1;
    }
}

const reach_feature_capsule_ops *reach_widgets_desktop_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {reach_widgets_reset,
                                                  reach_widgets_tick,
                                                  reach_widgets_desktop_open,
                                                  nullptr,
                                                  nullptr,
                                                  reach_widgets_desktop_tracking,
                                                  reach_widgets_desktop_pointer,
                                                  reach_widgets_desktop_tracking,
                                                  reach_widgets_desktop_input_regions,
                                                  reach_widgets_desktop_geometry,
                                                  reach_widgets_desktop_tracking};
    return &ops;
}

reach_result reach_widgets_create(reach_widgets **out_widgets)
{
    if (out_widgets == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_widgets = new (std::nothrow) reach_widgets();
    if (*out_widgets == nullptr)
    {
        return REACH_ERROR;
    }
    (*out_widgets)->drag_index = -1;
    reach_pressable_init(&(*out_widgets)->control_pressable);
    return REACH_OK;
}

void reach_widgets_destroy(reach_widgets *widgets)
{
    delete widgets;
}

void reach_widgets_attach_services(reach_widgets *widgets, reach_clock *clock,
                                   reach_system_stats *stats,
                                   reach_now_playing_service *now_playing)
{
    if (widgets != nullptr)
    {
        widgets->clock = clock;
        widgets->stats = stats;
        widgets->now_playing = now_playing;
    }
}

void reach_widgets_set_routes(reach_widgets *widgets, const reach_widgets_routes *routes)
{
    if (widgets != nullptr)
    {
        widgets->routes = routes != nullptr ? *routes : reach_widgets_routes{};
    }
}

int32_t reach_widgets_set_placement(reach_widgets *widgets, size_t index,
                                    reach_config_widget_placement placement)
{
    if (widgets == nullptr || index >= REACH_MAX_DESKTOP_WIDGETS ||
        std::memcmp(&widgets->placements[index], &placement, sizeof(placement)) == 0)
    {
        return 0;
    }
    widgets->placements[index] = placement;
    widgets->layout_dirty = true;
    return 1;
}

int32_t reach_widgets_set_enabled(reach_widgets *widgets, uint32_t mask)
{
    if (widgets == nullptr || widgets->enabled_mask == (mask & REACH_DESKTOP_WIDGET_MASK))
    {
        return 0;
    }
    widgets->enabled_mask = mask & REACH_DESKTOP_WIDGET_MASK;
    widgets->layout_dirty = true;
    reach_system_stats_set_gpu_enabled(widgets->stats,
                                       (widgets->enabled_mask & (1u << REACH_WIDGET_SYSTEM)) != 0);
    return 1;
}

int32_t reach_widgets_arrange_desktop(reach_widgets *widgets, reach_rect_f32 monitor,
                                      float dpi_scale)
{
    if (widgets == nullptr)
    {
        return 0;
    }
    const float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    const bool monitor_changed = !reach_rect_equal(widgets->desktop_bounds, monitor) ||
                                 widgets->dpi_scale != scale;
    widgets->desktop_bounds = monitor;
    widgets->dpi_scale = scale;
    widgets->layout_dirty |= monitor_changed;
    if (widgets->drag_index >= 0 || !widgets->layout_dirty)
    {
        return monitor_changed;
    }
    widgets->layout_dirty = false;
    const float widths[REACH_MAX_DESKTOP_WIDGETS] = {244, 320, 300, 300, 280, 280};
    const float heights[REACH_MAX_DESKTOP_WIDGETS] = {144, 184, 196, 300, 160, 200};
    reach_widget_layout_item items[REACH_MAX_DESKTOP_WIDGETS] = {};
    for (size_t index = 0; index < REACH_MAX_DESKTOP_WIDGETS; ++index)
    {
        const auto &placement = widgets->placements[index];
        auto limits = reach_config_widget_size_limits(index);
        auto &item = items[index];
        item.enabled = (widgets->enabled_mask & (1u << index)) != 0;
        item.custom = placement.width > 0 && placement.x >= 0 && placement.y >= 0;
        item.preferred = {
            placement.x * scale, placement.y * scale,
            std::clamp(placement.width > 0 ? placement.width * scale : widths[index] * scale,
                       limits.min_width * scale, limits.max_width * scale),
            std::clamp(placement.height > 0 ? placement.height * scale : heights[index] * scale,
                       limits.min_height * scale, limits.max_height * scale)};
        item.minimum = {limits.min_width * scale, limits.min_height * scale};
    }
    reach_rect_f32 arranged[REACH_MAX_DESKTOP_WIDGETS] = {};
    reach_rect_f32 desktop = {0, 0, monitor.width, monitor.height};
    reach_rect_f32 automatic = {24.0f * scale, 84.0f * scale,
                                std::max(0.0f, monitor.width - 48.0f * scale),
                                std::max(0.0f, monitor.height - 108.0f * scale)};
    (void)reach_widgets_pack(desktop, automatic, items, REACH_MAX_DESKTOP_WIDGETS, 14.0f * scale,
                             arranged);
    bool changed = std::memcmp(arranged, widgets->cards, sizeof(arranged)) != 0;
    std::memcpy(widgets->cards, arranged, sizeof(arranged));
    return changed || monitor_changed;
}

static void reach_widgets_rect(reach_render_command_buffer *commands, reach_rect_f32 rect,
                               float radius, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = rect;
    command.radius = radius;
    command.color = color;
    (void)reach_render_command_buffer_push(commands, &command);
}

static void reach_widgets_text(reach_render_command_buffer *commands, reach_rect_f32 rect,
                               const uint16_t *value, float size, int32_t weight, reach_color color,
                               int32_t alignment = REACH_TEXT_ALIGNMENT_LEADING)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = rect;
    command.text_size = size;
    command.text_weight = weight;
    command.text_alignment = alignment;
    command.text_ellipsis = 1;
    command.color = color;
    (void)reach_copy_utf16(command.text, 260, value);
    (void)reach_render_command_buffer_push(commands, &command);
}

static void reach_widgets_card(reach_render_command_buffer *commands, const reach_theme *theme,
                               reach_rect_f32 rect, float scale, reach_color accent)
{
    float border = reach_theme_border_thickness(theme, scale);
    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect = rect;
    shape.radius = theme->radius_large * scale;
    auto background = theme->popup_background;
    background.r = background.r * 0.88f + accent.r * 0.12f;
    background.g = background.g * 0.88f + accent.g * 0.12f;
    background.b = background.b * 0.88f + accent.b * 0.12f;
    (void)reach_render_push_bordered_background(commands, &shape, background,
                                                theme->popup_border, border, &theme->popup_shadow,
                                                scale);
}

static void reach_widgets_decorate(reach_render_command_buffer *commands, reach_rect_f32 inner,
                                  float radius, reach_color accent)
{
    reach_render_command tint = {};
    tint.type = REACH_RENDER_COMMAND_RECT;
    tint.rect = inner;
    tint.radius = radius;
    tint.color = accent;
    tint.color.a = 0.16f;
    tint.fill_gradient = REACH_RENDER_FILL_EASED_FADE_DOWN;
    (void)reach_render_command_buffer_push(commands, &tint);

    const float span = inner.width * 1.38f;
    for (int32_t wave = 0; wave < 2; ++wave)
    {
        reach_render_command ribbon = {};
        ribbon.type = REACH_RENDER_COMMAND_CLIPPED_ROUNDED_RECT;
        ribbon.rect = {inner.x + inner.width * (wave == 0 ? -0.55f : 0.16f),
                       inner.y + inner.height * (wave == 0 ? 0.64f : 0.76f),
                       span, span};
        ribbon.radius = span * 0.5f;
        ribbon.color = accent;
        ribbon.color.a = wave == 0 ? 0.09f : 0.08f;
        ribbon.clip_rect = inner;
        ribbon.clip_radius = radius;
        (void)reach_render_command_buffer_push(commands, &ribbon);
    }
}

static reach_color reach_widgets_intensity_color(float value)
{
    float amount = std::clamp(value / 100.0f, 0.0f, 1.0f);
    return {std::min(0.96f, 0.20f + amount * 1.52f),
            std::min(0.80f, 0.24f + (1.0f - amount) * 1.12f), 0.20f, 1.0f};
}

static void reach_widgets_meter(reach_render_command_buffer *commands, const reach_theme *theme,
                                reach_rect_f32 card, float y, const wchar_t *label, float value,
                                float scale, bool available)
{
    wchar_t amount[24] = {};
    if (available)
    {
        swprintf_s(amount, L"%.0f%%", value);
    }
    else
    {
        wcscpy_s(amount, L"—");
    }
    reach_widgets_text(commands, {card.x + 18.0f * scale, y, 68.0f * scale, 22.0f * scale},
                       reinterpret_cast<const uint16_t *>(label), REACH_TEXT_SIZE_SMALL * scale,
                       REACH_TEXT_WEIGHT_SEMIBOLD, theme->primary_text);
    reach_widgets_text(commands,
                       {card.x + card.width - 57.0f * scale, y, 42.0f * scale, 22.0f * scale},
                       reinterpret_cast<const uint16_t *>(amount), REACH_TEXT_SIZE_SMALL * scale,
                       REACH_TEXT_WEIGHT_SEMIBOLD, theme->primary_text);
    float track_width = std::max(0.0f, card.width - 142.0f * scale);
    float gap = 3.0f * scale;
    float pill_width = 8.0f * scale;
    int32_t segments = std::max(1, (int32_t)((track_width + gap) / (pill_width + gap)));
    float amount_filled = available ? std::clamp(value / 100.0f, 0.0f, 1.0f) * segments : 0.0f;
    for (int32_t segment = 0; segment < segments; ++segment)
    {
        float height = 10.0f * scale;
        reach_rect_f32 pill = {card.x + 77.0f * scale + segment * (pill_width + gap),
                               y + (22.0f * scale - height) * 0.5f, pill_width, height};
        reach_color color =
            segment < amount_filled
                ? reach_widgets_intensity_color(segment * 100.0f / std::max(1, segments - 1))
                : theme->icon_box_background;
        reach_widgets_rect(commands, pill, 2.5f * scale, color);
    }
}

static void reach_widgets_system_render(reach_render_command_buffer *commands,
                                        const reach_theme *theme, reach_rect_f32 card,
                                        reach_rect_f32 title, float scale,
                                        const reach_system_stats_snapshot &stats)
{
    reach_widgets_text(commands, title, (const uint16_t *)u"System monitor",
                       REACH_TEXT_SIZE_SMALL * scale, REACH_TEXT_WEIGHT_SEMIBOLD,
                       theme->primary_text);
    const float first_row = card.y + 43.0f * scale;
    const float row_gap = 26.0f * scale;
    reach_widgets_meter(commands, theme, card, first_row, L"CPU", stats.cpu_percent, scale,
                        stats.valid != 0);
    reach_widgets_meter(commands, theme, card, first_row + row_gap, L"RAM", stats.memory_percent,
                        scale, stats.valid != 0);
    reach_widgets_meter(commands, theme, card, first_row + 2.0f * row_gap, L"GPU",
                        stats.gpu_percent, scale, stats.gpu_available != 0);

    wchar_t temperature[48] = {};
    auto tint = theme->icon_box_background;
    if (stats.gpu_temperature_available)
    {
        swprintf_s(temperature, L"GPU temperature   %.0f°C", stats.gpu_temperature_c);
        tint = reach_widgets_intensity_color((stats.gpu_temperature_c - 40.0f) * 2.0f);
        tint.a = 0.17f;
    }
    else
    {
        wcscpy_s(temperature, L"GPU temperature   Unavailable");
    }
    const reach_rect_f32 chip = {title.x, card.y + 126.0f * scale,
                                 title.width - 12.0f * scale, 26.0f * scale};
    reach_widgets_rect(commands, chip, 8.0f * scale, tint);
    reach_widgets_text(commands,
                       {chip.x + 9.0f * scale, chip.y, chip.width - 18.0f * scale, chip.height},
                       reinterpret_cast<const uint16_t *>(temperature),
                       REACH_TEXT_SIZE_SMALL * scale, REACH_TEXT_WEIGHT_SEMIBOLD,
                       theme->primary_text);
}

static int32_t reach_widgets_days_in_month(int32_t year, int32_t month)
{
    static const int32_t days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int32_t count = days[month - 1];
    if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)))
    {
        ++count;
    }
    return count;
}

static int32_t reach_widgets_month_weekday(int32_t year, int32_t month)
{
    static const int32_t offsets[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3)
    {
        --year;
    }
    return (year + year / 4 - year / 100 + year / 400 + offsets[month - 1] + 1) % 7;
}

static void reach_widgets_resize_grip(reach_render_command_buffer *commands,
                                      const reach_theme *theme, reach_rect_f32 rect, float scale)
{
    reach_color grip = theme->primary_text;
    grip.a = 0.28f;
    reach_widgets_rect(commands,
                       {rect.x + rect.width - 24.0f * scale, rect.y + rect.height - 12.0f * scale,
                        11.0f * scale, 2.0f * scale},
                       1.0f * scale, grip);
}

static void reach_widgets_pomodoro_render(const reach_widgets *widgets,
                                              const reach_theme *theme, reach_rect_f32 rect,
                                              float scale, reach_color accent,
                                              reach_render_command_buffer *commands)
{
    reach_rect_f32 title = {rect.x + 16 * scale, rect.y + 14 * scale, rect.width - REACH_TEXT_SIZE_XLARGE * scale,
                            25 * scale};
    wchar_t buffer[260] = {};
    const wchar_t *phase = widgets->pomodoro.phase == 0   ? L"Focus"
                           : widgets->pomodoro.phase == 1 ? L"Short break"
                                                          : L"Long break";
    swprintf_s(buffer, L"Pomodoro · %ls", phase);
    reach_widgets_text(commands, title, reinterpret_cast<const uint16_t *>(buffer),
                       REACH_TEXT_SIZE_SMALL * scale, REACH_TEXT_WEIGHT_SEMIBOLD,
                       theme->primary_text);
    uint64_t seconds = (uint64_t)std::ceil(
        reach_pomodoro_remaining(widgets->pomodoro, reach_motion_now_seconds()));
    swprintf_s(buffer, L"%02llu:%02llu", seconds / 60, seconds % 60);
    reach_widgets_text(commands, {title.x, rect.y + 52 * scale, title.width, 43 * scale},
                       reinterpret_cast<const uint16_t *>(buffer), REACH_TEXT_SIZE_XLARGE * scale,
                       REACH_TEXT_WEIGHT_DEMIBOLD, theme->primary_text,
                       REACH_TEXT_ALIGNMENT_CENTER);
    const wchar_t *session = L"Session %d";
    if (widgets->pomodoro.phase != 0)
    {
        session = L"Next focus session %d";
    }
    else if (widgets->pomodoro_completed)
    {
        session = L"Break complete · session %d ready";
    }
    swprintf_s(buffer, session, widgets->pomodoro.completed + 1);
    reach_widgets_text(commands, {title.x, rect.y + 100 * scale, title.width, 22 * scale},
                       reinterpret_cast<const uint16_t *>(buffer),
                       REACH_TEXT_SIZE_XSMALL * scale, REACH_TEXT_WEIGHT_NORMAL,
                       theme->primary_text, REACH_TEXT_ALIGNMENT_CENTER);
    float duration = widgets->pomodoro.phase == 0   ? 1500.0f
                     : widgets->pomodoro.phase == 1 ? 300.0f
                                                    : 900.0f;
    reach_rect_f32 track = {title.x, rect.y + rect.height - 63 * scale, title.width, 4 * scale};
    reach_widgets_rect(commands, track, 0, theme->icon_box_background);
    track.width *=
        1.0f - (float)reach_pomodoro_remaining(widgets->pomodoro, reach_motion_now_seconds()) /
                   duration;
    if (track.width > 0)
        reach_widgets_rect(commands, track, 0, accent);
    constexpr int count = 3;
    const wchar_t *const labels[count] = {
        widgets->pomodoro.running ? L"Pause" : L"Start", L"Skip", L"Reset"};
    for (int button = 0; button < count; ++button)
    {
        const auto bounds = reach_widgets_control_bounds(rect, REACH_WIDGET_POMODORO, button, scale);
        reach_color wash = accent;
        wash.a = 0.15f;
        reach_widgets_rect(commands, bounds, 8 * scale, wash);
        reach_widgets_text(commands, bounds, reinterpret_cast<const uint16_t *>(labels[button]),
                           REACH_TEXT_SIZE_SMALL * scale, REACH_TEXT_WEIGHT_SEMIBOLD,
                           theme->primary_text, REACH_TEXT_ALIGNMENT_CENTER);
    }
    reach_widgets_resize_grip(commands, theme, rect, scale);
}

reach_result reach_widgets_render_desktop(const reach_widgets *widgets, const reach_theme *theme,
                                          float dpi_scale, reach_render_command_buffer *commands)
{
    if (widgets == nullptr || commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    const reach_theme *actual = theme != nullptr ? theme : reach_theme_default();
    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    for (size_t draw = 0; draw < REACH_MAX_DESKTOP_WIDGETS; ++draw)
    {
        size_t index = draw;
        if (widgets->drag_index >= 0)
            index = draw + 1 < REACH_MAX_DESKTOP_WIDGETS
                        ? (draw < (size_t)widgets->drag_index ? draw : draw + 1)
                        : (size_t)widgets->drag_index;
        if ((widgets->enabled_mask & (1u << index)) == 0)
        {
            continue;
        }
        reach_rect_f32 rect = widgets->cards[index];
        if (rect.width <= 0 || rect.height <= 0)
            continue;
        if (widgets->moved && (int32_t)index == widgets->drag_index)
        {
            const auto target = widgets->drag_has_destination ? widgets->drag_destination
                                                               : widgets->drag_origin;
            if (!reach_rect_equal(rect, target))
            {
                auto marker = actual->accent;
                marker.a = 0.28f;
                reach_widgets_rect(commands, target, actual->radius_large * scale, marker);
            }
        }
        reach_widgets_card(commands, actual, rect, scale, actual->accent);
        const auto inner = reach_theme_border_content_rect(actual, scale, rect);
        const float inner_radius = std::max(
            0.0f, actual->radius_large * scale - reach_theme_border_thickness(actual, scale));
        const bool has_artwork = index == REACH_WIDGET_NOW_PLAYING &&
                                 widgets->media_snapshot.cover_image_id != 0;
        if (!has_artwork)
        {
            reach_widgets_decorate(commands, inner, inner_radius, actual->accent);
        }
        if (index == REACH_WIDGET_POMODORO)
        {
            reach_widgets_pomodoro_render(widgets, actual, rect, scale, actual->accent,
                                              commands);
            continue;
        }
        reach_rect_f32 title = {rect.x + 16.0f * scale, rect.y + 14.0f * scale,
                                rect.width - 32.0f * scale, 25.0f * scale};
        reach_rect_f32 value = {title.x, rect.y + 51.0f * scale, title.width, 42.0f * scale};
        wchar_t buffer[128] = {};
        if (index == REACH_WIDGET_CLOCK)
        {
            reach_widgets_text(commands, title, reinterpret_cast<const uint16_t *>(L"Clock & date"),
                               REACH_TEXT_SIZE_SMALL * scale, REACH_TEXT_WEIGHT_SEMIBOLD,
                               actual->primary_text);
            if (widgets->clock_snapshot.valid)
            {
                swprintf_s(buffer, L"%02d:%02d", widgets->clock_snapshot.hour,
                           widgets->clock_snapshot.minute);
            }
            else
            {
                wcscpy_s(buffer, L"Time unavailable");
            }
        }
        else if (index == REACH_WIDGET_SYSTEM)
        {
            reach_widgets_system_render(commands, actual, rect, title, scale,
                                        widgets->stats_snapshot);
            reach_widgets_resize_grip(commands, actual, rect, scale);
            continue;
        }
        else if (index == REACH_WIDGET_STOPWATCH)
        {
            reach_widgets_text(commands, title, reinterpret_cast<const uint16_t *>(L"Stopwatch"),
                               REACH_TEXT_SIZE_SMALL * scale, REACH_TEXT_WEIGHT_SEMIBOLD,
                               actual->primary_text);
            double seconds = widgets->stopwatch_elapsed +
                             (widgets->stopwatch_running
                                  ? reach_motion_now_seconds() - widgets->stopwatch_started
                                  : 0.0);
            uint64_t tenths = (uint64_t)(std::max(0.0, seconds) * 10.0);
            swprintf_s(buffer, L"%02llu:%02llu.%llu", tenths / 600, tenths / 10 % 60, tenths % 10);
            reach_widgets_text(commands, value, reinterpret_cast<const uint16_t *>(buffer),
                               REACH_TEXT_SIZE_XLARGE * scale, REACH_TEXT_WEIGHT_DEMIBOLD, actual->primary_text,
                               REACH_TEXT_ALIGNMENT_CENTER);
            for (int32_t button = 0; button < 2; ++button)
            {
                const auto bounds = reach_widgets_control_bounds(rect, index, button, scale);
                reach_color wash = actual->accent;
                wash.a = 0.16f;
                reach_widgets_rect(commands, bounds, 8.0f * scale, wash);
                reach_widgets_text(commands, bounds,
                                   reinterpret_cast<const uint16_t *>(button == 1 ? L"Reset"
                                                                      : widgets->stopwatch_running
                                                                          ? L"Pause"
                                                                          : L"Start"),
                                   REACH_TEXT_SIZE_SMALL * scale, REACH_TEXT_WEIGHT_SEMIBOLD,
                                   actual->primary_text, REACH_TEXT_ALIGNMENT_CENTER);
            }
            reach_widgets_resize_grip(commands, actual, rect, scale);
            continue;
        }
        else if (index == REACH_WIDGET_CALENDAR)
        {
            static const wchar_t *months[12] = {
                L"January", L"February", L"March",     L"April",   L"May",      L"June",
                L"July",    L"August",   L"September", L"October", L"November", L"December"};
            int32_t year = widgets->clock_snapshot.valid ? widgets->clock_snapshot.year : 2026;
            int32_t month = widgets->clock_snapshot.valid ? widgets->clock_snapshot.month : 1;
            int32_t shifted = (year * 12 + month - 1) + widgets->calendar_month_offset;
            year = shifted / 12;
            month = shifted % 12 + 1;
            swprintf_s(buffer, L"%ls %d", months[month - 1], year);
            reach_widgets_text(commands, title, reinterpret_cast<const uint16_t *>(buffer),
                               REACH_TEXT_SIZE_MEDIUM * scale, REACH_TEXT_WEIGHT_SEMIBOLD,
                               actual->primary_text, REACH_TEXT_ALIGNMENT_CENTER);
            reach_widgets_text(commands, reach_widgets_control_bounds(rect, index, 0, scale),
                               reinterpret_cast<const uint16_t *>(L"‹"),
                               REACH_TEXT_SIZE_MEDIUM * scale, REACH_TEXT_WEIGHT_SEMIBOLD,
                               actual->primary_text, REACH_TEXT_ALIGNMENT_CENTER);
            reach_widgets_text(
                commands,
                reach_widgets_control_bounds(rect, index, 1, scale),
                reinterpret_cast<const uint16_t *>(L"›"), REACH_TEXT_SIZE_MEDIUM * scale,
                REACH_TEXT_WEIGHT_SEMIBOLD, actual->primary_text, REACH_TEXT_ALIGNMENT_CENTER);
            const wchar_t *weekdays[7] = {L"S", L"M", L"T", L"W", L"T", L"F", L"S"};
            float cell_width = (rect.width - 36.0f * scale) / 7.0f;
            float cell_height = (rect.height - 96.0f * scale) / 6.0f;
            int32_t first_weekday = reach_widgets_month_weekday(year, month);
            for (int32_t column = 0; column < 7; ++column)
            {
                reach_rect_f32 cell = {rect.x + 18.0f * scale + column * cell_width,
                                       rect.y + 50.0f * scale, cell_width, 22.0f * scale};
                reach_widgets_text(commands, cell,
                                   reinterpret_cast<const uint16_t *>(weekdays[column]),
                                   REACH_TEXT_SIZE_XSMALL * scale, REACH_TEXT_WEIGHT_SEMIBOLD,
                                   actual->primary_text);
            }
            int32_t days = reach_widgets_days_in_month(year, month);
            for (int32_t day = 1; day <= days; ++day)
            {
                int32_t slot = first_weekday + day - 1;
                int32_t row = slot / 7;
                int32_t column = slot % 7;
                reach_rect_f32 cell = {rect.x + 18.0f * scale + column * cell_width,
                                       rect.y + 76.0f * scale + row * cell_height,
                                       cell_width - 3.0f * scale, cell_height - 3.0f * scale};
                int32_t today = widgets->clock_snapshot.valid &&
                                widgets->calendar_month_offset == 0 &&
                                day == widgets->clock_snapshot.day;
                if (today)
                {
                    reach_widgets_rect(commands, cell, 8.0f * scale, actual->accent);
                }
                swprintf_s(buffer, L"%d", day);
                reach_widgets_text(commands,
                                   {cell.x + 7.0f * scale, cell.y + 5.0f * scale,
                                    cell.width - 8.0f * scale, cell.height - 4.0f * scale},
                                   reinterpret_cast<const uint16_t *>(buffer),
                                   REACH_TEXT_SIZE_SMALL * scale,
                                   today ? REACH_TEXT_WEIGHT_SEMIBOLD : REACH_TEXT_WEIGHT_NORMAL,
                                   actual->primary_text);
            }
            reach_widgets_resize_grip(commands, actual, rect, scale);
            continue;
        }
        else
        {
            if (has_artwork)
            {
                reach_now_playing_artwork_render_input artwork = {};
                artwork.bounds = inner;
                artwork.radius = inner_radius;
                artwork.blur = REACH_NOW_PLAYING_ARTWORK_BLUR;
                artwork.cover_image_id = widgets->media_snapshot.cover_image_id;
                artwork.overlay = actual->popup_background;
                artwork.overlay.a = 0.48f;
                (void)reach_now_playing_artwork_build_render_commands(&artwork, commands);
                reach_widgets_decorate(commands, inner, inner_radius, actual->accent);

                reach_render_command cover = {};
                cover.type = REACH_RENDER_COMMAND_ICON;
                cover.rect = {rect.x + 16.0f * scale, rect.y + 51.0f * scale,
                              62.0f * scale, 62.0f * scale};
                cover.icon_id = artwork.cover_image_id;
                cover.icon_crop_to_fill = 1;
                cover.radius = 12.0f * scale;
                cover.corner_mask = REACH_RENDER_CORNER_ALL;
                cover.color.a = 1.0f;
                (void)reach_render_command_buffer_push(commands, &cover);
            }
            reach_widgets_text(commands, title, reinterpret_cast<const uint16_t *>(L"Now playing"),
                               REACH_TEXT_SIZE_SMALL * scale, REACH_TEXT_WEIGHT_SEMIBOLD,
                               actual->primary_text);
            if (widgets->media_snapshot.has_session)
            {
                reach_rect_f32 track = {rect.x + 90.0f * scale, rect.y + 54.0f * scale,
                                        rect.width - 106.0f * scale, 27.0f * scale};
                reach_widgets_text(commands, track, widgets->media_snapshot.title,
                                   REACH_TEXT_SIZE_MEDIUM * scale, REACH_TEXT_WEIGHT_DEMIBOLD,
                                   actual->primary_text);
                track.y += 28.0f * scale;
                reach_widgets_text(commands, track, widgets->media_snapshot.artist,
                                   REACH_TEXT_SIZE_SMALL * scale, REACH_TEXT_WEIGHT_NORMAL,
                                   actual->primary_text);
                const reach_vector_icon_id controls[3] = {REACH_VECTOR_ICON_PREVIOUS,
                                                          widgets->media_snapshot.playback ==
                                                                  REACH_MEDIA_PLAYBACK_PLAYING
                                                              ? REACH_VECTOR_ICON_PAUSE
                                                              : REACH_VECTOR_ICON_PLAY,
                                                          REACH_VECTOR_ICON_NEXT};
                for (int32_t control = 0; control < 3; ++control)
                {
                    const auto button = reach_widgets_control_bounds(rect, index, control, scale);
                    reach_color wash = actual->accent;
                    wash.a = control == 1 ? 0.22f : 0.08f;
                    reach_widgets_rect(commands, button, 15.0f * scale, wash);
                    reach_render_command icon = {};
                    icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
                    icon.icon_id = controls[control];
                    icon.rect = {button.x + 8.0f * scale, button.y + 5.0f * scale, 20.0f * scale,
                                 20.0f * scale};
                    icon.color = actual->primary_text;
                    (void)reach_render_command_buffer_push(commands, &icon);
                }
                reach_widgets_resize_grip(commands, actual, rect, scale);
                continue;
            }
            value.x = rect.x + 90.0f * scale;
            value.width = rect.width - 106.0f * scale;
            wcscpy_s(buffer, L"Nothing playing");
        }
        reach_widgets_text(commands, value, reinterpret_cast<const uint16_t *>(buffer),
                           (index == REACH_WIDGET_CLOCK ? REACH_TEXT_SIZE_XLARGE : REACH_TEXT_SIZE_LARGE) * scale,
                           REACH_TEXT_WEIGHT_DEMIBOLD, actual->primary_text);
        if (index == REACH_WIDGET_CLOCK && widgets->clock_snapshot.valid)
        {
            swprintf_s(buffer, L"%04d / %02d / %02d", widgets->clock_snapshot.year,
                       widgets->clock_snapshot.month, widgets->clock_snapshot.day);
            reach_widgets_text(commands,
                               {rect.x + 18.0f * scale, rect.y + 102.0f * scale,
                                rect.width - 36.0f * scale, 24.0f * scale},
                               reinterpret_cast<const uint16_t *>(buffer),
                               REACH_TEXT_SIZE_SMALL * scale, REACH_TEXT_WEIGHT_NORMAL,
                               actual->primary_text);
        }
        reach_widgets_resize_grip(commands, actual, rect, scale);
    }
    return REACH_OK;
}
