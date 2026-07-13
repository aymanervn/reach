#include "reach/services/now_playing.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>
#include <thread>
#include <vector>

static const double REACH_NOW_PLAYING_TRANSPORT_SECONDS = 0.3;
static const double REACH_NOW_PLAYING_ABSENCE_SECONDS = 4.0;
static const double REACH_NOW_PLAYING_COVER_QUIET_SECONDS = 0.3;
static const double REACH_NOW_PLAYING_RETRY_SECONDS = 0.25;

static const uint16_t REACH_NOW_PLAYING_UNKNOWN[] = {'U', 'n', 'k', 'n', 'o', 'w', 'n', 0};

typedef std::chrono::steady_clock reach_now_playing_clock;

struct reach_now_playing_service
{
    reach_media_controls_port media_controls = {};
    void (*notify)(void *user) = nullptr;
    void *notify_user = nullptr;

    mutable std::mutex mutex;
    std::condition_variable cv;
    std::condition_variable cover_cv;
    std::thread worker;
    std::thread cover_worker;
    int32_t worker_started = 0;
    int32_t cover_worker_started = 0;
    int32_t watching = 0;
    int32_t stop_requested = 0;
    int32_t refresh_pending = 0;
    int32_t retry_pending = 0;
    reach_now_playing_clock::time_point retry_at = {};

    reach_now_playing_action command = REACH_NOW_PLAYING_ACTION_NONE;
    int32_t command_in_flight = 0;
    int32_t transport_pending = 0;
    int32_t transport_refresh_ready = 0;
    reach_now_playing_clock::time_point transport_deadline = {};

    uint64_t stable_media_generation = 0;
    uint64_t cover_request_generation = 0;
    int32_t cover_request_pending = 0;
    reach_now_playing_clock::time_point cover_request_at = {};

    int32_t absence_pending = 0;
    int32_t absence_confirmation_requested = 0;
    reach_now_playing_clock::time_point absence_deadline = {};

    reach_now_playing_snapshot stable = {};
    reach_now_playing_snapshot published = {};
    std::vector<uint64_t> retired_covers;
};

