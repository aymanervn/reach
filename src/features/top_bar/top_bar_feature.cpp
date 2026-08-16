#include "reach/features/top_bar.h"

#include "top_bar_common.h"
#include "top_bar_metrics.h"
#include "top_bar_now_playing.h"

#include <new>
#include <stdio.h>

const reach_top_bar_state *reach_top_bar_state_ptr(const reach_top_bar *top_bar)
{
    return top_bar != nullptr ? &top_bar->state : nullptr;
}

reach_top_bar_state *reach_top_bar_state_mut(reach_top_bar *top_bar)
{
    return top_bar != nullptr ? &top_bar->state : nullptr;
}

reach_animation_manager *reach_top_bar_manager(reach_top_bar *top_bar)
{
    return top_bar != nullptr ? &top_bar->manager : nullptr;
}

reach_result reach_top_bar_create(reach_top_bar **out_top_bar)
{
    if (out_top_bar == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_top_bar *top_bar = new (std::nothrow) reach_top_bar();
    if (top_bar == nullptr)
    {
        return REACH_ERROR;
    }

    if (reach_top_bar_now_playing_create(&top_bar->now_playing_subfeature) != REACH_OK)
    {
        delete top_bar;
        return REACH_ERROR;
    }

    reach_animation_manager_init(&top_bar->manager, top_bar->tracks, REACH_TOP_BAR_ANIM_COUNT);
    top_bar->state.feedback_index = REACH_TOP_BAR_FEEDBACK_NONE;
    top_bar->state.pressed_control = REACH_TOP_BAR_POINTER_REGION_NONE;
    *out_top_bar = top_bar;
    return REACH_OK;
}

void reach_top_bar_destroy(reach_top_bar *top_bar)
{
    if (top_bar != nullptr)
    {
        reach_top_bar_now_playing_destroy(top_bar->now_playing_subfeature);
    }
    delete top_bar;
}

void reach_top_bar_attach_services(reach_top_bar *top_bar, reach_now_playing_service *now_playing,
                                   reach_icon_service *icons, reach_window_tracking *windows,
                                   reach_system_stats *stats, reach_clock *clock,
                                   reach_input_language_service *input_language)
{
    if (top_bar != nullptr)
    {
        top_bar->now_playing = now_playing;
        top_bar->icons = icons;
        top_bar->windows = windows;
        top_bar->stats = stats;
        top_bar->clock = clock;
        top_bar->input_language = input_language;
    }
}

void reach_top_bar_attach_status(reach_top_bar *top_bar, reach_system_status *status)
{
    if (top_bar != nullptr)
    {
        top_bar->status = status;
    }
}

static void reach_top_bar_copy_ascii_to_utf16(uint16_t *dst, size_t dst_count, const char *src);
static int32_t reach_top_bar_utf16_equal(const uint16_t *a, const uint16_t *b);
static int32_t reach_top_bar_update_clock(reach_top_bar *top_bar);
static void reach_top_bar_update_language(reach_top_bar *top_bar);

static void reach_top_bar_format_percent(uint16_t *dst, size_t dst_count, const char *label,
                                         float percent)
{
    char text[16] = {};
    int32_t value = (int32_t)(percent + 0.5f);
    if (value < 0)
    {
        value = 0;
    }
    if (value > 100)
    {
        value = 100;
    }
    snprintf(text, sizeof(text), "%s %d%%", label, value);
    reach_top_bar_copy_ascii_to_utf16(dst, dst_count, text);
}

static void reach_top_bar_format_rate(uint16_t *dst, size_t dst_count, uint16_t prefix,
                                      uint64_t bytes_per_second)
{
    static const char *units[] = {"B", "KB", "MB", "GB"};
    const size_t unit_count = sizeof(units) / sizeof(units[0]);

    uint64_t value = bytes_per_second;
    size_t unit = 0;
    while (value >= 1000ull && unit + 1 < unit_count)
    {
        value = (value + 512ull) / 1024ull;
        ++unit;
    }
    if (value > 999ull)
    {
        value = 999ull;
    }

    char text[16] = {};
    snprintf(text, sizeof(text), " %llu%s", (unsigned long long)value, units[unit]);

    if (dst == nullptr || dst_count < 2)
    {
        return;
    }
    dst[0] = prefix;
    reach_top_bar_copy_ascii_to_utf16(dst + 1, dst_count - 1, text);
}

static void reach_top_bar_update_stats(reach_top_bar *top_bar)
{
    reach_top_bar_state *state = &top_bar->state;
    reach_system_stats_snapshot snapshot = {};
    reach_system_stats_snapshot_take(top_bar->stats, &snapshot);

    state->battery_valid =
        snapshot.power_valid && snapshot.power.has_battery && snapshot.power.battery_percent >= 0;
    state->battery_percent = state->battery_valid ? snapshot.power.battery_percent : 0;

    state->stats_valid = snapshot.valid;
    if (!snapshot.valid)
    {
        state->stats_cpu_text[0] = 0;
        state->stats_memory_text[0] = 0;
        state->stats_download_text[0] = 0;
        state->stats_upload_text[0] = 0;
        return;
    }

    reach_top_bar_format_percent(state->stats_cpu_text, 16, "CPU", snapshot.cpu_percent);
    reach_top_bar_format_percent(state->stats_memory_text, 16, "RAM", snapshot.memory_percent);
    reach_top_bar_format_rate(state->stats_download_text, 16, 0x2193,
                              snapshot.network_received_bytes_per_second);
    reach_top_bar_format_rate(state->stats_upload_text, 16, 0x2191,
                              snapshot.network_sent_bytes_per_second);
}

static uint32_t reach_top_bar_network_icon_id(const reach_network_state *network, int32_t valid)
{
    if (!valid || !network->connected)
    {
        return REACH_VECTOR_ICON_NO_INTERNET;
    }
    if (network->kind == REACH_NETWORK_KIND_ETHERNET)
    {
        return REACH_VECTOR_ICON_ETHERNET;
    }
    if (network->signal_strength < 34)
    {
        return REACH_VECTOR_ICON_WIFI_LOW;
    }
    if (network->signal_strength < 67)
    {
        return REACH_VECTOR_ICON_WIFI_MEDIUM;
    }
    return REACH_VECTOR_ICON_WIFI_HIGH;
}

static uint32_t reach_top_bar_bluetooth_icon_id(const reach_bluetooth_state *bluetooth,
                                                int32_t valid)
{
    if (!valid || !bluetooth->available)
    {
        return REACH_VECTOR_ICON_NONE;
    }
    return bluetooth->enabled ? REACH_VECTOR_ICON_BLUETOOTH_ON : REACH_VECTOR_ICON_BLUETOOTH_OFF;
}

static int32_t reach_top_bar_update_system_status(reach_top_bar *top_bar)
{
    reach_top_bar_state *state = &top_bar->state;
    reach_system_status_system_snapshot snapshot = {};
    reach_system_status_read_system(top_bar->status, &snapshot);

    uint32_t network_icon =
        reach_top_bar_network_icon_id(&snapshot.network, snapshot.network_valid);
    uint32_t bluetooth_icon =
        reach_top_bar_bluetooth_icon_id(&snapshot.bluetooth, snapshot.bluetooth_valid);

    uint16_t name[REACH_SYSTEM_NETWORK_LABEL_CAPACITY] = {};
    if (snapshot.network_valid && snapshot.network.connected &&
        snapshot.network.kind == REACH_NETWORK_KIND_WIFI)
    {
        reach_copy_utf16(name, REACH_SYSTEM_NETWORK_LABEL_CAPACITY, snapshot.network.label);
    }

    if (state->network_icon_id == network_icon && state->bluetooth_icon_id == bluetooth_icon &&
        reach_top_bar_utf16_equal(state->network_name, name))
    {
        return 0;
    }

    state->network_icon_id = network_icon;
    state->bluetooth_icon_id = bluetooth_icon;
    reach_copy_utf16(state->network_name, REACH_SYSTEM_NETWORK_LABEL_CAPACITY, name);
    return 1;
}

reach_icon_service *reach_top_bar_icons(reach_top_bar *top_bar)
{
    return top_bar != nullptr ? top_bar->icons : nullptr;
}

static const uint16_t *reach_top_bar_path_stem(const uint16_t *path)
{
    if (path == nullptr)
    {
        return nullptr;
    }
    const uint16_t *stem = path;
    for (const uint16_t *cursor = path; *cursor != 0; ++cursor)
    {
        if (*cursor == '\\' || *cursor == '/')
        {
            stem = cursor + 1;
        }
    }
    return stem;
}

static void reach_top_bar_copy_stem_without_extension(uint16_t *dst, size_t dst_count,
                                                      const uint16_t *path)
{
    const uint16_t *stem = reach_top_bar_path_stem(path);
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }
    size_t length = 0;
    size_t last_dot = 0;
    while (stem != nullptr && stem[length] != 0 && length + 1 < dst_count)
    {
        if (stem[length] == '.')
        {
            last_dot = length;
        }
        dst[length] = stem[length];
        ++length;
    }
    if (last_dot > 0)
    {
        length = last_dot;
    }
    dst[length] = 0;
}

