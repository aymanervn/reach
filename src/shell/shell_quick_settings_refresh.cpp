#include "shell_internal.h"

static float reach_shell_quick_settings_clamp01(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > 1.0f)
    {
        return 1.0f;
    }
    return value;
}

static void reach_shell_quick_settings_audio_thread_main(reach_shell *shell)
{
    for (;;)
    {
        uint32_t generation = 0;

        {
            std::unique_lock<std::mutex> lock(shell->quick_settings_audio_refresh.mutex);
            shell->quick_settings_audio_refresh.cv.wait(
                lock,
                [shell]()
                {
                    return shell->quick_settings_audio_refresh.stop ||
                           shell->quick_settings_audio_refresh.pending;
                });

            if (shell->quick_settings_audio_refresh.stop)
            {
                return;
            }

            generation = shell->quick_settings_audio_refresh.pending_generation;
            shell->quick_settings_audio_refresh.pending = 0;
            shell->quick_settings_audio_refresh.in_flight = 1;
        }

        reach_shell_quick_settings_audio_result result = {};
        result.generation = generation;

        if (shell->audio_volume.get_state != nullptr &&
            shell->audio_volume.get_state(shell->audio_volume.userdata, &result.state) == REACH_OK)
        {
            result.state.level = reach_shell_quick_settings_clamp01(result.state.level);
            result.state.muted = result.state.muted ? 1 : 0;
            result.state_valid = 1;
        }

        if (shell->audio_volume.list_sessions != nullptr &&
            shell->audio_volume.list_sessions(shell->audio_volume.userdata, &result.sessions) ==
                REACH_OK)
        {
            result.sessions_valid = 1;
        }

        if (shell->audio_volume.list_output_devices != nullptr &&
            shell->audio_volume.list_output_devices(shell->audio_volume.userdata,
                                                    &result.output_devices) == REACH_OK)
        {
            result.output_devices_valid = 1;
        }

        {
            std::lock_guard<std::mutex> lock(shell->quick_settings_audio_refresh.mutex);
            shell->quick_settings_audio_refresh.completed_result = result;
            shell->quick_settings_audio_refresh.completed = 1;
            shell->quick_settings_audio_refresh.in_flight = 0;
        }

        if (shell->quick_settings_audio_refresh.notify != nullptr)
        {
            shell->quick_settings_audio_refresh.notify(shell);
        }
    }
}

static reach_result reach_shell_start_quick_settings_audio_thread(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (shell->quick_settings_audio_refresh.thread_started)
    {
        return REACH_OK;
    }

    shell->quick_settings_audio_refresh.stop = 0;

    try
    {
        shell->quick_settings_audio_refresh.thread =
            std::thread(reach_shell_quick_settings_audio_thread_main, shell);
    }
    catch (...)
    {
        return REACH_ERROR;
    }

    shell->quick_settings_audio_refresh.thread_started = 1;
    return REACH_OK;
}

void reach_shell_stop_quick_settings_audio_refresh(reach_shell *shell)
{
    if (shell == nullptr || !shell->quick_settings_audio_refresh.thread_started)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(shell->quick_settings_audio_refresh.mutex);
        shell->quick_settings_audio_refresh.stop = 1;
        shell->quick_settings_audio_refresh.pending = 0;
    }

    shell->quick_settings_audio_refresh.cv.notify_one();

    if (shell->quick_settings_audio_refresh.thread.joinable())
    {
        shell->quick_settings_audio_refresh.thread.join();
    }

    shell->quick_settings_audio_refresh.thread_started = 0;
    shell->quick_settings_audio_refresh.stop = 0;
    shell->quick_settings_audio_refresh.pending = 0;
    shell->quick_settings_audio_refresh.in_flight = 0;
    shell->quick_settings_audio_refresh.completed = 0;
}

void reach_shell_start_quick_settings_audio_refresh(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    if (reach_shell_start_quick_settings_audio_thread(shell) != REACH_OK)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(shell->quick_settings_audio_refresh.mutex);
        ++shell->quick_settings_audio_refresh.generation;
        shell->quick_settings_audio_refresh.pending_generation =
            shell->quick_settings_audio_refresh.generation;
        shell->quick_settings_audio_refresh.pending = 1;
    }

    shell->quick_settings_audio_refresh.cv.notify_one();
}