static int32_t reach_now_playing_text_equal(const uint16_t *a, const uint16_t *b)
{
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

static int32_t reach_now_playing_color_equal(reach_color a, reach_color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static int32_t reach_now_playing_snapshot_equal(const reach_now_playing_snapshot *a,
                                                const reach_now_playing_snapshot *b)
{
    return a->has_session == b->has_session && reach_now_playing_text_equal(a->title, b->title) &&
           reach_now_playing_text_equal(a->artist, b->artist) &&
           a->cover_image_id == b->cover_image_id &&
           reach_now_playing_color_equal(a->cover_accent, b->cover_accent) &&
           a->playback == b->playback && a->previous_enabled == b->previous_enabled &&
           a->play_pause_enabled == b->play_pause_enabled && a->next_enabled == b->next_enabled &&
           a->transport_pending == b->transport_pending;
}

static void reach_now_playing_copy_or_unknown(uint16_t *dst, const uint16_t *src)
{
    reach_copy_utf16(dst, 260, src != nullptr && src[0] != 0 ? src : REACH_NOW_PLAYING_UNKNOWN);
}

static reach_now_playing_snapshot
reach_now_playing_candidate(const reach_media_controls_state *state)
{
    reach_now_playing_snapshot result = {};
    if (state == nullptr || !state->has_media)
    {
        return result;
    }

    result.has_session = 1;
    reach_now_playing_copy_or_unknown(result.title, state->title);
    reach_now_playing_copy_or_unknown(result.artist, state->artist);
    result.playback = state->playback;
    result.previous_enabled = state->previous_enabled ? 1 : 0;
    result.play_pause_enabled = state->play_pause_enabled ? 1 : 0;
    result.next_enabled = state->next_enabled ? 1 : 0;
    return result;
}

static void reach_now_playing_release_port_cover(reach_now_playing_service *service,
                                                 uint64_t cover_image_id)
{
    if (service != nullptr && cover_image_id != 0 &&
        service->media_controls.release_cover != nullptr)
    {
        service->media_controls.release_cover(service->media_controls.userdata, cover_image_id);
    }
}

static int32_t reach_now_playing_publish_locked(reach_now_playing_service *service)
{
    reach_now_playing_snapshot next = service->stable;
    next.transport_pending = service->transport_pending;
    if (service->transport_pending)
    {
        next.previous_enabled = 0;
        next.play_pause_enabled = 0;
        next.next_enabled = 0;
    }
    next.generation = service->published.generation;
    if (reach_now_playing_snapshot_equal(&next, &service->published))
    {
        return 0;
    }
    next.generation = service->published.generation + 1;
    service->published = next;
    return 1;
}

static int32_t reach_now_playing_adopt_core_locked(reach_now_playing_service *service,
                                                   reach_now_playing_snapshot *candidate,
                                                   uint64_t media_generation)
{
    if (candidate->has_session)
    {
        candidate->cover_image_id = service->stable.cover_image_id;
        candidate->cover_accent = service->stable.cover_accent;
        if (!service->stable.has_session ||
            media_generation != service->stable_media_generation)
        {
            service->stable_media_generation = media_generation;
            service->cover_request_generation = media_generation;
            service->cover_request_pending = 1;
            service->cover_request_at =
                reach_now_playing_clock::now() +
                std::chrono::duration_cast<reach_now_playing_clock::duration>(
                    std::chrono::duration<double>(REACH_NOW_PLAYING_COVER_QUIET_SECONDS));
            service->cover_cv.notify_one();
        }
    }
    else
    {
        if (service->stable.cover_image_id != 0)
        {
            service->retired_covers.push_back(service->stable.cover_image_id);
        }
        service->stable_media_generation = media_generation;
        service->cover_request_generation = 0;
        service->cover_request_pending = 0;
    }

    candidate->generation = service->stable.generation + 1;
    service->stable = *candidate;
    service->absence_pending = 0;
    service->absence_confirmation_requested = 0;
    return reach_now_playing_publish_locked(service);
}

static reach_result reach_now_playing_execute(reach_now_playing_service *service,
                                              reach_now_playing_action action)
{
    switch (action)
    {
    case REACH_NOW_PLAYING_ACTION_PREVIOUS:
        return service->media_controls.previous_track != nullptr
                   ? service->media_controls.previous_track(service->media_controls.userdata)
                   : REACH_ERROR;
    case REACH_NOW_PLAYING_ACTION_PLAY_PAUSE:
        return service->media_controls.play_pause != nullptr
                   ? service->media_controls.play_pause(service->media_controls.userdata)
                   : REACH_ERROR;
    case REACH_NOW_PLAYING_ACTION_NEXT:
        return service->media_controls.next_track != nullptr
                   ? service->media_controls.next_track(service->media_controls.userdata)
                   : REACH_ERROR;
    default:
        return REACH_INVALID_ARGUMENT;
    }
}

static void reach_now_playing_notify(reach_now_playing_service *service, int32_t changed)
{
    if (changed && service->notify != nullptr)
    {
        service->notify(service->notify_user);
    }
}

static void reach_now_playing_schedule_retry_locked(reach_now_playing_service *service)
{
    service->retry_pending = 1;
    service->retry_at = reach_now_playing_clock::now() +
                        std::chrono::duration_cast<reach_now_playing_clock::duration>(
                            std::chrono::duration<double>(REACH_NOW_PLAYING_RETRY_SECONDS));
}

static void reach_now_playing_apply_fetch(reach_now_playing_service *service,
                                          reach_media_controls_state *state, reach_result result)
{
    int32_t changed = 0;
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        if (result != REACH_OK)
        {
            reach_now_playing_schedule_retry_locked(service);
            service->cv.notify_one();
            return;
        }

        service->retry_pending = 0;
        reach_now_playing_snapshot candidate = reach_now_playing_candidate(state);
        int32_t stable_core_ready = 0;
        const auto now = reach_now_playing_clock::now();
        if (!candidate.has_session && service->stable.has_session)
        {
            if (!service->absence_pending)
            {
                service->absence_pending = 1;
                service->absence_confirmation_requested = 0;
                service->absence_deadline =
                    now + std::chrono::duration_cast<reach_now_playing_clock::duration>(
                              std::chrono::duration<double>(REACH_NOW_PLAYING_ABSENCE_SECONDS));
            }
            else if (service->absence_confirmation_requested && now >= service->absence_deadline)
            {
                changed = reach_now_playing_adopt_core_locked(service, &candidate,
                                                               state->media_generation);
                stable_core_ready = 1;
            }
        }
        else
        {
            changed = reach_now_playing_adopt_core_locked(service, &candidate,
                                                           state->media_generation);
            stable_core_ready = 1;
        }
        if (stable_core_ready && service->transport_pending && !service->command_in_flight)
        {
            service->transport_refresh_ready = 1;
        }
        service->cv.notify_one();
    }
    reach_now_playing_notify(service, changed);
}

static void reach_now_playing_apply_cover(reach_now_playing_service *service,
                                          reach_media_cover *cover, reach_result result)
{
    int32_t changed = 0;
    uint64_t unused_cover = 0;
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        if (cover->current && cover->media_generation == service->stable_media_generation &&
            service->stable.has_session)
        {
            if (service->stable.cover_image_id != 0)
            {
                service->retired_covers.push_back(service->stable.cover_image_id);
            }
            if (result == REACH_OK)
            {
                service->stable.cover_image_id = cover->cover_icon_id;
                service->stable.cover_accent =
                    cover->cover_icon_id != 0 ? cover->cover_accent : reach_color{};
                cover->cover_icon_id = 0;
            }
            else
            {
                service->stable.cover_image_id = 0;
                service->stable.cover_accent = {};
            }
            service->stable.generation += 1;
            changed = reach_now_playing_publish_locked(service);
        }
        unused_cover = cover->cover_icon_id;
        cover->cover_icon_id = 0;
    }
    reach_now_playing_release_port_cover(service, unused_cover);
    reach_now_playing_notify(service, changed);
}