static void reach_top_bar_update_current_app(reach_top_bar *top_bar,
                                             const reach_top_bar_build_context *ctx)
{
    reach_top_bar_state *state = &top_bar->state;
    state->current_app_name[0] = 0;
    state->current_app_icon_ref[0] = 0;

    uintptr_t foreground =
        top_bar->windows != nullptr ? reach_window_tracking_foreground(top_bar->windows) : 0;
    const reach_window_snapshot *window =
        foreground != 0 ? reach_window_tracking_window_by_id(top_bar->windows, foreground)
                        : nullptr;
    if (window == nullptr)
    {
        reach_copy_utf16(state->current_app_name, 260, (const uint16_t *)L"Desktop");
        return;
    }

    reach_copy_utf16(state->current_app_icon_ref, 260,
                     window->icon_ref[0] != 0 ? window->icon_ref : window->path);

    for (size_t index = 0; index < ctx->pinned_app_count; ++index)
    {
        const reach_pinned_app_model *app = &ctx->pinned_apps[index];
        if (reach_window_tracking_window_matches_app(app, window) && app->title[0] != 0)
        {
            reach_copy_utf16(state->current_app_name, 260, app->title);
            return;
        }
    }

    reach_top_bar_copy_stem_without_extension(state->current_app_name, 260, window->path);
    if (state->current_app_name[0] == 0)
    {
        reach_copy_utf16(state->current_app_name, 260, window->title);
    }
}