void reach_shell_apply_quick_settings_audio_refresh_result(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    reach_shell_quick_settings_audio_result result = {};
    int32_t has_result = 0;

    {
        std::lock_guard<std::mutex> lock(shell->quick_settings_audio_refresh.mutex);
        if (shell->quick_settings_audio_refresh.completed)
        {
            result = shell->quick_settings_audio_refresh.completed_result;
            shell->quick_settings_audio_refresh.completed = 0;
            has_result = 1;
        }
    }

    if (!has_result)
    {
        return;
    }

    if (result.generation != shell->quick_settings_audio_refresh.generation)
    {
        return;
    }

    reach_shell_release_quick_settings_audio_render_icons(shell);

    if (result.state_valid)
    {
        shell->quick_settings_audio_state = result.state;
        reach_quick_settings_model_set_main_volume(&shell->quick_settings_model, result.state.level,
                                                   result.state.muted);
    }

    if (result.sessions_valid)
    {
        shell->quick_settings_audio_sessions = result.sessions;
        reach_quick_settings_model_set_sessions(&shell->quick_settings_model, &result.sessions);
    }
    else
    {
        shell->quick_settings_audio_sessions = {};
        reach_quick_settings_model_set_sessions(&shell->quick_settings_model, nullptr);
    }

    if (result.output_devices_valid)
    {
        shell->quick_settings_output_devices = result.output_devices;
        reach_quick_settings_model_set_output_devices(&shell->quick_settings_model,
                                                      &result.output_devices);
    }
    else
    {
        shell->quick_settings_output_devices = {};
        reach_quick_settings_model_set_output_devices(&shell->quick_settings_model, nullptr);
    }

    if (shell->quick_settings_open)
    {
        reach_shell_relayout_quick_settings(shell, 1);
    }

    shell->quick_settings.dirty_flags = 1;
    reach_shell_request_update(shell);
}

int32_t reach_shell_quick_settings_audio_refresh_work_pending(const reach_shell *shell)
{
    if (shell == nullptr)
    {
        return 0;
    }

    reach_shell *mutable_shell = const_cast<reach_shell *>(shell);
    std::lock_guard<std::mutex> lock(mutable_shell->quick_settings_audio_refresh.mutex);
    return mutable_shell->quick_settings_audio_refresh.pending ||
           mutable_shell->quick_settings_audio_refresh.in_flight ||
           mutable_shell->quick_settings_audio_refresh.completed;
}

static void reach_shell_quick_settings_system_thread_main(reach_shell *shell)
{
    for (;;)
    {
        uint32_t generation = 0;
        uint32_t change_flags = 0;

        {
            std::unique_lock<std::mutex> lock(shell->quick_settings_system_refresh.mutex);
            shell->quick_settings_system_refresh.cv.wait(
                lock,
                [shell]()
                {
                    return shell->quick_settings_system_refresh.stop ||
                           shell->quick_settings_system_refresh.pending;
                });

            if (shell->quick_settings_system_refresh.stop)
            {
                return;
            }

            generation = shell->quick_settings_system_refresh.pending_generation;
            change_flags = shell->quick_settings_system_refresh.pending_change_flags;
            shell->quick_settings_system_refresh.pending = 0;
            shell->quick_settings_system_refresh.pending_change_flags = 0;
            shell->quick_settings_system_refresh.in_flight = 1;
        }

        reach_shell_quick_settings_system_result result = {};
        result.generation = generation;
        result.change_flags = change_flags;

        if (shell->system_controls.get_network_state != nullptr &&
            shell->system_controls.get_network_state(shell->system_controls.userdata,
                                                     &result.network) == REACH_OK)
        {
            result.network_valid = 1;
        }
        else
        {
            result.network.kind = REACH_NETWORK_KIND_NONE;
            result.network.connected = 0;
        }

        if (shell->system_controls.get_bluetooth_state != nullptr &&
            shell->system_controls.get_bluetooth_state(shell->system_controls.userdata,
                                                       &result.bluetooth) == REACH_OK)
        {
            result.bluetooth_valid = 1;
        }
        else
        {
            result.bluetooth.available = 0;
            result.bluetooth.enabled = 0;
        }

        if (shell->system_controls.get_power_state != nullptr &&
            shell->system_controls.get_power_state(shell->system_controls.userdata,
                                                   &result.power) == REACH_OK)
        {
            result.power_valid = 1;
        }

        if (shell->system_controls.get_brightness_state != nullptr &&
            shell->system_controls.get_brightness_state(shell->system_controls.userdata,
                                                        &result.brightness) == REACH_OK)
        {
            result.brightness_valid = 1;
        }

        {
            std::lock_guard<std::mutex> lock(shell->quick_settings_system_refresh.mutex);
            shell->quick_settings_system_refresh.completed_result = result;
            shell->quick_settings_system_refresh.completed = 1;
            shell->quick_settings_system_refresh.in_flight = 0;
        }

        if (shell->quick_settings_system_refresh.notify != nullptr)
        {
            shell->quick_settings_system_refresh.notify(shell);
        }
    }
}