static void reach_now_playing_cover_worker_main(reach_now_playing_service *service)
{
    for (;;)
    {
        uint64_t media_generation = 0;
        {
            std::unique_lock<std::mutex> lock(service->mutex);
            for (;;)
            {
                service->cover_cv.wait(lock, [service]()
                                       { return service->stop_requested ||
                                                service->cover_request_pending; });
                if (service->stop_requested)
                {
                    return;
                }
                if (reach_now_playing_clock::now() < service->cover_request_at)
                {
                    service->cover_cv.wait_until(lock, service->cover_request_at);
                    continue;
                }
                media_generation = service->cover_request_generation;
                service->cover_request_pending = 0;
                break;
            }
        }

        reach_media_cover cover = {};
        reach_result result =
            service->media_controls.get_cover != nullptr
                ? service->media_controls.get_cover(service->media_controls.userdata,
                                                    media_generation, &cover)
                : REACH_ERROR;
        if (service->media_controls.get_cover == nullptr)
        {
            cover.media_generation = media_generation;
            cover.current = 1;
        }
        reach_now_playing_apply_cover(service, &cover, result);
    }
}

static void reach_now_playing_worker_main(reach_now_playing_service *service)
{
    for (;;)
    {
        reach_now_playing_action command = REACH_NOW_PLAYING_ACTION_NONE;
        int32_t fetch = 0;
        int32_t changed = 0;
        {
            std::unique_lock<std::mutex> lock(service->mutex);
            for (;;)
            {
                const auto now = reach_now_playing_clock::now();
                if (service->stop_requested)
                {
                    return;
                }
                if (service->command != REACH_NOW_PLAYING_ACTION_NONE)
                {
                    command = service->command;
                    service->command = REACH_NOW_PLAYING_ACTION_NONE;
                    service->command_in_flight = 1;
                    break;
                }
                if (service->refresh_pending ||
                    (service->retry_pending && now >= service->retry_at))
                {
                    service->refresh_pending = 0;
                    service->retry_pending = 0;
                    fetch = 1;
                    break;
                }
                if (service->absence_pending && !service->absence_confirmation_requested &&
                    now >= service->absence_deadline)
                {
                    service->absence_confirmation_requested = 1;
                    fetch = 1;
                    break;
                }
                if (service->transport_pending && service->transport_refresh_ready &&
                    now >= service->transport_deadline)
                {
                    service->transport_pending = 0;
                    service->transport_refresh_ready = 0;
                    changed = reach_now_playing_publish_locked(service);
                    break;
                }

                reach_now_playing_clock::time_point wake =
                    reach_now_playing_clock::time_point::max();
                if (service->retry_pending)
                {
                    wake = service->retry_at;
                }
                if (service->absence_pending && !service->absence_confirmation_requested &&
                    service->absence_deadline < wake)
                {
                    wake = service->absence_deadline;
                }
                if (service->transport_pending && service->transport_refresh_ready &&
                    service->transport_deadline < wake)
                {
                    wake = service->transport_deadline;
                }
                if (wake == reach_now_playing_clock::time_point::max())
                {
                    service->cv.wait(lock);
                }
                else
                {
                    service->cv.wait_until(lock, wake);
                }
            }
        }

        reach_now_playing_notify(service, changed);
        if (command != REACH_NOW_PLAYING_ACTION_NONE)
        {
            (void)reach_now_playing_execute(service, command);
            std::lock_guard<std::mutex> lock(service->mutex);
            service->command_in_flight = 0;
            service->refresh_pending = 1;
            service->cv.notify_one();
            continue;
        }
        if (fetch)
        {
            reach_media_controls_state state = {};
            reach_result result =
                service->media_controls.get_state != nullptr
                    ? service->media_controls.get_state(service->media_controls.userdata, &state)
                    : REACH_ERROR;
            reach_now_playing_apply_fetch(service, &state, result);
        }
    }
}

