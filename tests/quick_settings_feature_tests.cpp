#include "reach/features/quick_settings.h"
#include "reach/features/popup.h"

#include <math.h>
#include <stdio.h>

static int g_failures = 0;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++g_failures;
        printf("FAIL: %s\n", message);
    }
}

static void expect_near(float actual, float expected, float epsilon, const char *message)
{
    if (fabsf(actual - expected) > epsilon)
    {
        ++g_failures;
        printf("FAIL: %s: expected %.3f, got %.3f\n", message, expected, actual);
    }
}

static void copy_ascii(uint16_t *dst, size_t dst_count, const char *src)
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

static reach_quick_settings_model test_model_with_sessions(size_t count)
{
    reach_quick_settings_model model = {};
    reach_quick_settings_model_init(&model);
    reach_quick_settings_model_set_main_volume(&model, 0.5f, 0);

    reach_audio_volume_session_list sessions = {};
    sessions.count = count;
    for (size_t index = 0; index < count && index < REACH_AUDIO_VOLUME_MAX_SESSIONS; ++index)
    {
        sessions.sessions[index].level = 0.25f + (float)index * 0.05f;
        sessions.sessions[index].muted = 0;
        sessions.sessions[index].process_id = (uint32_t)(1000 + index);
        sessions.sessions[index].icon_id = (uint64_t)(9000 + index);
        copy_ascii(sessions.sessions[index].session_instance_id,
                   REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY, index == 0 ? "session-a" : "session-b");
        copy_ascii(sessions.sessions[index].label, REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY,
                   index == 0 ? "App A.exe" : "App B.exe");
    }

    reach_quick_settings_model_set_sessions(&model, &sessions);
    return model;
}

static void test_model_clamps_volume(void)
{
    reach_quick_settings_model model = {};
    reach_quick_settings_model_init(&model);

    reach_quick_settings_model_set_main_volume(&model, -1.0f, 0);
    expect_near(model.main_volume_level, 0.0f, 0.001f, "volume clamps below zero");

    reach_quick_settings_model_set_main_volume(&model, 2.0f, 1);
    expect_near(model.main_volume_level, 1.0f, 0.001f, "volume clamps above one");
    expect_true(model.main_muted == 1, "muted flag normalizes to one");
}

static void test_session_list_cap_is_respected(void)
{
    reach_quick_settings_model model =
        test_model_with_sessions(REACH_AUDIO_VOLUME_MAX_SESSIONS + 4);

    expect_true(model.sessions.count == REACH_AUDIO_VOLUME_MAX_SESSIONS, "session model caps list");
}

static void test_volume_icon_selection(void)
{
    expect_true(reach_quick_settings_volume_icon_id(0.8f, 1) == REACH_VECTOR_ICON_VOLUME_ZERO,
                "muted volume uses zero icon");
    expect_true(reach_quick_settings_volume_icon_id(0.0f, 0) == REACH_VECTOR_ICON_VOLUME_ZERO,
                "zero volume uses zero icon");
    expect_true(reach_quick_settings_volume_icon_id(0.25f, 0) == REACH_VECTOR_ICON_VOLUME_LOW,
                "low volume uses low icon");
    expect_true(reach_quick_settings_volume_icon_id(0.75f, 0) == REACH_VECTOR_ICON_VOLUME_HIGH,
                "high volume uses high icon");
}

static void test_expansion_keeps_popup_anchor_position(void)
{
    reach_quick_settings *quick_settings = nullptr;
    expect_true(reach_quick_settings_create(&quick_settings) == REACH_OK,
                "quick settings capsule is created");
    if (quick_settings == nullptr)
    {
        return;
    }

    reach_quick_settings_layout_context ctx = {};
    ctx.theme = reach_theme_default();
    ctx.dpi_scale = 1.25f;
    ctx.anchor_button = {120.25f, 8.25f, 32.0f, 24.0f};
    ctx.monitor = {0.0f, 0.0f, 1920.0f, 1080.0f};
    ctx.bar_edge_y = 40.25f;
    ctx.drop_direction = REACH_POPUP_DROP_DOWN;

    (void)reach_quick_settings_set_open(quick_settings, 1);
    reach_quick_settings_refresh_layout(quick_settings, &ctx);
    const reach_quick_settings_state *state = reach_quick_settings_state_ptr(quick_settings);
    const float narrow_width = state->bounds.width;
    const float content_width = state->content_bounds.width;

    reach_theme wide_border_theme = *reach_theme_default();
    wide_border_theme.border_thickness = 3.0f;
    ctx.theme = &wide_border_theme;
    reach_quick_settings_refresh_layout(quick_settings, &ctx);
    state = reach_quick_settings_state_ptr(quick_settings);
    expect_near(state->bounds.width - narrow_width, 5.0f, 0.001f,
                "quick settings outer width derives both DPI-scaled border sides");
    expect_near(state->content_bounds.width, content_width, 0.001f,
                "quick settings content width stays stable across border widths");
    const float initial_y = state->bounds.y;

    (void)reach_quick_settings_toggle_expanded(quick_settings);
    reach_quick_settings_relayout(quick_settings, &ctx, 1);
    (void)reach_quick_settings_update_open_animation(quick_settings, &ctx);

    expect_near(reach_quick_settings_state_ptr(quick_settings)->bounds.y, initial_y, 0.001f,
                "height animation preserves the popup anchor position");
    reach_quick_settings_destroy(quick_settings);
}

int main(void)
{
    test_model_clamps_volume();
    test_session_list_cap_is_respected();
    test_volume_icon_selection();
    test_expansion_keeps_popup_anchor_position();

    if (g_failures != 0)
    {
        printf("%d quick settings feature test(s) failed.\n", g_failures);
        return 1;
    }

    printf("quick settings feature tests passed.\n");
    return 0;
}
