#include "windows_adapters_internal.h"

#include "reach/ports/media_controls.h"

#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/base.h>
#include <new>
#include <thread>

struct reach_media_controls_adapter
{
    int32_t reserved;
};

static reach_result reach_media_controls_ensure_apartment(void)
{
    try
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        return REACH_OK;
    }
    catch (winrt::hresult_error const &error)
    {
        return error.code() == RPC_E_CHANGED_MODE ? REACH_OK : REACH_ERROR;
    }
}

static reach_result reach_media_controls_with_current_session(
    bool (*execute)(winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession const
                       &session))
{
    REACH_ASSERT(execute != nullptr);
    if (execute == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (reach_media_controls_ensure_apartment() != REACH_OK)
    {
        return REACH_ERROR;
    }

    try
    {
        namespace media = winrt::Windows::Media::Control;
        media::GlobalSystemMediaTransportControlsSessionManager manager =
            media::GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
        media::GlobalSystemMediaTransportControlsSession session = manager.GetCurrentSession();
        if (session == nullptr)
        {
            return REACH_ERROR;
        }
        return execute(session) ? REACH_OK : REACH_ERROR;
    }
    catch (winrt::hresult_error const &)
    {
        return REACH_ERROR;
    }
}

static reach_result reach_media_controls_with_current_session_on_worker(
    bool (*execute)(winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession const
                       &session))
{
    REACH_ASSERT(execute != nullptr);
    if (execute == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = REACH_ERROR;
    try
    {
        std::thread worker([execute, &result]() {
            result = reach_media_controls_with_current_session(execute);
            winrt::uninit_apartment();
        });
        worker.join();
    }
    catch (...)
    {
        return REACH_ERROR;
    }

    return result;
}

static bool reach_media_controls_execute_previous(
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession const &session)
{
    return session.TrySkipPreviousAsync().get();
}

static bool reach_media_controls_execute_play_pause(
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession const &session)
{
    return session.TryTogglePlayPauseAsync().get();
}

static bool reach_media_controls_execute_next(
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession const &session)
{
    return session.TrySkipNextAsync().get();
}

static reach_result reach_media_controls_previous_track(void *userdata)
{
    (void)userdata;
    return reach_media_controls_with_current_session_on_worker(reach_media_controls_execute_previous);
}

static reach_result reach_media_controls_play_pause(void *userdata)
{
    (void)userdata;
    return reach_media_controls_with_current_session_on_worker(
        reach_media_controls_execute_play_pause);
}

static reach_result reach_media_controls_next_track(void *userdata)
{
    (void)userdata;
    return reach_media_controls_with_current_session_on_worker(reach_media_controls_execute_next);
}

static void reach_media_controls_destroy(void *userdata)
{
    delete static_cast<reach_media_controls_adapter *>(userdata);
}

extern "C" reach_result reach_windows_create_media_controls(reach_media_controls_port *out_port)
{
    REACH_ASSERT(out_port != nullptr);
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_media_controls_adapter *adapter = new (std::nothrow) reach_media_controls_adapter();
    if (adapter == nullptr)
    {
        return REACH_ERROR;
    }

    out_port->userdata = adapter;
    out_port->previous_track = reach_media_controls_previous_track;
    out_port->play_pause = reach_media_controls_play_pause;
    out_port->next_track = reach_media_controls_next_track;
    out_port->destroy = reach_media_controls_destroy;
    return REACH_OK;
}
