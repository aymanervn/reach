#include "windows_adapters_internal.h"

#include "reach/ports/media_controls.h"

#include "windows_icon_handle_internal.h"

#include <shcore.h>
#include <windows.h>
#include <wincodec.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/base.h>
#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

struct reach_media_controls_adapter
{
    std::mutex mutex;
    std::condition_variable cv;
    std::thread watcher_thread;
    uint16_t cached_title[260];
    uint64_t cached_cover_icon_id;
    int32_t cached_cover_valid;
    int32_t watcher_started;
    int32_t stop_requested;
    int32_t resubscribe_requested;
    DWORD main_thread_id;
    reach_media_controls_change_callback callback;
    void *callback_user;
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

static int32_t reach_media_controls_utf16_equal(const uint16_t *a, const uint16_t *b)
{
    if (a == nullptr || b == nullptr)
    {
        return a == b;
    }

    size_t index = 0;
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

static void reach_media_controls_copy_hstring(uint16_t *dst, size_t dst_count,
                                              winrt::hstring const &src)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }

    wchar_t const *text = src.c_str();
    size_t index = 0;
    while (index + 1 < dst_count && text != nullptr && text[index] != 0)
    {
        dst[index] = static_cast<uint16_t>(text[index]);
        ++index;
    }
    dst[index] = 0;
}

static reach_media_playback_state reach_media_controls_playback_state_from_winrt(
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionPlaybackStatus status)
{
    namespace media = winrt::Windows::Media::Control;
    switch (status)
    {
    case media::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing:
        return REACH_MEDIA_PLAYBACK_PLAYING;
    case media::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused:
        return REACH_MEDIA_PLAYBACK_PAUSED;
    case media::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Stopped:
        return REACH_MEDIA_PLAYBACK_STOPPED;
    default:
        return REACH_MEDIA_PLAYBACK_UNKNOWN;
    }
}

static void reach_media_controls_notify_changed(reach_media_controls_adapter *adapter)
{
    if (adapter == nullptr)
    {
        return;
    }

    reach_media_controls_change_callback callback = nullptr;
    void *callback_user = nullptr;
    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        callback = adapter->callback;
        callback_user = adapter->callback_user;
    }

    if (callback != nullptr)
    {
        callback(callback_user);
    }
    DWORD main_thread_id = 0;
    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        main_thread_id = adapter->main_thread_id;
    }
    if (main_thread_id != 0)
    {
        PostThreadMessageW(main_thread_id, WM_NULL, 0, 0);
    }
}

static void reach_media_controls_invalidate_cached_cover(reach_media_controls_adapter *adapter)
{
    if (adapter == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(adapter->mutex);
    adapter->cached_cover_valid = 0;
}

static void reach_media_controls_clear_cached_cover(reach_media_controls_adapter *adapter)
{
    if (adapter == nullptr)
    {
        return;
    }

    uint64_t cover_icon_id = 0;
    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        cover_icon_id = adapter->cached_cover_icon_id;
        adapter->cached_cover_icon_id = 0;
        adapter->cached_cover_valid = 0;
        adapter->cached_title[0] = 0;
    }

    if (cover_icon_id != 0)
    {
        reach_windows_icon_id_release(cover_icon_id);
    }
}

static HBITMAP reach_media_controls_hbitmap_from_stream(IStream *stream)
{
    if (stream == nullptr)
    {
        return nullptr;
    }

    winrt::com_ptr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(factory.put()));
    if (FAILED(hr))
    {
        return nullptr;
    }

    winrt::com_ptr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad,
                                          decoder.put());
    if (FAILED(hr))
    {
        return nullptr;
    }

    winrt::com_ptr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.put());
    if (FAILED(hr))
    {
        return nullptr;
    }

    UINT width = 0;
    UINT height = 0;
    hr = frame->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0 || width > 1024 || height > 1024)
    {
        return nullptr;
    }

    winrt::com_ptr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(converter.put());
    if (FAILED(hr))
    {
        return nullptr;
    }

    hr = converter->Initialize(frame.get(), GUID_WICPixelFormat32bppPBGRA,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeMedianCut);
    if (FAILED(hr))
    {
        return nullptr;
    }

    std::vector<BYTE> pixels(width * height * 4);
    hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()),
                               pixels.data());
    if (FAILED(hr))
    {
        return nullptr;
    }

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = static_cast<LONG>(width);
    info.bmiHeader.biHeight = -static_cast<LONG>(height);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void *bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bitmap == nullptr || bits == nullptr)
    {
        return nullptr;
    }

    memcpy(bits, pixels.data(), pixels.size());
    return bitmap;
}