reach_top_bar_now_playing *reach_top_bar_now_playing_subfeature(reach_top_bar *top_bar)
{
    return top_bar != nullptr ? top_bar->now_playing_subfeature : nullptr;
}

static void reach_top_bar_update_tray_items(reach_top_bar *top_bar,
                                            const reach_top_bar_build_context *ctx)
{
    reach_top_bar_state *state = &top_bar->state;
    size_t count = ctx->tray_item_count;
    state->tray_overflow = count > REACH_TOP_BAR_MAX_TRAY_ICONS;
    if (count > REACH_TOP_BAR_MAX_TRAY_ICONS)
    {
        count = REACH_TOP_BAR_MAX_TRAY_ICONS;
    }
    state->tray_item_count = count;
    state->tray_popup_open = ctx->tray_popup_open;
    for (size_t index = 0; index < count; ++index)
    {
        state->tray_items[index] = ctx->tray_items[index];
    }
}

size_t reach_top_bar_tray_overflow_start(const reach_top_bar *top_bar)
{
    return top_bar != nullptr && top_bar->state.tray_overflow ? top_bar->state.tray_item_count : 0;
}

static int32_t reach_top_bar_has_current_app_icon(const reach_top_bar *top_bar)
{
    return top_bar->state.current_app_icon_ref[0] != 0;
}

static float reach_top_bar_height(float dpi_scale)
{
    return reach_top_bar_metrics_values.height * (dpi_scale > 0.0f ? dpi_scale : 1.0f);
}

static float reach_top_bar_stats_slot_advance(const uint16_t *text, const uint16_t *widest,
                                              float text_size)
{
    float advance = reach_top_bar_text_advance(text, text_size);
    float minimum = reach_top_bar_text_advance(widest, text_size);
    return advance > minimum ? advance : minimum;
}

static float reach_top_bar_resolve_animated_width(reach_top_bar *top_bar, size_t track,
                                                  float *target_store, float target)
{
    if (*target_store != target)
    {
        *target_store = target;
        reach_animation_manager_animate_to(&top_bar->manager, track, target,
                                           reach_top_bar_metrics_values.width_animation_seconds,
                                           REACH_EASING_EASE_IN_OUT);
    }

    float width = reach_animation_manager_value(&top_bar->manager, track);
    if (width <= 0.0f)
    {
        width = target;
        reach_animation_manager_set(&top_bar->manager, track, target);
    }
    return width;
}

