#include "reach/services/system_status.h"

#include <stdio.h>

static int failures;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++failures;
        printf("FAIL: %s\n", message);
    }
}

typedef struct fake_audio
{
    reach_audio_volume_state state;
    int32_t get_state_supported;
    float last_level;
    int32_t set_level_calls;
    int32_t set_muted_calls;
    int32_t set_muted_fails;
} fake_audio;

static fake_audio audio;

static reach_result fake_get_state(void *userdata, reach_audio_volume_state *out_state)
{
    (void)userdata;
    if (!audio.get_state_supported)
    {
        return REACH_NOT_IMPLEMENTED;
    }
    *out_state = audio.state;
    return REACH_OK;
}

static reach_result fake_set_level(void *userdata, float level)
{
    (void)userdata;
    audio.last_level = level;
    ++audio.set_level_calls;
    return REACH_OK;
}

static reach_result fake_set_muted(void *userdata, int32_t muted)
{
    (void)userdata;
    ++audio.set_muted_calls;
    if (audio.set_muted_fails)
    {
        return REACH_ERROR;
    }
    audio.state.muted = muted ? 1 : 0;
    return REACH_OK;
}

typedef struct fake_controls
{
    int32_t request_calls;
    int32_t request_fails;
    int32_t set_calls;
} fake_controls;

static fake_controls controls;

static reach_result fake_request_bluetooth(void *userdata, int32_t enabled)
{
    (void)userdata;
    (void)enabled;
    ++controls.request_calls;
    return controls.request_fails ? REACH_ERROR : REACH_OK;
}

static reach_result fake_set_bluetooth(void *userdata, int32_t enabled)
{
    (void)userdata;
    (void)enabled;
    ++controls.set_calls;
    return REACH_OK;
}

static reach_system_status *make_service(reach_audio_volume_port audio_port,
                                         reach_system_controls_port controls_port)
{
    audio = {};
    controls = {};
    audio.get_state_supported = 1;
    reach_system_status *service = nullptr;
    if (reach_system_status_create(audio_port, controls_port, nullptr, nullptr, &service) !=
        REACH_OK)
    {
        return nullptr;
    }
    return service;
}

static reach_audio_volume_port full_audio_port(void)
{
    reach_audio_volume_port port = {};
    port.get_state = fake_get_state;
    port.set_level = fake_set_level;
    port.set_muted = fake_set_muted;
    return port;
}

static void test_set_main_volume_unmutes_first(void)
{
    reach_system_status *service = make_service(full_audio_port(), {});
    expect_true(service != nullptr, "service is created");
    audio.state.muted = 1;

    int32_t muted = 0;
    expect_true(reach_system_status_set_main_volume(service, 0.4f, &muted) == REACH_OK,
                "set_main_volume succeeds");
    expect_true(audio.set_muted_calls == 1, "a muted output is unmuted before the level is set");
    expect_true(muted == 0, "the reported mute state reflects the unmute");
    expect_true(audio.set_level_calls == 1 && audio.last_level == 0.4f, "the level is applied");

    reach_system_status_destroy(service);
}

static void test_set_main_volume_reports_live_mute_when_unmute_fails(void)
{
    reach_system_status *service = make_service(full_audio_port(), {});
    audio.state.muted = 1;
    audio.set_muted_fails = 1;

    int32_t muted = 0;
    (void)reach_system_status_set_main_volume(service, 0.5f, &muted);
    expect_true(muted == 1, "a failed unmute leaves the caller's mute state set");

    reach_system_status_destroy(service);
}

static void test_set_main_volume_keeps_caller_state_without_live_read(void)
{
    reach_audio_volume_port port = full_audio_port();
    port.get_state = nullptr;
    reach_system_status *service = make_service(port, {});
    audio.state.muted = 0;

    int32_t muted = 0;
    (void)reach_system_status_set_main_volume(service, 0.25f, &muted);
    expect_true(audio.set_muted_calls == 0, "an unmuted caller state does not trigger an unmute");
    expect_true(muted == 0, "the caller's mute state survives a missing live read");

    reach_system_status_destroy(service);
}

static void test_set_main_volume_clamps(void)
{
    reach_system_status *service = make_service(full_audio_port(), {});

    int32_t muted = 0;
    (void)reach_system_status_set_main_volume(service, 4.0f, &muted);
    expect_true(audio.last_level == 1.0f, "an out-of-range level is clamped to one");
    (void)reach_system_status_set_main_volume(service, -2.0f, &muted);
    expect_true(audio.last_level == 0.0f, "a negative level is clamped to zero");

    reach_system_status_destroy(service);
}