static uint64_t reach_media_controls_cover_icon_from_thumbnail(
    winrt::Windows::Storage::Streams::IRandomAccessStreamReference const &thumbnail)
{
    if (thumbnail == nullptr)
    {
        return 0;
    }

    try
    {
        winrt::Windows::Storage::Streams::IRandomAccessStreamWithContentType stream =
            thumbnail.OpenReadAsync().get();
        if (stream == nullptr)
        {
            return 0;
        }

        winrt::com_ptr<IStream> com_stream;
        HRESULT hr = CreateStreamOverRandomAccessStream(stream.as<IUnknown>().get(),
                                                        IID_PPV_ARGS(com_stream.put()));
        if (FAILED(hr))
        {
            return 0;
        }

        HBITMAP bitmap = reach_media_controls_hbitmap_from_stream(com_stream.get());
        return bitmap != nullptr ? reach_windows_icon_id_from_hbitmap(bitmap) : 0;
    }
    catch (winrt::hresult_error const &)
    {
        return 0;
    }
}

static reach_result reach_media_controls_read_state(reach_media_controls_adapter *adapter,
                                                    reach_media_controls_state *out_state)
{
    REACH_ASSERT(adapter != nullptr);
    REACH_ASSERT(out_state != nullptr);
    if (adapter == nullptr || out_state == nullptr)
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
            reach_media_controls_clear_cached_cover(adapter);
            *out_state = {};
            return REACH_OK;
        }

        media::GlobalSystemMediaTransportControlsSessionMediaProperties properties =
            session.TryGetMediaPropertiesAsync().get();
        reach_media_controls_state state = {};
        reach_media_controls_copy_hstring(state.title, 260, properties.Title());
        state.playback =
            reach_media_controls_playback_state_from_winrt(session.GetPlaybackInfo().PlaybackStatus());
        state.has_media = state.title[0] != 0 ? 1 : 0;

        uint64_t old_cover_icon_id = 0;
        {
            std::lock_guard<std::mutex> lock(adapter->mutex);
            if (adapter->cached_cover_valid &&
                reach_media_controls_utf16_equal(adapter->cached_title, state.title))
            {
                state.cover_icon_id = adapter->cached_cover_icon_id;
                *out_state = state;
                return REACH_OK;
            }
        }

        uint64_t cover_icon_id =
            reach_media_controls_cover_icon_from_thumbnail(properties.Thumbnail());

        {
            std::lock_guard<std::mutex> lock(adapter->mutex);
            old_cover_icon_id = adapter->cached_cover_icon_id;
            adapter->cached_cover_icon_id = cover_icon_id;
            adapter->cached_cover_valid = 1;
            reach_copy_utf16(adapter->cached_title, 260, state.title);
            state.cover_icon_id = adapter->cached_cover_icon_id;
        }

        if (old_cover_icon_id != 0 && old_cover_icon_id != cover_icon_id)
        {
            reach_windows_icon_id_release(old_cover_icon_id);
        }

        *out_state = state;
        return REACH_OK;
    }
    catch (winrt::hresult_error const &)
    {
        return REACH_ERROR;
    }
}