void reach_top_bar_build_layout(reach_top_bar *top_bar, const reach_top_bar_build_context *ctx)
{
    if (top_bar == nullptr || ctx == nullptr || ctx->theme == nullptr)
    {
        return;
    }

    const reach_top_bar_metrics &metrics = reach_top_bar_metrics_values;
    const float scale = ctx->dpi_scale > 0.0f ? ctx->dpi_scale : 1.0f;
    const float height = reach_top_bar_height(scale);
    reach_top_bar_layout *layout = &top_bar->state.layout;

    *layout = {};
    layout->bounds.x = ctx->monitor_bounds.x;
    layout->bounds.width = ctx->monitor_bounds.width;
    layout->bounds.y = ctx->monitor_bounds.y + metrics.screen_gap * scale;
    layout->bounds.height = height;

    if (height <= 0.0f || layout->bounds.width <= 0.0f)
    {
        return;
    }

    const float edge_inset = metrics.edge_inset * scale;
    const float pill_gap = metrics.pill_gap * scale;
    const float padding = metrics.pill_padding * scale;
    const float power_button_size = height * metrics.power_button_scale;
    const float clock_gap = metrics.clock_gap * scale;
    const float time_size = metrics.clock_time_text_size * scale;
    const float date_size = metrics.clock_date_text_size * scale;

    float time_advance = reach_top_bar_text_advance(top_bar->state.clock_time_text, time_size);
    float date_advance = reach_top_bar_text_advance(top_bar->state.clock_date_text, date_size);
    float clock_width = time_advance + clock_gap + date_advance;

    const float dot_size = ctx->theme->bar_separator_dot_size * scale;
    const float dot_gap = ctx->theme->bar_separator_dot_gap * scale;

    float now_playing_width = reach_top_bar_resolve_animated_width(
        top_bar, REACH_TOP_BAR_ANIM_NOW_PLAYING_WIDTH, &top_bar->now_playing_target_width,
        reach_top_bar_now_playing_desired_width(top_bar->now_playing_subfeature, ctx->theme,
                                                scale));

    float power_clock_width = padding + power_button_size + clock_gap + clock_width + dot_gap +
                              dot_size + dot_gap + now_playing_width;
    float left = edge_inset;
    layout->pills[REACH_TOP_BAR_PILL_POWER_CLOCK] =
        reach_top_bar_rect(left, 0.0f, power_clock_width, height);
    layout->power_button =
        reach_top_bar_rect(left + padding, (height - power_button_size) * 0.5f, power_button_size,
                           power_button_size);

    float clock_x = layout->power_button.x + power_button_size + clock_gap;
    layout->clock_time = reach_top_bar_text_run(clock_x, height, time_advance, time_size);
    layout->clock_date = reach_top_bar_text_run(clock_x + time_advance + clock_gap, height,
                                                date_advance, date_size);

    layout->now_playing =
        reach_top_bar_rect(left + power_clock_width - now_playing_width, 0.0f, now_playing_width,
                           height);
    layout->now_playing_separator =
        reach_top_bar_rect(layout->now_playing.x - dot_gap - dot_size, (height - dot_size) * 0.5f,
                           dot_size, dot_size);

    reach_top_bar_update_language(top_bar);
    reach_top_bar_update_stats(top_bar);

    float right = layout->bounds.width - edge_inset;
    float button_size = height * metrics.bar_button_scale;
    float language_width =
        top_bar->state.language_code[0] != 0 ? metrics.language_width * scale : 0.0f;
    float language_gap = language_width > 0.0f ? pill_gap : 0.0f;

    const float stats_size = metrics.stats_text_size * scale;
    const float stats_gap = metrics.stats_gap * scale;
    const float stats_group_gap = metrics.stats_group_gap * scale;
    float cpu_advance = 0.0f;
    float memory_advance = 0.0f;
    float download_advance = 0.0f;
    float upload_advance = 0.0f;
    float stats_width = 0.0f;
    if (top_bar->state.stats_valid)
    {
        cpu_advance = reach_top_bar_stats_slot_advance(top_bar->state.stats_cpu_text,
                                                       (const uint16_t *)L"CPU 100%", stats_size);
        memory_advance = reach_top_bar_stats_slot_advance(
            top_bar->state.stats_memory_text, (const uint16_t *)L"RAM 100%", stats_size);
        download_advance = reach_top_bar_stats_slot_advance(
            top_bar->state.stats_download_text, (const uint16_t *)L"\u2193 999KB", stats_size);
        upload_advance = reach_top_bar_stats_slot_advance(
            top_bar->state.stats_upload_text, (const uint16_t *)L"\u2191 999KB", stats_size);
        stats_width = cpu_advance + stats_gap + memory_advance + stats_group_gap +
                      download_advance + stats_gap + upload_advance + stats_group_gap;
    }

    const float glyph_size = button_size * metrics.bar_button_glyph_scale;
    const float quick_settings_padding = metrics.quick_settings_padding * scale;
    const float quick_settings_content_gap = metrics.quick_settings_content_gap * scale;
    const float network_name_size = metrics.network_name_text_size * scale;
    float network_name_advance =
        reach_top_bar_text_advance(top_bar->state.network_name, network_name_size);
    if (network_name_advance > metrics.network_name_max_width * scale)
    {
        network_name_advance = metrics.network_name_max_width * scale;
    }

    float quick_settings_content = glyph_size;
    if (network_name_advance > 0.0f)
    {
        quick_settings_content += quick_settings_content_gap + network_name_advance;
    }
    if (top_bar->state.bluetooth_icon_id != REACH_VECTOR_ICON_NONE)
    {
        quick_settings_content += quick_settings_content_gap + glyph_size;
    }
    float quick_settings_button_width = reach_top_bar_resolve_animated_width(
        top_bar, REACH_TOP_BAR_ANIM_QUICK_SETTINGS_WIDTH, &top_bar->quick_settings_target_width,
        quick_settings_padding * 2.0f + quick_settings_content);

    float quick_settings_width = dot_size * 0.5f + dot_gap + stats_width + language_width +
                                 language_gap + quick_settings_button_width + pill_gap +
                                 button_size + padding;
    layout->pills[REACH_TOP_BAR_PILL_QUICK_SETTINGS] =
        reach_top_bar_rect(right - quick_settings_width, 0.0f, quick_settings_width, height);
    layout->tray_separator = reach_top_bar_rect(
        layout->pills[REACH_TOP_BAR_PILL_QUICK_SETTINGS].x - dot_size * 0.5f,
        (height - dot_size) * 0.5f, dot_size, dot_size);

    float cluster_x =
        layout->pills[REACH_TOP_BAR_PILL_QUICK_SETTINGS].x + dot_size * 0.5f + dot_gap;
    if (top_bar->state.stats_valid)
    {
        layout->stats_cpu = reach_top_bar_text_run(cluster_x, height, cpu_advance, stats_size);
        cluster_x += cpu_advance + stats_gap;
        layout->stats_memory = reach_top_bar_text_run(cluster_x, height, memory_advance, stats_size);
        cluster_x += memory_advance + stats_group_gap;
        layout->stats_download =
            reach_top_bar_text_run(cluster_x, height, download_advance, stats_size);
        cluster_x += download_advance + stats_gap;
        layout->stats_upload = reach_top_bar_text_run(cluster_x, height, upload_advance, stats_size);
        cluster_x += upload_advance + stats_group_gap;
    }
    if (language_width > 0.0f)
    {
        layout->language_button = reach_top_bar_rect(
            cluster_x, (height - button_size) * 0.5f, language_width, button_size);
        cluster_x += language_width + language_gap;
    }
    layout->quick_settings_button = reach_top_bar_rect(
        cluster_x, (height - button_size) * 0.5f, quick_settings_button_width, button_size);

    float glyph_y = (height - glyph_size) * 0.5f;
    float content_x = cluster_x + quick_settings_padding;
    layout->network_icon = reach_top_bar_rect(content_x, glyph_y, glyph_size, glyph_size);
    content_x += glyph_size;
    if (network_name_advance > 0.0f)
    {
        content_x += quick_settings_content_gap;
        layout->network_label =
            reach_top_bar_text_run(content_x, height, network_name_advance, network_name_size);
        content_x += network_name_advance;
    }
    if (top_bar->state.bluetooth_icon_id != REACH_VECTOR_ICON_NONE)
    {
        content_x += quick_settings_content_gap;
        layout->bluetooth_icon = reach_top_bar_rect(content_x, glyph_y, glyph_size, glyph_size);
    }

    cluster_x += quick_settings_button_width + pill_gap;
    layout->settings_button =
        reach_top_bar_rect(cluster_x, (height - button_size) * 0.5f, button_size, button_size);
    right = layout->pills[REACH_TOP_BAR_PILL_QUICK_SETTINGS].x;

    reach_top_bar_update_tray_items(top_bar, ctx);
    const float tray_slot = height * metrics.tray_icon_scale;
    const float tray_gap = metrics.tray_icon_gap * scale;
    const size_t tray_count = top_bar->state.tray_item_count;
    const size_t tray_cells = tray_count + (top_bar->state.tray_overflow ? 1u : 0u);
    float tray_target_width = padding + dot_gap + dot_size * 0.5f;
    if (tray_cells > 0)
    {
        tray_target_width += (float)tray_cells * tray_slot + (float)(tray_cells - 1) * tray_gap;
    }
    float tray_width = reach_top_bar_resolve_animated_width(
        top_bar, REACH_TOP_BAR_ANIM_TRAY_WIDTH, &top_bar->tray_target_width, tray_target_width);
    layout->pills[REACH_TOP_BAR_PILL_TRAY] =
        reach_top_bar_rect(right - tray_width, 0.0f, tray_width, height);

    float tray_x = layout->pills[REACH_TOP_BAR_PILL_TRAY].x + padding;
    float tray_y = (height - tray_slot) * 0.5f;
    layout->tray_icon_count = tray_count;
    for (size_t index = 0; index < tray_count; ++index)
    {
        layout->tray_icons[index] = reach_top_bar_rect(tray_x, tray_y, tray_slot, tray_slot);
        tray_x += tray_slot + tray_gap;
    }
    layout->tray_overflow_button =
        top_bar->state.tray_overflow ? reach_top_bar_rect(tray_x, tray_y, tray_slot, tray_slot)
                                     : reach_rect_f32{};

    reach_top_bar_update_current_app(top_bar, ctx);

    const float current_app_gap = metrics.current_app_gap * scale;
    const float name_size = metrics.current_app_name_text_size * scale;
    float current_app_icon_size =
        reach_top_bar_has_current_app_icon(top_bar) ? height * metrics.current_app_icon_scale : 0.0f;
    float current_app_icon_gap = current_app_icon_size > 0.0f ? current_app_gap : 0.0f;
    float name_advance = reach_top_bar_text_advance(top_bar->state.current_app_name, name_size);
    float current_app_min_text = metrics.current_app_min_text_width * scale;
    if (name_advance < current_app_min_text)
    {
        name_advance = current_app_min_text;
    }
    float current_app_chrome =
        padding * 2.0f + current_app_icon_size + current_app_icon_gap;
    float current_app_max_width = layout->bounds.width * metrics.current_app_max_width_ratio;
    float current_app_target = current_app_chrome + name_advance;
    if (current_app_target > current_app_max_width)
    {
        current_app_target = current_app_max_width;
    }
    float current_app_width = reach_top_bar_resolve_animated_width(
        top_bar, REACH_TOP_BAR_ANIM_CURRENT_APP_WIDTH, &top_bar->current_app_target_width,
        current_app_target);

    layout->pills[REACH_TOP_BAR_PILL_CURRENT_APP] = reach_top_bar_rect(
        (layout->bounds.width - current_app_width) * 0.5f, 0.0f, current_app_width, height);

    reach_rect_f32 current_app = layout->pills[REACH_TOP_BAR_PILL_CURRENT_APP];
    float current_app_text_x = current_app.x + padding;
    if (current_app_icon_size > 0.0f)
    {
        layout->current_app_icon =
            reach_top_bar_rect(current_app_text_x, (height - current_app_icon_size) * 0.5f,
                               current_app_icon_size, current_app_icon_size);
        current_app_text_x += current_app_icon_size + current_app_icon_gap;
    }
    float current_app_text_advance =
        current_app.x + current_app.width - padding - current_app_text_x;
    if (current_app_text_advance < 0.0f)
    {
        current_app_text_advance = 0.0f;
    }
    layout->current_app_text =
        reach_top_bar_text_run(current_app_text_x, height, current_app_text_advance, name_size);

    for (size_t index = 0; index < REACH_TOP_BAR_PILL_COUNT; ++index)
    {
        layout->pill_visible[index] = layout->pills[index].width > 0.0f ? 1 : 0;
    }

    reach_top_bar_now_playing_relayout(top_bar->now_playing_subfeature, ctx->theme,
                                       layout->now_playing, scale);
}

