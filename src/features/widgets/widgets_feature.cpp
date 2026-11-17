#include "reach/features/widgets.h"
#include "widgets_layout.h"
#include "widgets_chrome.h"
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

static constexpr double reach_widgets_stopwatch_interval = 0.1;
static constexpr double reach_widgets_pomodoro_interval = 1.0;

struct reach_widget_timer
{
    reach_desktop_widget widget;
    double reach_widgets::*deadline;
    double interval;
    bool (*running)(const reach_widgets &widgets);
    void (*advance)(reach_widgets &widgets, double now);
};

static const reach_widget_timer reach_widgets_timers[] = {
    {REACH_WIDGET_STOPWATCH, &reach_widgets::stopwatch_deadline, reach_widgets_stopwatch_interval,
     [](const reach_widgets &w) { return w.stopwatch_running != 0; }, nullptr},
    {REACH_WIDGET_POMODORO, &reach_widgets::productivity_deadline, reach_widgets_pomodoro_interval,
     [](const reach_widgets &w) { return w.pomodoro.running; }, [](reach_widgets &w, double now)
     { w.pomodoro_completed = reach_pomodoro_tick(w.pomodoro, now); }},
};

static bool reach_widgets_timer_active(const reach_widgets &widgets,
                                       const reach_widget_timer &timer)
{
    return (widgets.enabled_mask & (1u << timer.widget)) != 0 && timer.running(widgets);
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
    for (const auto &timer : reach_widgets_timers)
    {
        if (reach_widgets_timer_active(*widgets, timer) && now >= widgets->*(timer.deadline))
        {
            if (timer.advance != nullptr)
            {
                timer.advance(*widgets, now);
            }
            widgets->*(timer.deadline) = now + timer.interval;
            out->redraw = 1;
        }
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
        clock.month != widgets->clock_snapshot.month ||
        clock.year != widgets->clock_snapshot.year ||
        stats.valid != widgets->stats_snapshot.valid ||
        stats.cpu_percent != widgets->stats_snapshot.cpu_percent ||
        stats.memory_percent != widgets->stats_snapshot.memory_percent ||
        stats.gpu_percent != widgets->stats_snapshot.gpu_percent ||
        stats.cpu_clock_mhz != widgets->stats_snapshot.cpu_clock_mhz ||
        stats.memory_speed_mt_s != widgets->stats_snapshot.memory_speed_mt_s ||
        stats.gpu_clock_mhz != widgets->stats_snapshot.gpu_clock_mhz ||
        stats.gpu_clock_max_mhz != widgets->stats_snapshot.gpu_clock_max_mhz ||
        stats.vram_available != widgets->stats_snapshot.vram_available ||
        stats.vram_percent != widgets->stats_snapshot.vram_percent ||
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
    for (const auto &timer : reach_widgets_timers)
    {
        if (reach_widgets_timer_active(*widgets, timer))
        {
            const double value = widgets->*(timer.deadline);
            if (value > 0 && (out->deadline_seconds <= 0 || value < out->deadline_seconds))
            {
                out->deadline_seconds = value;
            }
        }
    }
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
        const auto card = widgets->cards[index];
        if ((widgets->enabled_mask & (1u << index)) && card.width > 0.0f && card.height > 0.0f)
        {
            out_regions[count++] = card;
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

enum class reach_widget_control_slot
{
    none,
    header_edges,
    footer_buttons,
    footer_icons
};

struct reach_widget_controls
{
    uint64_t first;
    int32_t count;
    reach_widget_control_slot slot;
};

static constexpr reach_widget_controls reach_widgets_controls[REACH_MAX_DESKTOP_WIDGETS] = {
    {},
    {},
    {REACH_WIDGET_CONTROL_MEDIA_PREVIOUS, 3, reach_widget_control_slot::footer_icons},
    {REACH_WIDGET_CONTROL_CALENDAR_PREVIOUS, 2, reach_widget_control_slot::header_edges},
    {REACH_WIDGET_CONTROL_STOPWATCH_START, 2, reach_widget_control_slot::footer_buttons},
    {REACH_WIDGET_CONTROL_POMODORO_START, 3, reach_widget_control_slot::footer_buttons},
};

static bool reach_widgets_has_footer(int32_t widget)
{
    const auto slot = reach_widgets_controls[widget].slot;
    return slot == reach_widget_control_slot::footer_buttons ||
           slot == reach_widget_control_slot::footer_icons;
}

static reach_rect_f32 reach_widgets_control_bounds(const reach_widget_chrome &chrome,
                                                   int32_t widget, int32_t button, float scale)
{
    const auto &controls = reach_widgets_controls[widget];
    if (controls.slot == reach_widget_control_slot::header_edges)
    {
        const float edge_width = 20.0f * scale;
        const float x =
            button == 0 ? chrome.header.x : chrome.header.x + chrome.header.width - edge_width;
        return {x, chrome.header.y, edge_width, chrome.header.height};
    }
    const auto &footer = chrome.footer;
    const float cell = footer.width / controls.count;
    if (controls.slot == reach_widget_control_slot::footer_icons)
    {
        const float icon_slot = 36.0f * scale;
        return {footer.x + cell * (button + 0.5f) - icon_slot * 0.5f, footer.y, icon_slot,
                footer.height};
    }
    const float gap = 8.0f * scale;
    return {footer.x + button * cell + gap * 0.5f, footer.y, cell - gap, footer.height};
}

static uint64_t reach_widgets_control_target(const reach_widgets *widgets, float x, float y)
{
    const int32_t index = widgets->drag_index;
    if (index < 0 || widgets->resizing ||
        (index == REACH_WIDGET_NOW_PLAYING && !widgets->media_snapshot.has_session) ||
        (index == REACH_WIDGET_CALENDAR && !widgets->clock_snapshot.valid))
    {
        return REACH_PRESSABLE_TARGET_NONE;
    }

    const auto &controls = reach_widgets_controls[index];
    const auto chrome = reach_widgets_chrome(widgets->cards[index], widgets->dpi_scale,
                                             reach_widgets_has_footer(index));
    for (int32_t button = 0; button < controls.count; ++button)
    {
        if (reach_rect_contains(
                reach_widgets_control_bounds(chrome, index, button, widgets->dpi_scale), x, y))
        {
            return controls.first + button;
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
        const auto limits = reach_widget_definition_at(index)->limits;
        const float maximum_width = std::min(limits.max_width * scale, desktop.width - card.x);
        const float maximum_height = std::min(limits.max_height * scale, desktop.height - card.y);
        card.width = std::clamp(card.width + dx, std::min(limits.min_width * scale, maximum_width),
                                maximum_width);
        card.height = std::clamp(
            card.height + dy, std::min(limits.min_height * scale, maximum_height), maximum_height);
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
                !reach_rect_contains(widgets->cards[index], x, y))
            {
                continue;
            }
            widgets->drag_index = index;
            widgets->drag_x = x;
            widgets->drag_y = y;
            widgets->drag_origin = widgets->cards[index];
            const auto chrome =
                reach_widgets_chrome(widgets->cards[index], scale, reach_widgets_has_footer(index));
            widgets->resizing = reach_rect_contains(chrome.resize, x, y);
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
            else if (press.activated_target == REACH_WIDGET_CONTROL_STOPWATCH_START &&
                     widgets->stopwatch_running)
            {
                widgets->stopwatch_elapsed += now - widgets->stopwatch_started;
                widgets->stopwatch_running = 0;
                widgets->stopwatch_deadline = 0.0;
            }
            else if (press.activated_target == REACH_WIDGET_CONTROL_STOPWATCH_START)
            {
                widgets->stopwatch_started = now;
                widgets->stopwatch_deadline = now + reach_widgets_stopwatch_interval;
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
                widgets->productivity_deadline = now + reach_widgets_pomodoro_interval;
                widgets->pomodoro_completed = false;
            }
            else if (press.activated_target >= REACH_WIDGET_CONTROL_MEDIA_PREVIOUS &&
                     press.activated_target <= REACH_WIDGET_CONTROL_MEDIA_NEXT)
            {
                static const reach_now_playing_action actions[] = {
                    REACH_NOW_PLAYING_ACTION_PREVIOUS, REACH_NOW_PLAYING_ACTION_PLAY_PAUSE,
                    REACH_NOW_PLAYING_ACTION_NEXT};
                out->action.kind = REACH_FEATURE_ACTION_MEDIA_CONTROL;
                out->action.id =
                    actions[press.activated_target - REACH_WIDGET_CONTROL_MEDIA_PREVIOUS];
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
            const bool placed =
                event->kind == REACH_POINTER_EVENT_UP && widgets->drag_has_destination;
            auto &card = widgets->cards[index];
            card = placed ? widgets->drag_destination : widgets->drag_origin;
            if (placed && !reach_rect_equal(card, widgets->drag_origin))
            {
                const reach_config_widget_placement placement = {
                    (int32_t)std::lround(card.x / scale), (int32_t)std::lround(card.y / scale),
                    (int32_t)std::lround(card.width / scale),
                    (int32_t)std::lround(card.height / scale)};
                widgets->placements[index] = placement;
                if (widgets->routes.placement_changed != nullptr)
                {
                    widgets->routes.placement_changed(widgets->routes.user, (size_t)index,
                                                      placement);
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
    reach_system_stats_set_monitor_enabled(
        widgets->stats, (widgets->enabled_mask & (1u << REACH_WIDGET_SYSTEM)) != 0);
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
    const bool monitor_changed =
        !reach_rect_equal(widgets->desktop_bounds, monitor) || widgets->dpi_scale != scale;
    widgets->desktop_bounds = monitor;
    widgets->dpi_scale = scale;
    widgets->layout_dirty |= monitor_changed;
    if (widgets->drag_index >= 0 || !widgets->layout_dirty)
    {
        return monitor_changed;
    }
    widgets->layout_dirty = false;
    reach_widget_layout_item items[REACH_MAX_DESKTOP_WIDGETS] = {};
    for (size_t index = 0; index < REACH_MAX_DESKTOP_WIDGETS; ++index)
    {
        const auto &placement = widgets->placements[index];
        const auto &definition = *reach_widget_definition_at(index);
        const auto &limits = definition.limits;
        auto &item = items[index];
        item.enabled = (widgets->enabled_mask & (1u << index)) != 0;
        item.custom = placement.width > 0 && placement.x >= 0 && placement.y >= 0;
        item.preferred = {placement.x * scale, placement.y * scale,
                          std::clamp(placement.width > 0 ? placement.width * scale
                                                         : definition.default_width * scale,
                                     limits.min_width * scale, limits.max_width * scale),
                          std::clamp(placement.height > 0 ? placement.height * scale
                                                          : definition.default_height * scale,
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
    const auto background = reach_theme_color_mix(theme->popup_background, accent, 0.12f);
    (void)reach_render_push_bordered_background(commands, &shape, background, theme->popup_border,
                                                border, &theme->popup_shadow, scale);
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
                       inner.y + inner.height * (wave == 0 ? 0.64f : 0.76f), span, span};
        ribbon.radius = span * 0.5f;
        ribbon.color = accent;
        ribbon.color.a = wave == 0 ? 0.09f : 0.08f;
        ribbon.clip_rect = inner;
        ribbon.clip_radius = radius;
        (void)reach_render_command_buffer_push(commands, &ribbon);
    }
}

static void reach_widgets_format_speed(wchar_t (&clock)[24], uint32_t speed, bool memory)
{
    if (speed == 0)
    {
        wcscpy_s(clock, L"—");
    }
    else if (memory)
    {
        swprintf_s(clock, L"%u MT/s", speed);
    }
    else if (speed >= 1000)
    {
        swprintf_s(clock, L"%.2f GHz", speed / 1000.0);
    }
    else
    {
        swprintf_s(clock, L"%u MHz", speed);
    }
}

static void reach_widgets_segments(reach_render_command_buffer *commands, const reach_theme *theme,
                                   reach_rect_f32 track, float value, float scale, bool available)
{
    const float gap = 3.0f * scale;
    const float pill_width = 8.0f * scale;
    const float height = std::min(10.0f * scale, track.height);
    const int32_t segments = (int32_t)((track.width + gap) / (pill_width + gap));
    if (segments == 0)
    {
        return;
    }

    const float meter_width = segments * (pill_width + gap) - gap;
    const float meter_x = track.x + track.width - meter_width;
    const float amount_filled =
        available ? std::clamp(value / 100.0f, 0.0f, 1.0f) * segments : 0.0f;
    auto empty = theme->accent_text;
    empty.a = 0.18f;
    auto muted = theme->accent_text;
    muted.a = 0.55f;
    for (int32_t segment = 0; segment < segments; ++segment)
    {
        const reach_rect_f32 pill = {meter_x + segment * (pill_width + gap),
                                     track.y + (track.height - height) * 0.5f, pill_width, height};
        const float position = segments > 1 ? (float)segment / (segments - 1) : 1.0f;
        const reach_color color = segment < amount_filled
                                      ? reach_theme_color_mix(muted, theme->accent_text, position)
                                      : empty;
        reach_widgets_rect(commands, pill, std::min(3.0f * scale, height * 0.5f), color);
    }
}

static void reach_widgets_meter(reach_render_command_buffer *commands, const reach_theme *theme,
                                reach_rect_f32 body, float y, const wchar_t *label, float value,
                                float scale, bool available, const wchar_t *detail,
                                float detail_width, bool show_labeled_percentage = true)
{
    wchar_t percentage[16] = {};
    if (available)
    {
        swprintf_s(percentage, L"%.0f%%", value);
    }
    else
    {
        wcscpy_s(percentage, L"—");
    }

    wchar_t labeled_percentage[24] = {};
    const wchar_t *left = label;
    const wchar_t *right = percentage;
    if (detail != nullptr)
    {
        if (show_labeled_percentage)
        {
            swprintf_s(labeled_percentage, L"%ls  %ls", label, percentage);
            left = labeled_percentage;
        }
        right = detail;
    }

    const float inset = 2.0f * scale;
    const float label_width = 76.0f * scale;
    const float column_gap = 8.0f * scale;
    const float track_x = body.x + inset + label_width + column_gap;
    const float detail_x = body.x + body.width - inset - detail_width;
    reach_widgets_text(commands, {body.x + inset, y, label_width, 22.0f * scale},
                       reinterpret_cast<const uint16_t *>(left), REACH_TEXT_SIZE_SMALL * scale,
                       REACH_TEXT_WEIGHT_SEMIBOLD, theme->primary_text);
    reach_widgets_text(commands, {detail_x, y, detail_width, 22.0f * scale},
                       reinterpret_cast<const uint16_t *>(right), REACH_TEXT_SIZE_SMALL * scale,
                       REACH_TEXT_WEIGHT_SEMIBOLD, theme->primary_text,
                       REACH_TEXT_ALIGNMENT_TRAILING);
    const float track_width = std::max(0.0f, detail_x - column_gap - track_x);
    const reach_rect_f32 track = {track_x, y, track_width, 22.0f * scale};
    if (!available && detail != nullptr && !show_labeled_percentage)
    {
        reach_widgets_text(commands, track, (const uint16_t *)u"—", REACH_TEXT_SIZE_SMALL * scale,
                           REACH_TEXT_WEIGHT_SEMIBOLD,
                           reach_theme_color_alpha(theme->primary_text, 0.55f),
                           REACH_TEXT_ALIGNMENT_CENTER);
        return;
    }
    reach_widgets_segments(commands, theme, track, value, scale, available);
}

static void reach_widgets_title(reach_render_command_buffer *commands, const reach_theme *theme,
                                reach_rect_f32 bounds, const uint16_t *label, float scale,
                                int32_t alignment = REACH_TEXT_ALIGNMENT_LEADING,
                                float size = REACH_TEXT_SIZE_SMALL)
{
    reach_widgets_text(commands, bounds, label, size * scale, REACH_TEXT_WEIGHT_SEMIBOLD,
                       theme->primary_text, alignment);
}

static void reach_widgets_section_header(reach_render_command_buffer *commands,
                                         const reach_theme *theme, reach_rect_f32 section,
                                         const uint16_t *section_label, const uint16_t *badge_label,
                                         float badge_alpha, float scale,
                                         const reach_text_measure_port *text_measure)
{
    const float text_size = REACH_TEXT_SIZE_SMALL * scale;
    const float badge_padding = 9.0f * scale;
    const float badge_width =
        std::ceil(reach_text_width_or_estimate(text_measure, badge_label, text_size,
                                               REACH_TEXT_WEIGHT_SEMIBOLD, 0.65f)) +
        2.0f * badge_padding;
    const float badge_height = 22.0f * scale;
    const reach_rect_f32 badge = {section.x + section.width - badge_width - 2.0f * scale,
                                  section.y + (section.height - badge_height) * 0.5f, badge_width,
                                  badge_height};
    reach_widgets_rect(commands, badge, badge.height * 0.5f,
                       reach_theme_color_alpha(theme->accent_text, badge_alpha));
    reach_widgets_text(commands, badge, badge_label, text_size, REACH_TEXT_WEIGHT_SEMIBOLD,
                       theme->primary_text, REACH_TEXT_ALIGNMENT_CENTER);
    const float label_width =
        std::ceil(reach_text_width_or_estimate(text_measure, section_label, text_size,
                                               REACH_TEXT_WEIGHT_SEMIBOLD, 0.65f)) +
        scale;
    reach_widgets_text(commands, {section.x, section.y, label_width, section.height}, section_label,
                       text_size, REACH_TEXT_WEIGHT_SEMIBOLD, theme->primary_text);
    const float divider_x = section.x + label_width + 8.0f * scale;
    const float divider_width = badge.x - divider_x - 8.0f * scale;
    if (divider_width > 0.0f)
    {
        reach_widgets_rect(commands,
                           {divider_x, section.y + section.height * 0.5f, divider_width, scale},
                           0.0f, reach_theme_color_alpha(theme->accent_text, 0.25f));
    }
}

static void reach_widgets_system_render(reach_render_command_buffer *commands,
                                        const reach_theme *theme, const reach_widget_chrome &chrome,
                                        float scale, const reach_system_stats_snapshot &stats,
                                        const reach_text_measure_port *text_measure)
{
    reach_widgets_title(commands, theme, chrome.header,
                        reach_widget_definition_at(REACH_WIDGET_SYSTEM)->label, scale);
    wchar_t speeds[3][24] = {};
    reach_widgets_format_speed(speeds[0], stats.cpu_clock_mhz, false);
    reach_widgets_format_speed(speeds[1], stats.memory_speed_mt_s, true);
    reach_widgets_format_speed(speeds[2], stats.gpu_clock_mhz, false);
    float clock_width = reach_text_width_or_estimate(text_measure, (const uint16_t *)u"100%",
                                                     REACH_TEXT_SIZE_SMALL * scale,
                                                     REACH_TEXT_WEIGHT_SEMIBOLD, 0.65f);
    for (const auto &speed : speeds)
    {
        clock_width = std::max(
            clock_width, reach_text_width_or_estimate(
                             text_measure, reinterpret_cast<const uint16_t *>(speed),
                             REACH_TEXT_SIZE_SMALL * scale, REACH_TEXT_WEIGHT_SEMIBOLD, 0.65f));
    }
    clock_width = std::ceil(clock_width) + scale;

    const float row_height = 22.0f * scale;
    const float before_section = 5.0f * scale;
    const float after_section = 4.0f * scale;
    const float row_gap = (chrome.body.height - row_height - before_section - after_section) / 5.0f;
    const float first_row = chrome.body.y;
    reach_widgets_meter(commands, theme, chrome.body, first_row, L"CPU", stats.cpu_percent, scale,
                        stats.valid != 0, speeds[0], clock_width);
    reach_widgets_meter(commands, theme, chrome.body, first_row + row_gap, L"RAM",
                        stats.memory_percent, scale, stats.valid != 0, speeds[1], clock_width);

    const reach_rect_f32 section = {chrome.body.x + 2.0f * scale,
                                    first_row + 2.0f * row_gap + before_section,
                                    chrome.body.width - 2.0f * scale, chrome.header.height};
    const bool has_temperature =
        stats.gpu_temperature_available && std::isfinite(stats.gpu_temperature_c);
    wchar_t temperature[24] = {};
    float thermal_tint = 0.08f;
    if (has_temperature)
    {
        swprintf_s(temperature, L"GPU temp  %.0f°C", stats.gpu_temperature_c);
        const float heat = std::clamp((stats.gpu_temperature_c - 40.0f) / 50.0f, 0.0f, 1.0f);
        thermal_tint = 0.14f + heat * 0.28f;
    }
    else
    {
        wcscpy_s(temperature, L"GPU temp  —");
    }

    reach_widgets_section_header(commands, theme, section, (const uint16_t *)u"GPU monitor",
                                 reinterpret_cast<const uint16_t *>(temperature), thermal_tint,
                                 scale, text_measure);

    const float gpu_row = section.y + row_gap + after_section;
    reach_widgets_meter(commands, theme, chrome.body, gpu_row, L"GPU", stats.gpu_percent, scale,
                        stats.gpu_available != 0, nullptr, clock_width);
    const float clock_row = gpu_row + row_gap;
    const bool clock_range_available = stats.gpu_clock_mhz != 0 && stats.gpu_clock_max_mhz != 0;
    const float clock_percent =
        clock_range_available ? 100.0f * (float)stats.gpu_clock_mhz / (float)stats.gpu_clock_max_mhz
                              : 0.0f;
    reach_widgets_meter(commands, theme, chrome.body, clock_row, L"Clock speed", clock_percent,
                        scale, clock_range_available, speeds[2], clock_width, false);
    reach_widgets_meter(commands, theme, chrome.body, clock_row + row_gap, L"VRAM",
                        stats.vram_percent, scale, stats.vram_available != 0, nullptr, clock_width);
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
                                      const reach_theme *theme, reach_rect_f32 grip, float scale)
{
    reach_widgets_rect(commands,
                       {grip.x + 4.0f * scale, grip.y + 10.0f * scale, 11.0f * scale, 2.0f * scale},
                       scale, reach_theme_color_alpha(theme->primary_text, 0.28f));
}

static void reach_widgets_footer_button(reach_render_command_buffer *commands,
                                        const reach_theme *theme, reach_rect_f32 bounds,
                                        const wchar_t *label, float scale)
{
    reach_widgets_rect(commands, bounds, 8.0f * scale, theme->accent_tint);
    reach_widgets_title(commands, theme, bounds, reinterpret_cast<const uint16_t *>(label), scale,
                        REACH_TEXT_ALIGNMENT_CENTER);
}

static void reach_widgets_pomodoro_render(const reach_widgets *widgets, const reach_theme *theme,
                                          const reach_widget_chrome &chrome, float scale,
                                          reach_render_command_buffer *commands)
{
    wchar_t buffer[260] = {};
    const wchar_t *phase = widgets->pomodoro.phase == 0   ? L"Focus"
                           : widgets->pomodoro.phase == 1 ? L"Short break"
                                                          : L"Long break";
    const auto *label = reach_widget_definition_at(REACH_WIDGET_POMODORO)->label;
    swprintf_s(buffer, L"%ls · %ls", reinterpret_cast<const wchar_t *>(label), phase);
    reach_widgets_title(commands, theme, chrome.header, reinterpret_cast<const uint16_t *>(buffer),
                        scale);
    uint64_t seconds = (uint64_t)std::ceil(
        reach_pomodoro_remaining(widgets->pomodoro, reach_motion_now_seconds()));
    swprintf_s(buffer, L"%02llu:%02llu", seconds / 60, seconds % 60);
    reach_widgets_text(
        commands, {chrome.body.x, chrome.body.y + 5.0f * scale, chrome.body.width, 43.0f * scale},
        reinterpret_cast<const uint16_t *>(buffer), REACH_TEXT_SIZE_XLARGE * scale,
        REACH_TEXT_WEIGHT_DEMIBOLD, theme->primary_text, REACH_TEXT_ALIGNMENT_CENTER);
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
    reach_widgets_text(
        commands, {chrome.body.x, chrome.body.y + 50.0f * scale, chrome.body.width, 18.0f * scale},
        reinterpret_cast<const uint16_t *>(buffer), REACH_TEXT_SIZE_XSMALL * scale,
        REACH_TEXT_WEIGHT_NORMAL, theme->primary_text, REACH_TEXT_ALIGNMENT_CENTER);
    float duration = widgets->pomodoro.phase == 0   ? 1500.0f
                     : widgets->pomodoro.phase == 1 ? 300.0f
                                                    : 900.0f;
    const reach_rect_f32 track = {chrome.footer.x, chrome.footer.y - 9.0f * scale,
                                  chrome.footer.width, 4.0f * scale};
    const float progress =
        100.0f *
        (1.0f -
         (float)reach_pomodoro_remaining(widgets->pomodoro, reach_motion_now_seconds()) / duration);
    reach_widgets_segments(commands, theme, track, progress, scale, true);
    const wchar_t *const labels[] = {widgets->pomodoro.running ? L"Pause" : L"Start", L"Skip",
                                     L"Reset"};
    for (int button = 0; button < reach_widgets_controls[REACH_WIDGET_POMODORO].count; ++button)
    {
        const auto bounds =
            reach_widgets_control_bounds(chrome, REACH_WIDGET_POMODORO, button, scale);
        reach_widgets_footer_button(commands, theme, bounds, labels[button], scale);
    }
}

static void reach_widgets_render_body(const reach_widgets *widgets, size_t index,
                                      const reach_widget_chrome &chrome, const reach_theme *actual,
                                      float scale, const reach_text_measure_port *text_measure,
                                      reach_render_command_buffer *commands)
{
    if (index == REACH_WIDGET_POMODORO)
    {
        reach_widgets_pomodoro_render(widgets, actual, chrome, scale, commands);
        return;
    }
    const auto &title = chrome.header;
    const auto &body = chrome.body;
    reach_rect_f32 value = {body.x, body.y + 4.0f * scale, body.width, 42.0f * scale};
    wchar_t buffer[128] = {};
    if (index == REACH_WIDGET_CLOCK)
    {
        reach_widgets_title(commands, actual, title, reach_widget_definition_at(index)->label,
                            scale);
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
        reach_widgets_system_render(commands, actual, chrome, scale, widgets->stats_snapshot,
                                    text_measure);
        return;
    }
    else if (index == REACH_WIDGET_STOPWATCH)
    {
        reach_widgets_title(commands, actual, title, reach_widget_definition_at(index)->label,
                            scale);
        double seconds =
            widgets->stopwatch_elapsed +
            (widgets->stopwatch_running ? reach_motion_now_seconds() - widgets->stopwatch_started
                                        : 0.0);
        uint64_t tenths = (uint64_t)(std::max(0.0, seconds) * 10.0);
        swprintf_s(buffer, L"%02llu:%02llu.%llu", tenths / 600, tenths / 10 % 60, tenths % 10);
        reach_widgets_text(commands, value, reinterpret_cast<const uint16_t *>(buffer),
                           REACH_TEXT_SIZE_XLARGE * scale, REACH_TEXT_WEIGHT_DEMIBOLD,
                           actual->primary_text, REACH_TEXT_ALIGNMENT_CENTER);
        for (int32_t button = 0; button < reach_widgets_controls[index].count; ++button)
        {
            const auto bounds = reach_widgets_control_bounds(chrome, (int32_t)index, button, scale);
            const wchar_t *label =
                button == 1 ? L"Reset" : (widgets->stopwatch_running ? L"Pause" : L"Start");
            reach_widgets_footer_button(commands, actual, bounds, label, scale);
        }
        return;
    }
    else if (index == REACH_WIDGET_CALENDAR)
    {
        static const wchar_t *months[12] = {L"January",   L"February", L"March",    L"April",
                                            L"May",       L"June",     L"July",     L"August",
                                            L"September", L"October",  L"November", L"December"};
        if (!widgets->clock_snapshot.valid)
        {
            reach_widgets_title(commands, actual, title, reach_widget_definition_at(index)->label,
                                scale);
            reach_widgets_text(commands, body, (const uint16_t *)u"Date unavailable",
                               REACH_TEXT_SIZE_MEDIUM * scale, REACH_TEXT_WEIGHT_NORMAL,
                               actual->primary_text, REACH_TEXT_ALIGNMENT_CENTER);
            return;
        }
        int32_t year = widgets->clock_snapshot.year;
        int32_t month = widgets->clock_snapshot.month;
        int32_t shifted = (year * 12 + month - 1) + widgets->calendar_month_offset;
        year = shifted / 12;
        month = shifted % 12 + 1;
        swprintf_s(buffer, L"%ls %d", months[month - 1], year);
        auto month_title = title;
        month_title.x += 20.0f * scale;
        month_title.width -= 40.0f * scale;
        reach_widgets_title(commands, actual, month_title,
                            reinterpret_cast<const uint16_t *>(buffer), scale,
                            REACH_TEXT_ALIGNMENT_CENTER, REACH_TEXT_SIZE_MEDIUM);
        reach_widgets_text(commands, reach_widgets_control_bounds(chrome, (int32_t)index, 0, scale),
                           reinterpret_cast<const uint16_t *>(L"‹"), REACH_TEXT_SIZE_MEDIUM * scale,
                           REACH_TEXT_WEIGHT_SEMIBOLD, actual->primary_text,
                           REACH_TEXT_ALIGNMENT_CENTER);
        reach_widgets_text(commands, reach_widgets_control_bounds(chrome, (int32_t)index, 1, scale),
                           reinterpret_cast<const uint16_t *>(L"›"), REACH_TEXT_SIZE_MEDIUM * scale,
                           REACH_TEXT_WEIGHT_SEMIBOLD, actual->primary_text,
                           REACH_TEXT_ALIGNMENT_CENTER);
        const wchar_t *weekdays[7] = {L"S", L"M", L"T", L"W", L"T", L"F", L"S"};
        float cell_width = body.width / 7.0f;
        float cell_height = (body.height - 29.0f * scale) / 6.0f;
        int32_t first_weekday = reach_widgets_month_weekday(year, month);
        for (int32_t column = 0; column < 7; ++column)
        {
            reach_rect_f32 cell = {body.x + column * cell_width, body.y + 3.0f * scale, cell_width,
                                   22.0f * scale};
            reach_widgets_text(commands, cell, reinterpret_cast<const uint16_t *>(weekdays[column]),
                               REACH_TEXT_SIZE_XSMALL * scale, REACH_TEXT_WEIGHT_SEMIBOLD,
                               actual->primary_text);
        }
        int32_t days = reach_widgets_days_in_month(year, month);
        for (int32_t day = 1; day <= days; ++day)
        {
            int32_t slot = first_weekday + day - 1;
            int32_t row = slot / 7;
            int32_t column = slot % 7;
            reach_rect_f32 cell = {body.x + column * cell_width,
                                   body.y + 29.0f * scale + row * cell_height,
                                   cell_width - 3.0f * scale, cell_height - 3.0f * scale};
            const bool today =
                widgets->calendar_month_offset == 0 && day == widgets->clock_snapshot.day;
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
        return;
    }
    else
    {
        const bool has_cover = widgets->media_snapshot.cover_image_id != 0;
        if (has_cover)
        {
            reach_render_command cover = {};
            cover.type = REACH_RENDER_COMMAND_ICON;
            cover.rect = {body.x, body.y + 4.0f * scale, 62.0f * scale, 62.0f * scale};
            cover.icon_id = widgets->media_snapshot.cover_image_id;
            cover.icon_crop_to_fill = 1;
            cover.radius = 12.0f * scale;
            cover.corner_mask = REACH_RENDER_CORNER_ALL;
            cover.color.a = 1.0f;
            (void)reach_render_command_buffer_push(commands, &cover);
        }
        reach_widgets_title(commands, actual, title, reach_widget_definition_at(index)->label,
                            scale);
        if (widgets->media_snapshot.has_session)
        {
            const float cover_space = has_cover ? 74.0f * scale : 0.0f;
            reach_rect_f32 track = {body.x + cover_space, body.y + 7.0f * scale,
                                    body.width - cover_space, 27.0f * scale};
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
            for (int32_t control = 0; control < reach_widgets_controls[index].count; ++control)
            {
                const auto button =
                    reach_widgets_control_bounds(chrome, (int32_t)index, control, scale);
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
            return;
        }
        wcscpy_s(buffer, L"Nothing playing");
    }
    reach_widgets_text(
        commands, value, reinterpret_cast<const uint16_t *>(buffer),
        (index == REACH_WIDGET_CLOCK ? REACH_TEXT_SIZE_XLARGE : REACH_TEXT_SIZE_LARGE) * scale,
        REACH_TEXT_WEIGHT_DEMIBOLD, actual->primary_text);
    if (index == REACH_WIDGET_CLOCK && widgets->clock_snapshot.valid)
    {
        swprintf_s(buffer, L"%04d / %02d / %02d", widgets->clock_snapshot.year,
                   widgets->clock_snapshot.month, widgets->clock_snapshot.day);
        reach_widgets_text(commands, {body.x, body.y + 55.0f * scale, body.width, 24.0f * scale},
                           reinterpret_cast<const uint16_t *>(buffer),
                           REACH_TEXT_SIZE_SMALL * scale, REACH_TEXT_WEIGHT_NORMAL,
                           actual->primary_text);
    }
}

reach_result reach_widgets_render_desktop(const reach_widgets *widgets, const reach_theme *theme,
                                          float dpi_scale,
                                          const reach_text_measure_port *text_measure,
                                          reach_render_command_buffer *commands)
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
            const auto target =
                widgets->drag_has_destination ? widgets->drag_destination : widgets->drag_origin;
            if (!reach_rect_equal(rect, target))
            {
                auto marker = actual->accent;
                marker.a = 0.28f;
                reach_widgets_rect(commands, target, actual->radius_large * scale, marker);
            }
        }
        reach_widgets_card(commands, actual, rect, scale, actual->accent);
        const auto inner = reach_theme_border_content_rect(actual, scale, rect);
        const float inner_radius = std::max(0.0f, actual->radius_large * scale -
                                                      reach_theme_border_thickness(actual, scale));
        const bool has_artwork =
            index == REACH_WIDGET_NOW_PLAYING && widgets->media_snapshot.cover_image_id != 0;
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
        }
        reach_widgets_decorate(commands, inner, inner_radius, actual->accent);
        const auto chrome =
            reach_widgets_chrome(rect, scale, reach_widgets_has_footer((int32_t)index));
        reach_widgets_render_body(widgets, index, chrome, actual, scale, text_measure, commands);
        reach_widgets_resize_grip(commands, actual, chrome.resize, scale);
    }
    return REACH_OK;
}
