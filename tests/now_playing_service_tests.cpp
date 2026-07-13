#include "reach/services/now_playing.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdio.h>
#include <thread>
#include <vector>

static int failures;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++failures;
        fprintf(stderr, "FAILED: %s\n", message);
    }
}

static void copy_ascii(uint16_t *dst, size_t dst_count, const char *src)
{
    size_t index = 0;
    while (src != nullptr && src[index] != 0 && index + 1 < dst_count)
    {
        dst[index] = (uint16_t)(unsigned char)src[index];
        ++index;
    }
    dst[index] = 0;
}

static int text_equals_ascii(const uint16_t *text, const char *expected)
{
    size_t index = 0;
    while (expected[index] != 0)
    {
        if (text[index] != (uint16_t)(unsigned char)expected[index])
        {
            return 0;
        }
        ++index;
    }
    return text[index] == 0;
}

struct fake_media_controls
{
    std::mutex mutex;
    std::condition_variable cv;
    reach_media_controls_state state = {};
    reach_media_controls_change_callback callback = nullptr;
    void *callback_user = nullptr;
    uint64_t blocked_cover_generation = 0;
    int32_t allow_blocked_cover = 0;
    uint64_t failed_cover_generation = 0;
    std::vector<uint64_t> cover_requests;
    std::vector<uint64_t> released_covers;
    int32_t next_calls = 0;
    int32_t next_clears_media = 0;
};