reach_point_i32 reach_top_bar_local_point(const reach_top_bar_layout *layout, int32_t x, int32_t y)
{
    reach_point_i32 point = {};
    if (layout == nullptr)
    {
        point.x = x;
        point.y = y;
        return point;
    }
    point.x = static_cast<int32_t>((float)x - layout->bounds.x);
    point.y = static_cast<int32_t>((float)y - layout->bounds.y);
    return point;
}

reach_rect_f32 reach_top_bar_rect_to_screen(const reach_top_bar_layout *layout, reach_rect_f32 rect)
{
    if (layout == nullptr)
    {
        return rect;
    }
    rect.x += layout->bounds.x;
    rect.y += layout->bounds.y;
    return rect;
}

reach_top_bar_pointer_region reach_top_bar_pointer_region_at(const reach_top_bar *top_bar,
                                                             int32_t local_x, int32_t local_y)
{
    if (top_bar == nullptr)
    {
        return REACH_TOP_BAR_POINTER_REGION_NONE;
    }
    return reach_top_bar_hit_test(&top_bar->state.layout, local_x, local_y);
}

void reach_top_bar_suppress_power_release(reach_top_bar *top_bar)
{
    if (top_bar != nullptr)
    {
        top_bar->state.power_release_suppressed = 1;
    }
}