static reach_result reach_media_controls_get_state_on_worker(
    reach_media_controls_adapter *adapter, reach_media_controls_state *out_state)
{
    REACH_ASSERT(adapter != nullptr);
    REACH_ASSERT(out_state != nullptr);
    if (adapter == nullptr || out_state == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = REACH_ERROR;
    try
    {
        std::thread worker([adapter, out_state, &result]() {
            result = reach_media_controls_read_state(adapter, out_state);
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

static void reach_media_controls_request_resubscribe(reach_media_controls_adapter *adapter)
{
    if (adapter == nullptr)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        adapter->resubscribe_requested = 1;
    }
    adapter->cv.notify_one();
}

static void reach_media_controls_subscribe_current_session(
    reach_media_controls_adapter *adapter,
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager const
        &manager,
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession *session,
    winrt::event_token *media_properties_token, winrt::event_token *playback_info_token)
{
    namespace media = winrt::Windows::Media::Control;

    if (session == nullptr || media_properties_token == nullptr || playback_info_token == nullptr)
    {
        return;
    }

    if (*session != nullptr)
    {
        (*session).MediaPropertiesChanged(*media_properties_token);
        (*session).PlaybackInfoChanged(*playback_info_token);
    }

    *session = manager.GetCurrentSession();
    *media_properties_token = {};
    *playback_info_token = {};

    if (*session == nullptr)
    {
        reach_media_controls_invalidate_cached_cover(adapter);
        return;
    }

    *media_properties_token =
        (*session).MediaPropertiesChanged([adapter](
            media::GlobalSystemMediaTransportControlsSession const &,
            media::MediaPropertiesChangedEventArgs const &) {
            reach_media_controls_invalidate_cached_cover(adapter);
            reach_media_controls_notify_changed(adapter);
        });

    *playback_info_token =
        (*session).PlaybackInfoChanged([adapter](
            media::GlobalSystemMediaTransportControlsSession const &,
            media::PlaybackInfoChangedEventArgs const &) {
            reach_media_controls_notify_changed(adapter);
        });
}

static void reach_media_controls_watcher_thread_main(reach_media_controls_adapter *adapter)
{
    if (adapter == nullptr)
    {
        return;
    }
    if (reach_media_controls_ensure_apartment() != REACH_OK)
    {
        return;
    }

    try
    {
        namespace media = winrt::Windows::Media::Control;
        media::GlobalSystemMediaTransportControlsSessionManager manager =
            media::GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
        media::GlobalSystemMediaTransportControlsSession session = nullptr;
        winrt::event_token media_properties_token = {};
        winrt::event_token playback_info_token = {};
        winrt::event_token sessions_token =
            manager.SessionsChanged([adapter](
                media::GlobalSystemMediaTransportControlsSessionManager const &,
                media::SessionsChangedEventArgs const &) {
                reach_media_controls_request_resubscribe(adapter);
                reach_media_controls_notify_changed(adapter);
            });

        winrt::event_token current_session_token =
            manager.CurrentSessionChanged([adapter](
                media::GlobalSystemMediaTransportControlsSessionManager const &,
                media::CurrentSessionChangedEventArgs const &) {
                reach_media_controls_invalidate_cached_cover(adapter);
                reach_media_controls_request_resubscribe(adapter);
                reach_media_controls_notify_changed(adapter);
            });

        reach_media_controls_subscribe_current_session(adapter, manager, &session,
                                                       &media_properties_token,
                                                       &playback_info_token);
        reach_media_controls_notify_changed(adapter);

        std::unique_lock<std::mutex> lock(adapter->mutex);
        for (;;)
        {
            adapter->cv.wait(lock, [adapter]() {
                return adapter->stop_requested || adapter->resubscribe_requested;
            });

            if (adapter->stop_requested)
            {
                break;
            }

            if (adapter->resubscribe_requested)
            {
                adapter->resubscribe_requested = 0;
                lock.unlock();
                reach_media_controls_subscribe_current_session(adapter, manager, &session,
                                                               &media_properties_token,
                                                               &playback_info_token);
                lock.lock();
            }
        }

        lock.unlock();
        if (session != nullptr)
        {
            session.MediaPropertiesChanged(media_properties_token);
            session.PlaybackInfoChanged(playback_info_token);
        }
        manager.CurrentSessionChanged(current_session_token);
        manager.SessionsChanged(sessions_token);
    }
    catch (winrt::hresult_error const &)
    {
    }

    winrt::uninit_apartment();
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

static reach_result reach_media_controls_get_state(void *userdata,
                                                   reach_media_controls_state *out_state)
{
    return reach_media_controls_get_state_on_worker(
        static_cast<reach_media_controls_adapter *>(userdata), out_state);
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

static reach_result
reach_media_controls_start_watching(void *userdata, reach_media_controls_change_callback callback,
                                    void *callback_user)
{
    reach_media_controls_adapter *adapter = static_cast<reach_media_controls_adapter *>(userdata);
    if (adapter == nullptr || callback == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        if (adapter->watcher_started)
        {
            adapter->callback = callback;
            adapter->callback_user = callback_user;
            adapter->main_thread_id = GetCurrentThreadId();
            return REACH_OK;
        }

        adapter->callback = callback;
        adapter->callback_user = callback_user;
        adapter->main_thread_id = GetCurrentThreadId();
        adapter->stop_requested = 0;
        adapter->resubscribe_requested = 0;
    }

    try
    {
        adapter->watcher_thread = std::thread(reach_media_controls_watcher_thread_main, adapter);
    }
    catch (...)
    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        adapter->callback = nullptr;
        adapter->callback_user = nullptr;
        return REACH_ERROR;
    }

    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        adapter->watcher_started = 1;
    }
    return REACH_OK;
}

static void reach_media_controls_stop_watching(void *userdata)
{
    reach_media_controls_adapter *adapter = static_cast<reach_media_controls_adapter *>(userdata);
    if (adapter == nullptr)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        adapter->callback = nullptr;
        adapter->callback_user = nullptr;
        adapter->main_thread_id = 0;
        adapter->stop_requested = 1;
        adapter->resubscribe_requested = 0;
    }
    adapter->cv.notify_one();

    if (adapter->watcher_thread.joinable())
    {
        adapter->watcher_thread.join();
    }

    {
        std::lock_guard<std::mutex> lock(adapter->mutex);
        adapter->watcher_started = 0;
        adapter->stop_requested = 0;
        adapter->resubscribe_requested = 0;
        adapter->main_thread_id = 0;
    }
}

static void reach_media_controls_destroy(void *userdata)
{
    reach_media_controls_adapter *adapter = static_cast<reach_media_controls_adapter *>(userdata);
    if (adapter == nullptr)
    {
        return;
    }
    reach_media_controls_stop_watching(adapter);
    reach_media_controls_clear_cached_cover(adapter);
    delete adapter;
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
    out_port->get_state = reach_media_controls_get_state;
    out_port->previous_track = reach_media_controls_previous_track;
    out_port->play_pause = reach_media_controls_play_pause;
    out_port->next_track = reach_media_controls_next_track;
    out_port->start_watching = reach_media_controls_start_watching;
    out_port->stop_watching = reach_media_controls_stop_watching;
    out_port->destroy = reach_media_controls_destroy;
    return REACH_OK;
}
