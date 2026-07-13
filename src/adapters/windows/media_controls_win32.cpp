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
#include <math.h>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

struct reach_media_controls_adapter
{
    std::mutex mutex;
    std::condition_variable cv;
    std::thread watcher_thread;
    uint64_t media_generation;
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

static reach_result reach_media_controls_with_current_session(bool (*execute)(
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession const &session))
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

static reach_result reach_media_controls_with_current_session_on_worker(bool (*execute)(
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession const &session))
{
    REACH_ASSERT(execute != nullptr);
    if (execute == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = REACH_ERROR;
    try
    {
        std::thread worker(
            [execute, &result]()
            {
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

static uint64_t reach_media_controls_generation(reach_media_controls_adapter *adapter)
{
    if (adapter == nullptr)
    {
        return 0;
    }
    std::lock_guard<std::mutex> lock(adapter->mutex);
    return adapter->media_generation;
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

static void reach_media_controls_advance_generation(reach_media_controls_adapter *adapter)
{
    if (adapter == nullptr)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(adapter->mutex);
    ++adapter->media_generation;
    if (adapter->media_generation == 0)
    {
        adapter->media_generation = 1;
    }
}

static reach_color reach_media_controls_cover_accent_from_pixels(const std::vector<BYTE> &pixels,
                                                                 UINT width, UINT height)
{
    reach_color accent = {};
    size_t pixel_count = (size_t)width * (size_t)height;
    if (pixels.empty() || pixel_count == 0)
    {
        return accent;
    }

    size_t step = pixel_count / 4096;
    if (step < 1)
    {
        step = 1;
    }

    double red_sum = 0.0;
    double green_sum = 0.0;
    double blue_sum = 0.0;
    double weight_sum = 0.0;

    for (size_t pixel_index = 0; pixel_index < pixel_count; pixel_index += step)
    {
        size_t byte_index = pixel_index * 4;
        double alpha = (double)pixels[byte_index + 3] / 255.0;
        if (alpha < 0.12)
        {
            continue;
        }

        double blue = (double)pixels[byte_index] / 255.0;
        double green = (double)pixels[byte_index + 1] / 255.0;
        double red = (double)pixels[byte_index + 2] / 255.0;
        if (alpha > 0.0)
        {
            red = fmin(1.0, red / alpha);
            green = fmin(1.0, green / alpha);
            blue = fmin(1.0, blue / alpha);
        }

        double max_channel = fmax(red, fmax(green, blue));
        double min_channel = fmin(red, fmin(green, blue));
        double saturation = max_channel - min_channel;
        double weight = alpha * (0.65 + saturation);
        red_sum += red * weight;
        green_sum += green * weight;
        blue_sum += blue * weight;
        weight_sum += weight;
    }

    if (weight_sum <= 0.0)
    {
        return accent;
    }

    double red = red_sum / weight_sum;
    double green = green_sum / weight_sum;
    double blue = blue_sum / weight_sum;
    double luma = red * 0.2126 + green * 0.7152 + blue * 0.0722;
    if (luma < 0.32)
    {
        double lift = 0.32 - luma;
        red = fmin(1.0, red + lift);
        green = fmin(1.0, green + lift);
        blue = fmin(1.0, blue + lift);
    }
    else if (luma > 0.72)
    {
        double scale = 0.72 / luma;
        red *= scale;
        green *= scale;
        blue *= scale;
    }

    accent.r = (float)red;
    accent.g = (float)green;
    accent.b = (float)blue;
    accent.a = 0.92f;
    return accent;
}

static HBITMAP reach_media_controls_hbitmap_from_stream(IStream *stream, reach_color *out_accent)
{
    if (stream == nullptr)
    {
        return nullptr;
    }
    if (out_accent != nullptr)
    {
        *out_accent = {};
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

    hr = converter->Initialize(frame.get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                               nullptr, 0.0, WICBitmapPaletteTypeMedianCut);
    if (FAILED(hr))
    {
        return nullptr;
    }

    std::vector<BYTE> pixels(width * height * 4);
    hr = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data());
    if (FAILED(hr))
    {
        return nullptr;
    }
    if (out_accent != nullptr)
    {
        *out_accent = reach_media_controls_cover_accent_from_pixels(pixels, width, height);
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
    winrt::Windows::Storage::Streams::IRandomAccessStreamReference const &thumbnail,
    reach_color *out_accent)
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

        HBITMAP bitmap = reach_media_controls_hbitmap_from_stream(com_stream.get(), out_accent);
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
        uint64_t generation = reach_media_controls_generation(adapter);
        namespace media = winrt::Windows::Media::Control;
        media::GlobalSystemMediaTransportControlsSessionManager manager =
            media::GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
        media::GlobalSystemMediaTransportControlsSession session = manager.GetCurrentSession();
        if (session == nullptr)
        {
            *out_state = {};
            out_state->media_generation = generation;
            return REACH_OK;
        }

        media::GlobalSystemMediaTransportControlsSessionMediaProperties properties =
            session.TryGetMediaPropertiesAsync().get();
        reach_media_controls_state state = {};
        reach_media_controls_copy_hstring(state.title, 260, properties.Title());
        reach_media_controls_copy_hstring(state.artist, 260, properties.Artist());
        auto playback_info = session.GetPlaybackInfo();
        auto controls = playback_info.Controls();

        state.playback =
            reach_media_controls_playback_state_from_winrt(playback_info.PlaybackStatus());
        state.previous_enabled = controls.IsPreviousEnabled() ? 1 : 0;
        state.play_pause_enabled = controls.IsPlayPauseToggleEnabled() ? 1 : 0;
        state.next_enabled = controls.IsNextEnabled() ? 1 : 0;
        state.has_media = 1;
        state.media_generation = generation;
        if (reach_media_controls_generation(adapter) != generation)
        {
            return REACH_ERROR;
        }

        *out_state = state;
        return REACH_OK;
    }
    catch (winrt::hresult_error const &)
    {
        return REACH_ERROR;
    }
}

static reach_result reach_media_controls_read_cover(reach_media_controls_adapter *adapter,
                                                     uint64_t media_generation,
                                                     reach_media_cover *out_cover)
{
    REACH_ASSERT(adapter != nullptr);
    REACH_ASSERT(out_cover != nullptr);
    if (adapter == nullptr || out_cover == nullptr || media_generation == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_cover = {};
    out_cover->media_generation = media_generation;
    if (reach_media_controls_generation(adapter) != media_generation)
    {
        return REACH_OK;
    }
    if (reach_media_controls_ensure_apartment() != REACH_OK)
    {
        out_cover->current = 1;
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
            out_cover->current =
                reach_media_controls_generation(adapter) == media_generation ? 1 : 0;
            return REACH_OK;
        }

        media::GlobalSystemMediaTransportControlsSessionMediaProperties properties =
            session.TryGetMediaPropertiesAsync().get();
        if (reach_media_controls_generation(adapter) != media_generation)
        {
            return REACH_OK;
        }

        reach_color accent = {};
        uint64_t cover_icon_id =
            reach_media_controls_cover_icon_from_thumbnail(properties.Thumbnail(), &accent);
        if (reach_media_controls_generation(adapter) != media_generation)
        {
            if (cover_icon_id != 0)
            {
                reach_windows_icon_id_release(cover_icon_id);
            }
            return REACH_OK;
        }

        out_cover->cover_icon_id = cover_icon_id;
        out_cover->cover_accent = accent;
        out_cover->current = 1;
        return REACH_OK;
    }
    catch (winrt::hresult_error const &)
    {
        out_cover->current =
            reach_media_controls_generation(adapter) == media_generation ? 1 : 0;
        return REACH_ERROR;
    }
}

static reach_result reach_media_controls_get_state_on_worker(reach_media_controls_adapter *adapter,
                                                             reach_media_controls_state *out_state)
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
        std::thread worker(
            [adapter, out_state, &result]()
            {
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

static reach_result reach_media_controls_get_cover_on_worker(reach_media_controls_adapter *adapter,
                                                              uint64_t media_generation,
                                                              reach_media_cover *out_cover)
{
    REACH_ASSERT(adapter != nullptr);
    REACH_ASSERT(out_cover != nullptr);
    if (adapter == nullptr || out_cover == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_cover = {};
    out_cover->media_generation = media_generation;

    reach_result result = REACH_ERROR;
    try
    {
        std::thread worker(
            [adapter, media_generation, out_cover, &result]()
            {
                result = reach_media_controls_read_cover(adapter, media_generation, out_cover);
                winrt::uninit_apartment();
            });
        worker.join();
    }
    catch (...)
    {
        out_cover->current =
            reach_media_controls_generation(adapter) == media_generation ? 1 : 0;
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
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager const &manager,
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
        return;
    }

    *media_properties_token = (*session).MediaPropertiesChanged(
        [adapter](media::GlobalSystemMediaTransportControlsSession const &,
                  media::MediaPropertiesChangedEventArgs const &)
        {
            reach_media_controls_advance_generation(adapter);
            reach_media_controls_notify_changed(adapter);
        });

    *playback_info_token = (*session).PlaybackInfoChanged(
        [adapter](media::GlobalSystemMediaTransportControlsSession const &,
                  media::PlaybackInfoChangedEventArgs const &)
        { reach_media_controls_notify_changed(adapter); });
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
        winrt::event_token sessions_token = manager.SessionsChanged(
            [adapter](media::GlobalSystemMediaTransportControlsSessionManager const &,
                      media::SessionsChangedEventArgs const &)
            {
                reach_media_controls_advance_generation(adapter);
                reach_media_controls_request_resubscribe(adapter);
                reach_media_controls_notify_changed(adapter);
            });

        winrt::event_token current_session_token = manager.CurrentSessionChanged(
            [adapter](media::GlobalSystemMediaTransportControlsSessionManager const &,
                      media::CurrentSessionChangedEventArgs const &)
            {
                reach_media_controls_advance_generation(adapter);
                reach_media_controls_request_resubscribe(adapter);
                reach_media_controls_notify_changed(adapter);
            });

        reach_media_controls_subscribe_current_session(
            adapter, manager, &session, &media_properties_token, &playback_info_token);
        reach_media_controls_notify_changed(adapter);

        std::unique_lock<std::mutex> lock(adapter->mutex);
        for (;;)
        {
            adapter->cv.wait(lock, [adapter]()
                             { return adapter->stop_requested || adapter->resubscribe_requested; });

            if (adapter->stop_requested)
            {
                break;
            }

            if (adapter->resubscribe_requested)
            {
                adapter->resubscribe_requested = 0;
                lock.unlock();
                reach_media_controls_subscribe_current_session(
                    adapter, manager, &session, &media_properties_token, &playback_info_token);
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

static reach_result reach_media_controls_get_cover(void *userdata, uint64_t media_generation,
                                                   reach_media_cover *out_cover)
{
    return reach_media_controls_get_cover_on_worker(
        static_cast<reach_media_controls_adapter *>(userdata), media_generation, out_cover);
}

static void reach_media_controls_release_cover(void *userdata, uint64_t cover_icon_id)
{
    (void)userdata;
    reach_windows_icon_id_release(cover_icon_id);
}

static reach_result reach_media_controls_previous_track(void *userdata)
{
    (void)userdata;
    return reach_media_controls_with_current_session_on_worker(
        reach_media_controls_execute_previous);
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
    adapter->media_generation = 1;

    out_port->userdata = adapter;
    out_port->get_state = reach_media_controls_get_state;
    out_port->get_cover = reach_media_controls_get_cover;
    out_port->release_cover = reach_media_controls_release_cover;
    out_port->previous_track = reach_media_controls_previous_track;
    out_port->play_pause = reach_media_controls_play_pause;
    out_port->next_track = reach_media_controls_next_track;
    out_port->start_watching = reach_media_controls_start_watching;
    out_port->stop_watching = reach_media_controls_stop_watching;
    out_port->destroy = reach_media_controls_destroy;
    return REACH_OK;
}