static void reach_top_bar_copy_ascii_to_utf16(uint16_t *dst, size_t dst_count, const char *src)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }
    size_t index = 0;
    if (src != nullptr)
    {
        while (index + 1 < dst_count && src[index] != 0)
        {
            dst[index] = (uint16_t)(unsigned char)src[index];
            ++index;
        }
    }
    dst[index] = 0;
}

static int32_t reach_top_bar_utf16_equal(const uint16_t *a, const uint16_t *b)
{
    size_t index = 0;
    if (a == nullptr || b == nullptr)
    {
        return a == b;
    }
    while (a[index] != 0 || b[index] != 0)
    {
        if (a[index] != b[index])
        {
            return 0;
        }
        ++index;
    }
    return 1;
}

static int32_t reach_top_bar_update_clock(reach_top_bar *top_bar)
{
    reach_top_bar_state *state = &top_bar->state;
    reach_clock_snapshot now = {};
    reach_clock_snapshot_take(top_bar->clock, &now);
    if (!now.valid || now.month < 1 || now.month > 12 || now.weekday < 0 || now.weekday > 6)
    {
        return 0;
    }

    static const char *months[] = {"January",   "February", "March",    "April",
                                   "May",       "June",     "July",     "August",
                                   "September", "October",  "November", "December"};
    static const char *days[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                 "Thursday", "Friday", "Saturday"};

    int32_t hour = now.hour % 12;
    if (hour == 0)
    {
        hour = 12;
    }
    const char *suffix = now.hour >= 12 ? "PM" : "AM";

    char time_text[32] = {};
    char date_text[64] = {};
    snprintf(time_text, sizeof(time_text), "%d:%02d %s", hour, now.minute, suffix);
    snprintf(date_text, sizeof(date_text), "%.3s, %.3s %d", days[now.weekday],
             months[now.month - 1], now.day);

    uint16_t next_time[32] = {};
    uint16_t next_date[64] = {};
    reach_top_bar_copy_ascii_to_utf16(next_time, 32, time_text);
    reach_top_bar_copy_ascii_to_utf16(next_date, 64, date_text);
    if (state->clock_initialized &&
        reach_top_bar_utf16_equal(state->clock_time_text, next_time) &&
        reach_top_bar_utf16_equal(state->clock_date_text, next_date))
    {
        return 0;
    }

    reach_copy_utf16(state->clock_time_text, 32, next_time);
    reach_copy_utf16(state->clock_date_text, 64, next_date);
    state->clock_initialized = 1;
    return 1;
}

static void reach_top_bar_update_language(reach_top_bar *top_bar)
{
    reach_input_language_snapshot language = {};
    reach_input_language_service_snapshot_take(top_bar->input_language, &language);
    reach_copy_utf16(top_bar->state.language_code, 8, language.code);
}

void reach_top_bar_begin_reveal_session(reach_top_bar *top_bar)
{
    if (top_bar != nullptr)
    {
        reach_bar_begin_reveal_session(&top_bar->state.visibility);
    }
}