static reach_result fake_get_state(void *userdata, reach_media_controls_state *out_state)
{
    fake_media_controls *fake = static_cast<fake_media_controls *>(userdata);
    if (fake == nullptr || out_state == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lock(fake->mutex);
    *out_state = fake->state;
    return REACH_OK;
}

static reach_result fake_get_cover(void *userdata, uint64_t media_generation,
                                   reach_media_cover *out_cover)
{
    fake_media_controls *fake = static_cast<fake_media_controls *>(userdata);
    if (fake == nullptr || out_cover == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    int32_t current = 0;
    int32_t failed = 0;
    {
        std::unique_lock<std::mutex> lock(fake->mutex);
        fake->cover_requests.push_back(media_generation);
        fake->cv.notify_all();
        fake->cv.wait(lock, [fake, media_generation]()
                      { return fake->blocked_cover_generation != media_generation ||
                               fake->allow_blocked_cover; });
        current = fake->state.has_media && fake->state.media_generation == media_generation;
        failed = current && fake->failed_cover_generation == media_generation;
    }

    *out_cover = {};
    out_cover->media_generation = media_generation;
    out_cover->current = current;
    if (!current)
    {
        return REACH_OK;
    }
    if (failed)
    {
        return REACH_ERROR;
    }
    out_cover->cover_icon_id = 1000 + media_generation;
    out_cover->cover_accent = {0.2f, 0.4f, 0.6f, 0.9f};
    return REACH_OK;
}

static void fake_release_cover(void *userdata, uint64_t cover_icon_id)
{
    fake_media_controls *fake = static_cast<fake_media_controls *>(userdata);
    std::lock_guard<std::mutex> lock(fake->mutex);
    fake->released_covers.push_back(cover_icon_id);
}

static reach_result fake_next_track(void *userdata)
{
    fake_media_controls *fake = static_cast<fake_media_controls *>(userdata);
    std::lock_guard<std::mutex> lock(fake->mutex);
    ++fake->next_calls;
    if (fake->next_clears_media)
    {
        uint64_t next_generation = fake->state.media_generation + 1;
        fake->state = {};
        fake->state.media_generation = next_generation;
    }
    return REACH_OK;
}

static reach_result fake_start_watching(void *userdata,
                                        reach_media_controls_change_callback callback,
                                        void *callback_user)
{
    fake_media_controls *fake = static_cast<fake_media_controls *>(userdata);
    std::lock_guard<std::mutex> lock(fake->mutex);
    fake->callback = callback;
    fake->callback_user = callback_user;
    return REACH_OK;
}

static void fake_stop_watching(void *userdata)
{
    fake_media_controls *fake = static_cast<fake_media_controls *>(userdata);
    std::lock_guard<std::mutex> lock(fake->mutex);
    fake->callback = nullptr;
    fake->callback_user = nullptr;
}

static void fake_set_state(fake_media_controls *fake, uint64_t media_generation,
                           const char *title, int32_t has_media)
{
    reach_media_controls_change_callback callback = nullptr;
    void *callback_user = nullptr;
    {
        std::lock_guard<std::mutex> lock(fake->mutex);
        fake->state = {};
        fake->state.has_media = has_media;
        fake->state.media_generation = media_generation;
        if (has_media)
        {
            copy_ascii(fake->state.title, 260, title);
            copy_ascii(fake->state.artist, 260, "Artist");
            fake->state.playback = REACH_MEDIA_PLAYBACK_PLAYING;
            fake->state.previous_enabled = 1;
            fake->state.play_pause_enabled = 1;
            fake->state.next_enabled = 1;
        }
        callback = fake->callback;
        callback_user = fake->callback_user;
    }
    if (callback != nullptr)
    {
        callback(callback_user);
    }
}

template <typename Predicate>
static int32_t wait_for_snapshot(reach_now_playing_service *service, Predicate predicate,
                                 reach_now_playing_snapshot *out_snapshot = nullptr,
                                 int32_t timeout_ms = 1500)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    do
    {
        reach_now_playing_snapshot snapshot = {};
        reach_now_playing_service_snapshot(service, &snapshot);
        if (predicate(snapshot))
        {
            if (out_snapshot != nullptr)
            {
                *out_snapshot = snapshot;
            }
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    } while (std::chrono::steady_clock::now() < deadline);
    return 0;
}

static int32_t wait_for_cover_request(fake_media_controls *fake, uint64_t media_generation)
{
    std::unique_lock<std::mutex> lock(fake->mutex);
    return fake->cv.wait_for(lock, std::chrono::milliseconds(1500),
                             [fake, media_generation]()
                             {
                                 for (uint64_t request : fake->cover_requests)
                                 {
                                     if (request == media_generation)
                                     {
                                         return true;
                                     }
                                 }
                                 return false;
                             })
               ? 1
               : 0;
}

static int32_t cover_was_requested(fake_media_controls *fake, uint64_t media_generation)
{
    std::lock_guard<std::mutex> lock(fake->mutex);
    for (uint64_t request : fake->cover_requests)
    {
        if (request == media_generation)
        {
            return 1;
        }
    }
    return 0;
}

static void test_progressive_latest_only_publication()
{
    fake_media_controls fake = {};
    fake.blocked_cover_generation = 1;
    fake_set_state(&fake, 1, "First", 1);

    reach_media_controls_port port = {};
    port.userdata = &fake;
    port.get_state = fake_get_state;
    port.get_cover = fake_get_cover;
    port.release_cover = fake_release_cover;
    port.next_track = fake_next_track;
    port.start_watching = fake_start_watching;
    port.stop_watching = fake_stop_watching;

    reach_now_playing_service *service = nullptr;
    expect_true(reach_now_playing_service_create(port, nullptr, nullptr, &service) == REACH_OK,
                "service creation succeeds");
    auto service_started_at = std::chrono::steady_clock::now();
    expect_true(reach_now_playing_service_start(service) == REACH_OK,
                "service start succeeds");

    reach_now_playing_snapshot snapshot = {};
    expect_true(wait_for_snapshot(
                    service,
                    [](const reach_now_playing_snapshot &value)
                    { return value.has_session && text_equals_ascii(value.title, "First"); },
                    &snapshot),
                "core state publishes while first cover is blocked");
    expect_true(snapshot.cover_image_id == 0, "first core generation does not wait for cover");
    expect_true(snapshot.next_enabled, "core controls publish before cover");
    expect_true(wait_for_cover_request(&fake, 1), "first cover read starts");
    auto first_cover_delay_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - service_started_at)
                                    .count();
    expect_true(first_cover_delay_ms >= 250,
                "cover loading waits for the media-generation quiet period");

    {
        std::lock_guard<std::mutex> lock(fake.mutex);
        fake.allow_blocked_cover = 1;
    }
    fake.cv.notify_all();
    expect_true(wait_for_snapshot(service, [](const reach_now_playing_snapshot &value)
                                  { return value.cover_image_id == 1001; }),
                "first cover enriches the published state");

    {
        std::lock_guard<std::mutex> lock(fake.mutex);
        fake.blocked_cover_generation = 2;
        fake.allow_blocked_cover = 0;
    }
    fake_set_state(&fake, 2, "Second", 1);
    expect_true(wait_for_cover_request(&fake, 2), "second cover read starts");
    expect_true(wait_for_snapshot(
                    service,
                    [](const reach_now_playing_snapshot &value)
                    {
                        return text_equals_ascii(value.title, "Second") &&
                               value.cover_image_id == 1001;
                    }),
                "new core generation temporarily retains the previous cover");

    fake_set_state(&fake, 3, "Third", 1);
    expect_true(wait_for_snapshot(service, [](const reach_now_playing_snapshot &value)
                                  { return text_equals_ascii(value.title, "Third"); }),
                "intermediate core generation publishes");
    fake_set_state(&fake, 4, "Fourth", 1);
    expect_true(wait_for_snapshot(service, [](const reach_now_playing_snapshot &value)
                                  { return text_equals_ascii(value.title, "Fourth"); }),
                "latest core generation publishes");

    {
        std::lock_guard<std::mutex> lock(fake.mutex);
        fake.allow_blocked_cover = 1;
    }
    fake.cv.notify_all();
    expect_true(wait_for_snapshot(service, [](const reach_now_playing_snapshot &value)
                                  { return value.cover_image_id == 1004; }),
                "latest cover replaces the retained cover");
    expect_true(!cover_was_requested(&fake, 3), "intermediate cover request is coalesced");

    {
        std::lock_guard<std::mutex> lock(fake.mutex);
        fake.blocked_cover_generation = 5;
        fake.allow_blocked_cover = 0;
        fake.failed_cover_generation = 5;
    }
    fake_set_state(&fake, 5, "Fifth", 1);
    expect_true(wait_for_cover_request(&fake, 5), "failed current cover read starts");
    expect_true(wait_for_snapshot(
                    service,
                    [](const reach_now_playing_snapshot &value)
                    {
                        return text_equals_ascii(value.title, "Fifth") &&
                               value.cover_image_id == 1004;
                    }),
                "old cover remains while the current cover is loading");
    {
        std::lock_guard<std::mutex> lock(fake.mutex);
        fake.allow_blocked_cover = 1;
    }
    fake.cv.notify_all();
    expect_true(wait_for_snapshot(
                    service,
                    [](const reach_now_playing_snapshot &value)
                    {
                        return text_equals_ascii(value.title, "Fifth") &&
                               value.cover_image_id == 0;
                    }),
                "current cover failure clears to the placeholder");

    fake_set_state(&fake, 6, nullptr, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    reach_now_playing_service_snapshot(service, &snapshot);
    expect_true(snapshot.has_session && text_equals_ascii(snapshot.title, "Fifth"),
                "transient session absence retains the last snapshot");
    fake_set_state(&fake, 7, "Recovered", 1);
    expect_true(wait_for_snapshot(service, [](const reach_now_playing_snapshot &value)
                                  { return text_equals_ascii(value.title, "Recovered"); }),
                "media recovery cancels pending disappearance");

    auto disappearance_started = std::chrono::steady_clock::now();
    fake_set_state(&fake, 8, nullptr, 0);
    expect_true(wait_for_snapshot(service, [](const reach_now_playing_snapshot &value)
                                  { return !value.has_session; },
                                  nullptr, 5000),
                "confirmed session absence eventually hides Now Playing");
    auto disappearance_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - disappearance_started)
                                .count();
    expect_true(disappearance_ms >= 3800,
                "session disappearance retains the snapshot for four seconds");

    {
        std::lock_guard<std::mutex> lock(fake.mutex);
        fake.blocked_cover_generation = 9;
        fake.allow_blocked_cover = 0;
        fake.failed_cover_generation = 0;
        fake.next_clears_media = 1;
    }
    fake_set_state(&fake, 9, "Ninth", 1);
    expect_true(wait_for_cover_request(&fake, 9), "ninth cover read is blocked");
    expect_true(wait_for_snapshot(service, [](const reach_now_playing_snapshot &value)
                                  { return text_equals_ascii(value.title, "Ninth"); }),
                "ninth core generation publishes before its cover");

    expect_true(reach_now_playing_service_try_action(service, REACH_NOW_PLAYING_ACTION_NEXT),
                "transport action is accepted");
    expect_true(wait_for_snapshot(service, [](const reach_now_playing_snapshot &value)
                                  { return value.transport_pending != 0; }),
                "transport controls publish disabled");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    reach_now_playing_service_snapshot(service, &snapshot);
    expect_true(snapshot.has_session && snapshot.transport_pending,
                "transient empty state neither hides nor unlocks transport");
    fake_set_state(&fake, 11, "Eleventh", 1);
    expect_true(wait_for_snapshot(
                    service,
                    [](const reach_now_playing_snapshot &value)
                    {
                        return text_equals_ascii(value.title, "Eleventh") &&
                               value.transport_pending == 0;
                    },
                    &snapshot, 1000),
                "new core state cancels disappearance and unlocks transport");
    expect_true(snapshot.next_enabled, "transport controls restore from core state");

    {
        std::lock_guard<std::mutex> lock(fake.mutex);
        fake.allow_blocked_cover = 1;
    }
    fake.cv.notify_all();
    reach_now_playing_service_destroy(service);
}

int main(void)
{
    test_progressive_latest_only_publication();
    return failures == 0 ? 0 : 1;
}