static reach_result reach_shell_start_quick_settings_system_thread(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (shell->quick_settings_system_refresh.thread_started)
    {
        return REACH_OK;
    }

    shell->quick_settings_system_refresh.stop = 0;

    try
    {
        shell->quick_settings_system_refresh.thread =
            std::thread(reach_shell_quick_settings_system_thread_main, shell);
    }
    catch (...)
    {
        return REACH_ERROR;
    }

    shell->quick_settings_system_refresh.thread_started = 1;
    return REACH_OK;
}

void reach_shell_stop_quick_settings_system_refresh(reach_shell *shell)
{
    if (shell == nullptr || !shell->quick_settings_system_refresh.thread_started)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(shell->quick_settings_system_refresh.mutex);
        shell->quick_settings_system_refresh.stop = 1;
        shell->quick_settings_system_refresh.pending = 0;
        shell->quick_settings_system_refresh.pending_change_flags = 0;
    }

    shell->quick_settings_system_refresh.cv.notify_one();

    if (shell->quick_settings_system_refresh.thread.joinable())
    {
        shell->quick_settings_system_refresh.thread.join();
    }

    shell->quick_settings_system_refresh.thread_started = 0;
    shell->quick_settings_system_refresh.stop = 0;
    shell->quick_settings_system_refresh.pending = 0;
    shell->quick_settings_system_refresh.in_flight = 0;
    shell->quick_settings_system_refresh.completed = 0;
    shell->quick_settings_system_refresh.pending_change_flags = 0;
}

void reach_shell_start_quick_settings_system_refresh(reach_shell *shell, uint32_t change_flags)
{
    if (shell == nullptr)
    {
        return;
    }

    if (reach_shell_start_quick_settings_system_thread(shell) != REACH_OK)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(shell->quick_settings_system_refresh.mutex);
        ++shell->quick_settings_system_refresh.generation;
        shell->quick_settings_system_refresh.pending_generation =
            shell->quick_settings_system_refresh.generation;
        shell->quick_settings_system_refresh.pending_change_flags |= change_flags;
        shell->quick_settings_system_refresh.pending = 1;
    }

    shell->quick_settings_system_refresh.cv.notify_one();
}

void reach_shell_apply_quick_settings_system_refresh_result(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    reach_shell_quick_settings_system_result result = {};
    int32_t has_result = 0;

    {
        std::lock_guard<std::mutex> lock(shell->quick_settings_system_refresh.mutex);
        if (shell->quick_settings_system_refresh.completed)
        {
            result = shell->quick_settings_system_refresh.completed_result;
            shell->quick_settings_system_refresh.completed = 0;
            has_result = 1;
        }
    }

    if (!has_result)
    {
        return;
    }

    if (result.generation != shell->quick_settings_system_refresh.generation)
    {
        return;
    }

    reach_power_state previous_power = shell->quick_settings_model.power;
    reach_brightness_state previous_brightness = shell->quick_settings_model.brightness;
    int32_t bluetooth_pending = shell->quick_settings_model.bluetooth_pending;

    reach_quick_settings_model_set_system_states(&shell->quick_settings_model, &result.network,
                                                 &result.bluetooth, &result.power,
                                                 &result.brightness);

    if ((result.change_flags & REACH_SYSTEM_CONTROLS_CHANGE_BLUETOOTH) != 0 && bluetooth_pending)
    {
        reach_quick_settings_model_set_bluetooth_pending(&shell->quick_settings_model, 0, 0);
    }

    int32_t layout_changed =
        previous_power.has_battery != shell->quick_settings_model.power.has_battery ||
        previous_brightness.available != shell->quick_settings_model.brightness.available;

    if (layout_changed && shell->quick_settings_open)
    {
        reach_shell_relayout_quick_settings(shell, 1);
    }

    shell->quick_settings.dirty_flags = 1;
    shell->dirty.render = 1;
    reach_shell_request_update(shell);
}

int32_t reach_shell_quick_settings_system_refresh_work_pending(const reach_shell *shell)
{
    if (shell == nullptr)
    {
        return 0;
    }

    reach_shell *mutable_shell = const_cast<reach_shell *>(shell);
    std::lock_guard<std::mutex> lock(mutable_shell->quick_settings_system_refresh.mutex);
    return mutable_shell->quick_settings_system_refresh.pending ||
           mutable_shell->quick_settings_system_refresh.in_flight ||
           mutable_shell->quick_settings_system_refresh.completed;
}

void reach_shell_on_system_controls_changed(void *user, uint32_t change_flags)
{
    reach_shell *shell = static_cast<reach_shell *>(user);
    if (shell == nullptr || change_flags == 0)
    {
        return;
    }

    shell->quick_settings_system_change_flags.fetch_or(change_flags);
}

void reach_shell_process_quick_settings_system_changes(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    uint32_t change_flags = shell->quick_settings_system_change_flags.exchange(0);
    if (change_flags == 0 || !shell->quick_settings_open)
    {
        return;
    }

    reach_shell_start_quick_settings_system_refresh(shell, change_flags);
}