reach_bar_visibility_result
reach_top_bar_update_visibility(reach_top_bar *top_bar,
                                const reach_bar_visibility_request *request)
{
    if (top_bar == nullptr || request == nullptr)
    {
        return reach_bar_visibility_result{};
    }

    reach_bar_visibility_request bar_request = *request;
    bar_request.edge = REACH_BAR_EDGE_TOP;
    bar_request.pointer_sequence_active = top_bar->state.pointer_sequence_active;

    return reach_bar_update_visibility(&top_bar->state.visibility, &top_bar->manager,
                                       REACH_TOP_BAR_ANIM_Y, &bar_request);
}

static int32_t reach_top_bar_width_animation_active(const reach_top_bar *top_bar)
{
    return reach_animation_manager_active(&top_bar->manager,
                                          REACH_TOP_BAR_ANIM_NOW_PLAYING_WIDTH) ||
           reach_animation_manager_active(&top_bar->manager,
                                          REACH_TOP_BAR_ANIM_CURRENT_APP_WIDTH) ||
           reach_animation_manager_active(&top_bar->manager, REACH_TOP_BAR_ANIM_TRAY_WIDTH) ||
           reach_animation_manager_active(&top_bar->manager,
                                          REACH_TOP_BAR_ANIM_QUICK_SETTINGS_WIDTH);
}

reach_bar_reveal_animation reach_top_bar_reveal_animation(const reach_top_bar *top_bar)
{
    reach_bar_reveal_animation animation = {};
    if (top_bar == nullptr)
    {
        return animation;
    }

    animation.position_animating =
        reach_animation_manager_active(&top_bar->manager, REACH_TOP_BAR_ANIM_Y);
    animation.content_animating =
        reach_animation_manager_active(&top_bar->manager, REACH_TOP_BAR_ANIM_POWER_HOVER) ||
        reach_animation_manager_active(&top_bar->manager, REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY) ||
        reach_top_bar_width_animation_active(top_bar) ||
        reach_top_bar_now_playing_scrolling(top_bar->now_playing_subfeature);
    return animation;
}

static void reach_top_bar_capsule_reset(void *capsule)
{
    reach_top_bar *top_bar = static_cast<reach_top_bar *>(capsule);
    if (top_bar == nullptr)
    {
        return;
    }
    reach_bar_visibility_reset(&top_bar->state.visibility);
    reach_top_bar_now_playing_reset(top_bar->now_playing_subfeature);
    top_bar->state.pointer_sequence_active = 0;
    top_bar->state.pressed_control = REACH_TOP_BAR_POINTER_REGION_NONE;
    top_bar->state.feedback_index = REACH_TOP_BAR_FEEDBACK_NONE;
    top_bar->state.feedback_pressed = 0;
    top_bar->state.pressed_tray_index = REACH_TOP_BAR_MAX_TRAY_ICONS;
    top_bar->state.power_hovered = 0;
    top_bar->state.power_release_suppressed = 0;
}

static void reach_top_bar_capsule_on_game_mode(void *capsule, int32_t enabled)
{
    reach_top_bar *top_bar = static_cast<reach_top_bar *>(capsule);
    if (!enabled || top_bar == nullptr)
    {
        return;
    }
    reach_bar_visibility_reset(&top_bar->state.visibility);
    top_bar->state.pointer_sequence_active = 0;
}

static void reach_top_bar_capsule_tick(void *capsule, double delta_seconds,
                                       reach_feature_tick_result *out)
{
    if (out != nullptr)
    {
        *out = {};
    }
    reach_top_bar *top_bar = static_cast<reach_top_bar *>(capsule);
    if (top_bar == nullptr)
    {
        return;
    }

    if (reach_top_bar_update_clock(top_bar) && out != nullptr)
    {
        out->redraw = 1;
        out->relayout = 1;
    }

    if (reach_top_bar_update_system_status(top_bar) && out != nullptr)
    {
        out->redraw = 1;
        out->relayout = 1;
    }

    reach_top_bar_now_playing_update_result now_playing = {};
    reach_top_bar_now_playing_sync(top_bar->now_playing_subfeature, top_bar->now_playing,
                                   &now_playing);
    if (now_playing.changed && out != nullptr)
    {
        out->redraw = 1;
    }
    if (now_playing.visibility_changed && out != nullptr)
    {
        out->relayout = 1;
    }
    if (reach_top_bar_now_playing_tick(top_bar->now_playing_subfeature, delta_seconds) &&
        out != nullptr)
    {
        out->redraw = 1;
    }

    reach_animation_manager *manager = &top_bar->manager;
    int32_t feedback_was_active =
        reach_animation_manager_active(manager, REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY);
    int32_t power_hover_was_active =
        reach_animation_manager_active(manager, REACH_TOP_BAR_ANIM_POWER_HOVER);

    int32_t width_was_active = reach_top_bar_width_animation_active(top_bar);

    reach_animation_manager_tick(manager, delta_seconds);

    int32_t redraw =
        feedback_was_active ||
        reach_animation_manager_active(manager, REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY) ||
        power_hover_was_active ||
        reach_animation_manager_active(manager, REACH_TOP_BAR_ANIM_POWER_HOVER);

    if (width_was_active || reach_top_bar_width_animation_active(top_bar))
    {
        redraw = 1;
        if (out != nullptr)
        {
            out->relayout = 1;
        }
    }

    if (feedback_was_active &&
        !reach_animation_manager_active(manager, REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY) &&
        !top_bar->state.feedback_pressed &&
        reach_animation_manager_value(manager, REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY) <= 0.001f)
    {
        reach_animation_manager_set(manager, REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY, 0.0f);
        top_bar->state.feedback_index = REACH_TOP_BAR_FEEDBACK_NONE;
    }

    if (redraw && out != nullptr)
    {
        out->redraw = 1;
    }
}