static void test_bluetooth_prefers_the_asynchronous_request(void)
{
    reach_system_controls_port port = {};
    port.request_bluetooth_enabled = fake_request_bluetooth;
    port.set_bluetooth_enabled = fake_set_bluetooth;
    reach_system_status *service = make_service({}, port);

    expect_true(reach_system_status_set_bluetooth_enabled(service, 1) ==
                    REACH_SYSTEM_STATUS_BLUETOOTH_PENDING,
                "an accepted asynchronous request reports pending");
    expect_true(controls.request_calls == 1 && controls.set_calls == 0,
                "the synchronous path is not used when a request is available");

    reach_system_status_destroy(service);
}

static void test_bluetooth_reports_a_rejected_request(void)
{
    reach_system_controls_port port = {};
    port.request_bluetooth_enabled = fake_request_bluetooth;
    reach_system_status *service = make_service({}, port);
    controls.request_fails = 1;

    expect_true(reach_system_status_set_bluetooth_enabled(service, 1) ==
                    REACH_SYSTEM_STATUS_BLUETOOTH_REJECTED,
                "a failed asynchronous request reports rejected");

    reach_system_status_destroy(service);
}

static void test_bluetooth_falls_back_to_the_synchronous_set(void)
{
    reach_system_controls_port port = {};
    port.set_bluetooth_enabled = fake_set_bluetooth;
    reach_system_status *service = make_service({}, port);

    expect_true(reach_system_status_set_bluetooth_enabled(service, 0) ==
                    REACH_SYSTEM_STATUS_BLUETOOTH_APPLIED,
                "the synchronous set reports applied");
    expect_true(controls.set_calls == 1, "the synchronous set is used");

    reach_system_status_destroy(service);
}

static void test_bluetooth_without_a_capable_port_is_unsupported(void)
{
    reach_system_status *service = make_service({}, {});

    expect_true(reach_system_status_set_bluetooth_enabled(service, 1) ==
                    REACH_SYSTEM_STATUS_BLUETOOTH_UNSUPPORTED,
                "a port with neither entry point reports unsupported");

    reach_system_status_destroy(service);
}

static void test_missing_entry_points_are_not_implemented(void)
{
    reach_system_status *service = make_service({}, {});

    expect_true(reach_system_status_set_brightness(service, 0.5f) == REACH_NOT_IMPLEMENTED,
                "brightness without a port entry point is not implemented");
    expect_true(reach_system_status_set_battery_saver_enabled(service, 1) == REACH_NOT_IMPLEMENTED,
                "battery saver without a port entry point is not implemented");
    expect_true(reach_system_status_open_project_menu(service) == REACH_NOT_IMPLEMENTED,
                "the project menu without a port entry point is not implemented");
    expect_true(reach_system_status_open_system_quick_settings(service) == REACH_NOT_IMPLEMENTED,
                "system quick settings without a port entry point is not implemented");

    reach_system_status_destroy(service);
}

static void test_null_service_is_rejected(void)
{
    int32_t muted = 0;
    expect_true(reach_system_status_set_main_volume(nullptr, 0.5f, &muted) ==
                    REACH_INVALID_ARGUMENT,
                "a null service is rejected");
    expect_true(reach_system_status_set_bluetooth_enabled(nullptr, 1) ==
                    REACH_SYSTEM_STATUS_BLUETOOTH_UNSUPPORTED,
                "a null service reports unsupported bluetooth");
}

int main(void)
{
    test_set_main_volume_unmutes_first();
    test_set_main_volume_reports_live_mute_when_unmute_fails();
    test_set_main_volume_keeps_caller_state_without_live_read();
    test_set_main_volume_clamps();
    test_bluetooth_prefers_the_asynchronous_request();
    test_bluetooth_reports_a_rejected_request();
    test_bluetooth_falls_back_to_the_synchronous_set();
    test_bluetooth_without_a_capable_port_is_unsupported();
    test_missing_entry_points_are_not_implemented();
    test_null_service_is_rejected();

    if (failures != 0)
    {
        printf("%d failure(s)\n", failures);
        return 1;
    }

    return 0;
}