static void reach_now_playing_on_media_changed(void *user)
{
    reach_now_playing_service *service = static_cast<reach_now_playing_service *>(user);
    if (service == nullptr)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(service->mutex);
    service->refresh_pending = 1;
    service->cv.notify_one();
}

reach_result reach_now_playing_service_create(reach_media_controls_port media_controls,
                                              void (*notify)(void *user), void *notify_user,
                                              reach_now_playing_service **out_service)
{
    if (out_service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_now_playing_service *service = new (std::nothrow) reach_now_playing_service();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }
    service->media_controls = media_controls;
    service->notify = notify;
    service->notify_user = notify_user;
    *out_service = service;
    return REACH_OK;
}

reach_result reach_now_playing_service_start(reach_now_playing_service *service)
{
    if (service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    int32_t start_failed = 0;
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        if (!service->worker_started)
        {
            service->stop_requested = 0;
            try
            {
                service->cover_worker =
                    std::thread(reach_now_playing_cover_worker_main, service);
                service->cover_worker_started = 1;
                service->worker = std::thread(reach_now_playing_worker_main, service);
                service->worker_started = 1;
            }
            catch (...)
            {
                service->stop_requested = 1;
                start_failed = 1;
            }
        }
        if (!start_failed)
        {
            service->refresh_pending = 1;
            service->cv.notify_one();
        }
        else
        {
            service->cv.notify_one();
            service->cover_cv.notify_one();
        }
    }
    if (start_failed)
    {
        if (service->worker.joinable())
        {
            service->worker.join();
        }
        if (service->cover_worker.joinable())
        {
            service->cover_worker.join();
        }
        std::lock_guard<std::mutex> lock(service->mutex);
        service->worker_started = 0;
        service->cover_worker_started = 0;
        service->stop_requested = 0;
        return REACH_ERROR;
    }
    if (!service->watching && service->media_controls.start_watching != nullptr)
    {
        reach_result result = service->media_controls.start_watching(
            service->media_controls.userdata, reach_now_playing_on_media_changed, service);
        if (result != REACH_OK)
        {
            reach_now_playing_service_stop(service);
            return result;
        }
        service->watching = 1;
    }
    return REACH_OK;
}