static int32_t reach_top_bar_capsule_is_open(const void *capsule)
{
    (void)capsule;
    return 1;
}

static int32_t reach_top_bar_capsule_needs_frame(const void *capsule)
{
    const reach_top_bar *top_bar = static_cast<const reach_top_bar *>(capsule);
    if (top_bar == nullptr)
    {
        return 0;
    }
    return reach_animation_manager_active(&top_bar->manager, REACH_TOP_BAR_ANIM_Y) ||
           reach_animation_manager_active(&top_bar->manager, REACH_TOP_BAR_ANIM_POWER_HOVER) ||
           reach_animation_manager_active(&top_bar->manager, REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY) ||
           reach_top_bar_width_animation_active(top_bar) ||
           reach_top_bar_now_playing_scrolling(top_bar->now_playing_subfeature);
}

static int32_t reach_top_bar_capsule_pointer_sequence_active(const void *capsule)
{
    const reach_top_bar *top_bar = static_cast<const reach_top_bar *>(capsule);
    return top_bar != nullptr && top_bar->state.pointer_sequence_active;
}

static int32_t reach_top_bar_capsule_wants_pointer_move(const void *capsule)
{
    return reach_top_bar_capsule_pointer_sequence_active(capsule);
}

static size_t reach_top_bar_capsule_input_regions(const void *capsule, reach_rect_f32 *out_regions,
                                                  size_t max_regions)
{
    const reach_top_bar *top_bar = static_cast<const reach_top_bar *>(capsule);
    if (top_bar == nullptr || out_regions == nullptr)
    {
        return 0;
    }

    size_t count = 0;
    for (size_t pill = 0; pill < REACH_TOP_BAR_PILL_COUNT && count < max_regions; ++pill)
    {
        if (top_bar->state.layout.pill_visible[pill])
        {
            out_regions[count] = top_bar->state.layout.pills[pill];
            ++count;
        }
    }
    return count;
}

static void reach_top_bar_capsule_apply_event_result(const reach_top_bar_event_result *event_result,
                                                     reach_capsule_pointer_result *out)
{
    out->handled = event_result->handled;
    out->redraw = event_result->redraw;
    out->sync_pointer_subscriptions = event_result->sync_pointer_subscriptions;
    out->action.kind = event_result->action_kind;
    out->action.id = event_result->action_id;
}

static void reach_top_bar_capsule_handle_pointer(void *capsule, const reach_pointer_event *event,
                                                 reach_capsule_pointer_result *out)
{
    if (out != nullptr)
    {
        *out = {};
    }
    reach_top_bar *top_bar = static_cast<reach_top_bar *>(capsule);
    if (top_bar == nullptr || event == nullptr || out == nullptr)
    {
        return;
    }

    reach_point_i32 local = reach_top_bar_local_point(&top_bar->state.layout, event->x, event->y);
    reach_top_bar_event_result event_result = {};
    switch (event->kind)
    {
    case REACH_POINTER_EVENT_DOWN:
        reach_top_bar_pointer_down(top_bar, local.x, local.y, &event_result);
        break;
    case REACH_POINTER_EVENT_UP:
        reach_top_bar_pointer_up(top_bar, local.x, local.y, &event_result);
        break;
    case REACH_POINTER_EVENT_MOVE:
        reach_top_bar_pointer_move(top_bar, local.x, local.y, &event_result);
        break;
    case REACH_POINTER_EVENT_CONTEXT:
        reach_top_bar_pointer_context(top_bar, local.x, local.y, &event_result);
        break;
    case REACH_POINTER_EVENT_CANCEL:
        reach_top_bar_pointer_cancel(top_bar, &event_result);
        break;
    case REACH_POINTER_EVENT_LEAVE:
        reach_top_bar_pointer_leave(top_bar, &event_result);
        break;
    case REACH_POINTER_EVENT_WHEEL:
    case REACH_POINTER_EVENT_MIDDLE:
    default:
        return;
    }
    reach_top_bar_capsule_apply_event_result(&event_result, out);
}

const reach_feature_capsule_ops *reach_top_bar_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        reach_top_bar_capsule_reset,
        reach_top_bar_capsule_tick,
        reach_top_bar_capsule_is_open,
        nullptr,
        reach_top_bar_capsule_on_game_mode,
        reach_top_bar_capsule_needs_frame,
        reach_top_bar_capsule_wants_pointer_move,
        reach_top_bar_capsule_handle_pointer,
        reach_top_bar_capsule_pointer_sequence_active,
        reach_top_bar_capsule_input_regions,
    };
    return &ops;
}