void reach_now_playing_service_stop(reach_now_playing_service *service)
{
    if (service == nullptr)
    {
        return;
    }
    if (service->watching && service->media_controls.stop_watching != nullptr)
    {
        service->media_controls.stop_watching(service->media_controls.userdata);
    }
    service->watching = 0;
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        service->stop_requested = 1;
        service->command = REACH_NOW_PLAYING_ACTION_NONE;
        service->refresh_pending = 0;
        service->retry_pending = 0;
        service->cover_request_pending = 0;
        service->cv.notify_one();
        service->cover_cv.notify_one();
    }
    if (service->worker.joinable())
    {
        service->worker.join();
    }
    if (service->cover_worker.joinable())
    {
        service->cover_worker.join();
    }
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        service->worker_started = 0;
        service->cover_worker_started = 0;
        service->stop_requested = 0;
        service->command_in_flight = 0;
        service->transport_pending = 0;
        service->transport_refresh_ready = 0;
        service->stable_media_generation = 0;
        service->cover_request_generation = 0;
        service->absence_pending = 0;
        service->absence_confirmation_requested = 0;
        (void)reach_now_playing_publish_locked(service);
    }
}

void reach_now_playing_service_destroy(reach_now_playing_service *service)
{
    if (service == nullptr)
    {
        return;
    }
    reach_now_playing_service_stop(service);
    reach_now_playing_release_port_cover(service, service->stable.cover_image_id);
    for (uint64_t cover : service->retired_covers)
    {
        reach_now_playing_release_port_cover(service, cover);
    }
    delete service;
}

void reach_now_playing_service_snapshot(const reach_now_playing_service *service,
                                        reach_now_playing_snapshot *out_snapshot)
{
    if (out_snapshot == nullptr)
    {
        return;
    }
    *out_snapshot = {};
    if (service != nullptr)
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        *out_snapshot = service->published;
    }
}

int32_t reach_now_playing_service_try_action(reach_now_playing_service *service,
                                             reach_now_playing_action action)
{
    if (service == nullptr)
    {
        return 0;
    }
    int32_t changed = 0;
    {
        std::lock_guard<std::mutex> lock(service->mutex);
        if (service->transport_pending || service->command_in_flight ||
            service->command != REACH_NOW_PLAYING_ACTION_NONE || !service->published.has_session)
        {
            return 0;
        }
        int32_t enabled =
            action == REACH_NOW_PLAYING_ACTION_PREVIOUS     ? service->published.previous_enabled
            : action == REACH_NOW_PLAYING_ACTION_PLAY_PAUSE ? service->published.play_pause_enabled
            : action == REACH_NOW_PLAYING_ACTION_NEXT       ? service->published.next_enabled
                                                            : 0;
        if (!enabled)
        {
            return 0;
        }
        service->transport_pending = 1;
        service->transport_refresh_ready = 0;
        service->transport_deadline =
            reach_now_playing_clock::now() +
            std::chrono::duration_cast<reach_now_playing_clock::duration>(
                std::chrono::duration<double>(REACH_NOW_PLAYING_TRANSPORT_SECONDS));
        service->command = action;
        changed = reach_now_playing_publish_locked(service);
        service->cv.notify_one();
    }
    reach_now_playing_notify(service, changed);
    return 1;
}

int32_t reach_now_playing_service_take_retired_cover(reach_now_playing_service *service,
                                                     uint64_t *out_cover_image_id)
{
    if (service == nullptr || out_cover_image_id == nullptr)
    {
        return 0;
    }
    std::lock_guard<std::mutex> lock(service->mutex);
    if (service->retired_covers.empty())
    {
        return 0;
    }
    *out_cover_image_id = service->retired_covers.back();
    service->retired_covers.pop_back();
    return 1;
}

void reach_now_playing_service_release_cover(reach_now_playing_service *service,
                                             uint64_t cover_image_id)
{
    reach_now_playing_release_port_cover(service, cover_image_id);
}
