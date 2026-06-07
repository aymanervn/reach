#include "elevation_helper_shared_state_win32.h"

#include <windows.h>
#include <sddl.h>

#include <new>

static const size_t REACH_SHARED_MAX_SUBSCRIBERS = 4;

struct reach_shared_reader_subscriber
{
    reach_elevation_helper_shared_reader_callback callback;
    void *user;
};

struct reach_shared_reader
{
    HANDLE thread;
    HANDLE stop_event;
    HANDLE mapping;
    HANDLE update_event;
    const reach_elevation_helper_shared_state *view;
    CRITICAL_SECTION lock;
    int32_t lock_initialized;
    int32_t connected;
    uint64_t last_publish_sequence;
    uint64_t last_window_sequence;
    uint64_t last_hotkey_sequence;
    uint64_t last_game_mode_sequence;
    reach_elevation_helper_shared_state cache;
    reach_shared_reader_subscriber subscribers[REACH_SHARED_MAX_SUBSCRIBERS];
};

struct reach_shared_writer
{
    HANDLE mapping;
    HANDLE update_event;
    reach_elevation_helper_shared_state *view;
    uint32_t generation;
    uint64_t next_hotkey_event;
};

static reach_shared_reader g_reader;
static reach_shared_writer g_writer;

static SECURITY_ATTRIBUTES reach_shared_security(PSECURITY_DESCRIPTOR *sd)
{
    *sd = nullptr;
    SECURITY_ATTRIBUTES attributes = {};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = FALSE;

    const wchar_t *sddl = L"D:P(A;;GA;;;IU)(A;;GA;;;SY)(A;;GA;;;BA)";
    if (ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, sd, nullptr))
    {
        attributes.lpSecurityDescriptor = *sd;
    }

    return attributes;
}

static void reach_shared_reader_lock(void)
{
    if (g_reader.lock_initialized)
    {
        EnterCriticalSection(&g_reader.lock);
    }
}

static void reach_shared_reader_unlock(void)
{
    if (g_reader.lock_initialized)
    {
        LeaveCriticalSection(&g_reader.lock);
    }
}

static void reach_shared_reader_dispatch(reach_elevation_helper_shared_reader_event event)
{
    reach_shared_reader_subscriber subscribers[REACH_SHARED_MAX_SUBSCRIBERS] = {};
    reach_shared_reader_lock();
    for (size_t index = 0; index < REACH_SHARED_MAX_SUBSCRIBERS; ++index)
    {
        subscribers[index] = g_reader.subscribers[index];
    }
    reach_shared_reader_unlock();

    for (size_t index = 0; index < REACH_SHARED_MAX_SUBSCRIBERS; ++index)
    {
        if (subscribers[index].user != nullptr && subscribers[index].callback != nullptr)
        {
            subscribers[index].callback(subscribers[index].user, event);
        }
    }
}

static void reach_shared_reader_close_mapping(void)
{
    if (g_reader.view != nullptr)
    {
        UnmapViewOfFile(g_reader.view);
        g_reader.view = nullptr;
    }
    if (g_reader.mapping != nullptr)
    {
        CloseHandle(g_reader.mapping);
        g_reader.mapping = nullptr;
    }
    if (g_reader.update_event != nullptr)
    {
        CloseHandle(g_reader.update_event);
        g_reader.update_event = nullptr;
    }
}

static int32_t reach_shared_reader_open_mapping(void)
{
    if (g_reader.view != nullptr && g_reader.update_event != nullptr)
    {
        return 1;
    }

    reach_shared_reader_close_mapping();
    g_reader.mapping =
        OpenFileMappingW(FILE_MAP_READ, FALSE, REACH_ELEVATION_HELPER_STATE_NAME);
    if (g_reader.mapping == nullptr)
    {
        return 0;
    }

    g_reader.view = static_cast<const reach_elevation_helper_shared_state *>(
        MapViewOfFile(g_reader.mapping, FILE_MAP_READ, 0, 0,
                      sizeof(reach_elevation_helper_shared_state)));
    if (g_reader.view == nullptr)
    {
        reach_shared_reader_close_mapping();
        return 0;
    }

    g_reader.update_event =
        OpenEventW(SYNCHRONIZE, FALSE, REACH_ELEVATION_HELPER_UPDATE_EVENT_NAME);
    if (g_reader.update_event == nullptr)
    {
        reach_shared_reader_close_mapping();
        return 0;
    }

    return 1;
}

static int32_t reach_shared_reader_copy_from_view(reach_elevation_helper_shared_state *out_state)
{
    if (g_reader.view == nullptr || out_state == nullptr)
    {
        return 0;
    }

    for (int attempt = 0; attempt < 6; ++attempt)
    {
        uint64_t before = g_reader.view->publish_sequence;
        if ((before & 1u) != 0)
        {
            Sleep(0);
            continue;
        }

        reach_elevation_helper_shared_state snapshot = *g_reader.view;
        uint64_t after = g_reader.view->publish_sequence;
        if (before == after && (after & 1u) == 0 &&
            snapshot.version == reach_elevation_helper_protocol_version() &&
            snapshot.layout_size == sizeof(reach_elevation_helper_shared_state))
        {
            *out_state = snapshot;
            return 1;
        }
    }

    return 0;
}

static void reach_shared_reader_set_disconnected(void)
{
    int32_t was_connected = 0;
    reach_shared_reader_lock();
    was_connected = g_reader.connected;
    g_reader.connected = 0;
    g_reader.cache = {};
    g_reader.last_publish_sequence = 0;
    g_reader.last_window_sequence = 0;
    g_reader.last_hotkey_sequence = 0;
    g_reader.last_game_mode_sequence = 0;
    reach_shared_reader_unlock();

    reach_shared_reader_close_mapping();
    if (was_connected)
    {
        reach_shared_reader_dispatch(REACH_ELEVATION_HELPER_SHARED_EVENT_DISCONNECTED);
    }
}

static void reach_shared_reader_accept_state(const reach_elevation_helper_shared_state *state)
{
    if (state == nullptr)
    {
        return;
    }

    int32_t became_connected = 0;
    int32_t windows_changed = 0;
    int32_t hotkeys_changed = 0;
    int32_t game_mode_changed = 0;

    reach_shared_reader_lock();
    if (!g_reader.connected)
    {
        became_connected = 1;
    }
    g_reader.connected = 1;
    windows_changed = state->window_sequence != g_reader.last_window_sequence;
    hotkeys_changed = state->hotkey_sequence != g_reader.last_hotkey_sequence;
    game_mode_changed = state->game_mode_sequence != g_reader.last_game_mode_sequence;
    g_reader.cache = *state;
    g_reader.last_publish_sequence = state->publish_sequence;
    g_reader.last_window_sequence = state->window_sequence;
    g_reader.last_hotkey_sequence = state->hotkey_sequence;
    g_reader.last_game_mode_sequence = state->game_mode_sequence;
    reach_shared_reader_unlock();

    if (became_connected)
    {
        reach_shared_reader_dispatch(REACH_ELEVATION_HELPER_SHARED_EVENT_CONNECTED);
    }
    if (windows_changed)
    {
        reach_shared_reader_dispatch(REACH_ELEVATION_HELPER_SHARED_EVENT_WINDOWS_CHANGED);
    }
    if (hotkeys_changed)
    {
        reach_shared_reader_dispatch(REACH_ELEVATION_HELPER_SHARED_EVENT_HOTKEYS_CHANGED);
    }
    if (game_mode_changed)
    {
        reach_shared_reader_dispatch(REACH_ELEVATION_HELPER_SHARED_EVENT_GAME_MODE_CHANGED);
    }
}

static DWORD WINAPI reach_shared_reader_thread(void *param)
{
    (void)param;

    for (;;)
    {
        if (WaitForSingleObject(g_reader.stop_event, 0) == WAIT_OBJECT_0)
        {
            return 0;
        }

        if (!reach_shared_reader_open_mapping())
        {
            reach_shared_reader_set_disconnected();
            WaitForSingleObject(g_reader.stop_event, 500);
            continue;
        }

        reach_elevation_helper_shared_state state = {};
        if (reach_shared_reader_copy_from_view(&state))
        {
            reach_shared_reader_accept_state(&state);
        }

        HANDLE handles[2] = {g_reader.stop_event, g_reader.update_event};
        DWORD wait = WaitForMultipleObjects(2, handles, FALSE, 1000);
        if (wait == WAIT_OBJECT_0)
        {
            return 0;
        }
        if (wait != WAIT_OBJECT_0 + 1 && wait != WAIT_TIMEOUT)
        {
            reach_shared_reader_set_disconnected();
        }
    }
}

static reach_result reach_shared_reader_ensure_started(void)
{
    if (!g_reader.lock_initialized)
    {
        InitializeCriticalSection(&g_reader.lock);
        g_reader.lock_initialized = 1;
    }
    if (g_reader.stop_event == nullptr)
    {
        g_reader.stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    }
    if (g_reader.stop_event == nullptr)
    {
        return REACH_ERROR;
    }
    ResetEvent(g_reader.stop_event);
    if (g_reader.thread == nullptr)
    {
        g_reader.thread = CreateThread(nullptr, 0, reach_shared_reader_thread, nullptr, 0, nullptr);
    }
    return g_reader.thread != nullptr ? REACH_OK : REACH_ERROR;
}

static int32_t reach_shared_reader_has_subscribers(void)
{
    for (size_t index = 0; index < REACH_SHARED_MAX_SUBSCRIBERS; ++index)
    {
        if (g_reader.subscribers[index].user != nullptr)
        {
            return 1;
        }
    }
    return 0;
}

static void reach_shared_reader_stop_if_unused(void)
{
    int32_t has_subscribers = 0;
    reach_shared_reader_lock();
    has_subscribers = reach_shared_reader_has_subscribers();
    reach_shared_reader_unlock();
    if (has_subscribers)
    {
        return;
    }

    if (g_reader.stop_event != nullptr)
    {
        SetEvent(g_reader.stop_event);
    }
    if (g_reader.thread != nullptr)
    {
        WaitForSingleObject(g_reader.thread, 1000);
        CloseHandle(g_reader.thread);
        g_reader.thread = nullptr;
    }
    if (g_reader.stop_event != nullptr)
    {
        CloseHandle(g_reader.stop_event);
        g_reader.stop_event = nullptr;
    }
    reach_shared_reader_close_mapping();

    reach_shared_reader_lock();
    g_reader.connected = 0;
    g_reader.cache = {};
    g_reader.last_publish_sequence = 0;
    g_reader.last_window_sequence = 0;
    g_reader.last_hotkey_sequence = 0;
    g_reader.last_game_mode_sequence = 0;
    reach_shared_reader_unlock();
}

reach_result reach_elevation_helper_shared_reader_subscribe(
    reach_elevation_helper_shared_reader_callback callback, void *user)
{
    if (callback == nullptr || user == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (reach_shared_reader_ensure_started() != REACH_OK)
    {
        return REACH_ERROR;
    }

    reach_shared_reader_lock();
    size_t slot = REACH_SHARED_MAX_SUBSCRIBERS;
    for (size_t index = 0; index < REACH_SHARED_MAX_SUBSCRIBERS; ++index)
    {
        if (g_reader.subscribers[index].user == user)
        {
            slot = index;
            break;
        }
        if (slot == REACH_SHARED_MAX_SUBSCRIBERS && g_reader.subscribers[index].user == nullptr)
        {
            slot = index;
        }
    }
    if (slot == REACH_SHARED_MAX_SUBSCRIBERS)
    {
        reach_shared_reader_unlock();
        return REACH_ERROR;
    }
    g_reader.subscribers[slot].callback = callback;
    g_reader.subscribers[slot].user = user;
    reach_shared_reader_unlock();
    return REACH_OK;
}

void reach_elevation_helper_shared_reader_unsubscribe(void *user)
{
    if (user == nullptr)
    {
        return;
    }

    reach_shared_reader_lock();
    for (size_t index = 0; index < REACH_SHARED_MAX_SUBSCRIBERS; ++index)
    {
        if (g_reader.subscribers[index].user == user)
        {
            g_reader.subscribers[index] = {};
        }
    }
    reach_shared_reader_unlock();
    reach_shared_reader_stop_if_unused();
}

int32_t reach_elevation_helper_shared_reader_connected(void)
{
    reach_shared_reader_lock();
    int32_t connected = g_reader.connected;
    reach_shared_reader_unlock();
    return connected;
}

reach_result reach_elevation_helper_shared_copy_windows(
    reach_elevation_helper_window_snapshot *windows, uint32_t max_windows,
    uint32_t *out_window_count)
{
    if (out_window_count == nullptr || (max_windows > 0 && windows == nullptr))
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_shared_reader_lock();
    uint32_t count = g_reader.cache.window_count;
    if (count > REACH_ELEVATION_HELPER_MAX_WINDOWS)
    {
        count = REACH_ELEVATION_HELPER_MAX_WINDOWS;
    }
    if (count > max_windows)
    {
        count = max_windows;
    }
    for (uint32_t index = 0; index < count; ++index)
    {
        windows[index] = g_reader.cache.windows[index];
    }
    reach_shared_reader_unlock();

    *out_window_count = count;
    return REACH_OK;
}

reach_result reach_elevation_helper_shared_copy_hotkeys_since(
    uint64_t last_consumed_event, reach_elevation_helper_hotkey_record *records,
    uint32_t max_records, uint32_t *out_record_count, int32_t *out_missed,
    uint64_t *out_first_available, uint64_t *out_last_available)
{
    if (out_record_count == nullptr || out_missed == nullptr ||
        (max_records > 0 && records == nullptr))
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_shared_reader_lock();
    uint32_t queue_count = g_reader.cache.hotkey_queue_count;
    if (queue_count > REACH_ELEVATION_HELPER_HOTKEY_QUEUE_CAPACITY)
    {
        queue_count = REACH_ELEVATION_HELPER_HOTKEY_QUEUE_CAPACITY;
    }
    uint64_t first = g_reader.cache.first_hotkey_event_number;
    uint64_t last = g_reader.cache.last_hotkey_event_number;
    int32_t missed = queue_count > 0 && last_consumed_event != 0 &&
                     last_consumed_event + 1 < first;
    uint64_t next = missed ? first : last_consumed_event + 1;
    uint32_t out_count = 0;

    for (uint32_t offset = 0; offset < queue_count && out_count < max_records; ++offset)
    {
        uint32_t index = (g_reader.cache.hotkey_queue_start + offset) %
                         REACH_ELEVATION_HELPER_HOTKEY_QUEUE_CAPACITY;
        const reach_elevation_helper_hotkey_record &record = g_reader.cache.hotkeys[index];
        if (record.event_number >= next)
        {
            records[out_count++] = record;
        }
    }
    reach_shared_reader_unlock();

    *out_record_count = out_count;
    *out_missed = missed;
    if (out_first_available != nullptr)
    {
        *out_first_available = first;
    }
    if (out_last_available != nullptr)
    {
        *out_last_available = last;
    }
    return REACH_OK;
}

reach_result reach_elevation_helper_shared_copy_game_mode(int32_t *out_active)
{
    if (out_active == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_shared_reader_lock();
    *out_active = g_reader.cache.game_mode_active ? 1 : 0;
    reach_shared_reader_unlock();
    return REACH_OK;
}

reach_result reach_elevation_helper_shared_writer_start(void)
{
    if (g_writer.view != nullptr)
    {
        return REACH_OK;
    }

    PSECURITY_DESCRIPTOR descriptor = nullptr;
    SECURITY_ATTRIBUTES security = reach_shared_security(&descriptor);
    g_writer.mapping = CreateFileMappingW(
        INVALID_HANDLE_VALUE, security.lpSecurityDescriptor != nullptr ? &security : nullptr,
        PAGE_READWRITE, 0, sizeof(reach_elevation_helper_shared_state),
        REACH_ELEVATION_HELPER_STATE_NAME);
    if (descriptor != nullptr)
    {
        LocalFree(descriptor);
    }
    if (g_writer.mapping == nullptr)
    {
        return REACH_ERROR;
    }

    g_writer.view = static_cast<reach_elevation_helper_shared_state *>(
        MapViewOfFile(g_writer.mapping, FILE_MAP_WRITE, 0, 0,
                      sizeof(reach_elevation_helper_shared_state)));
    if (g_writer.view == nullptr)
    {
        reach_elevation_helper_shared_writer_stop();
        return REACH_ERROR;
    }

    descriptor = nullptr;
    security = reach_shared_security(&descriptor);
    g_writer.update_event =
        CreateEventW(security.lpSecurityDescriptor != nullptr ? &security : nullptr, FALSE, FALSE,
                     REACH_ELEVATION_HELPER_UPDATE_EVENT_NAME);
    if (descriptor != nullptr)
    {
        LocalFree(descriptor);
    }
    if (g_writer.update_event == nullptr)
    {
        reach_elevation_helper_shared_writer_stop();
        return REACH_ERROR;
    }

    g_writer.generation = GetTickCount();
    g_writer.next_hotkey_event = 1;
    *g_writer.view = {};
    g_writer.view->version = reach_elevation_helper_protocol_version();
    g_writer.view->layout_size = sizeof(reach_elevation_helper_shared_state);
    g_writer.view->writer_pid = GetCurrentProcessId();
    g_writer.view->generation = g_writer.generation;
    g_writer.view->publish_sequence = 2;
    SetEvent(g_writer.update_event);
    return REACH_OK;
}

void reach_elevation_helper_shared_writer_stop(void)
{
    if (g_writer.view != nullptr)
    {
        UnmapViewOfFile(g_writer.view);
        g_writer.view = nullptr;
    }
    if (g_writer.mapping != nullptr)
    {
        CloseHandle(g_writer.mapping);
        g_writer.mapping = nullptr;
    }
    if (g_writer.update_event != nullptr)
    {
        CloseHandle(g_writer.update_event);
        g_writer.update_event = nullptr;
    }
}

static void reach_shared_writer_begin_publish(void)
{
    uint64_t sequence = g_writer.view->publish_sequence;
    if ((sequence & 1u) == 0)
    {
        g_writer.view->publish_sequence = sequence + 1;
    }
}

static void reach_shared_writer_end_publish(void)
{
    uint64_t sequence = g_writer.view->publish_sequence;
    g_writer.view->publish_sequence = (sequence & 1u) != 0 ? sequence + 1 : sequence + 2;
    if (g_writer.update_event != nullptr)
    {
        SetEvent(g_writer.update_event);
    }
}

reach_result reach_elevation_helper_shared_publish_windows(
    const reach_elevation_helper_window_snapshot *windows, uint32_t window_count)
{
    if (g_writer.view == nullptr)
    {
        return REACH_ERROR;
    }
    if (window_count > 0 && windows == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (window_count > REACH_ELEVATION_HELPER_MAX_WINDOWS)
    {
        window_count = REACH_ELEVATION_HELPER_MAX_WINDOWS;
    }

    reach_shared_writer_begin_publish();
    g_writer.view->version = reach_elevation_helper_protocol_version();
    g_writer.view->layout_size = sizeof(reach_elevation_helper_shared_state);
    g_writer.view->writer_pid = GetCurrentProcessId();
    g_writer.view->generation = g_writer.generation;
    g_writer.view->window_count = window_count;
    for (uint32_t index = 0; index < window_count; ++index)
    {
        g_writer.view->windows[index] = windows[index];
    }
    for (uint32_t index = window_count; index < REACH_ELEVATION_HELPER_MAX_WINDOWS; ++index)
    {
        g_writer.view->windows[index] = {};
    }
    ++g_writer.view->window_sequence;
    reach_shared_writer_end_publish();
    return REACH_OK;
}

reach_result reach_elevation_helper_shared_publish_game_mode(int32_t active)
{
    if (g_writer.view == nullptr)
    {
        return REACH_ERROR;
    }

    active = active ? 1 : 0;
    if (g_writer.view->game_mode_active == active)
    {
        return REACH_OK;
    }

    reach_shared_writer_begin_publish();
    g_writer.view->version = reach_elevation_helper_protocol_version();
    g_writer.view->layout_size = sizeof(reach_elevation_helper_shared_state);
    g_writer.view->writer_pid = GetCurrentProcessId();
    g_writer.view->generation = g_writer.generation;
    g_writer.view->game_mode_active = active;
    ++g_writer.view->game_mode_sequence;
    reach_shared_writer_end_publish();
    return REACH_OK;
}

reach_result reach_elevation_helper_shared_append_hotkey(uint32_t key, uint32_t action,
                                                         uint32_t modifiers)
{
    if (g_writer.view == nullptr)
    {
        return REACH_ERROR;
    }

    reach_shared_writer_begin_publish();
    uint32_t count = g_writer.view->hotkey_queue_count;
    if (count > REACH_ELEVATION_HELPER_HOTKEY_QUEUE_CAPACITY)
    {
        count = REACH_ELEVATION_HELPER_HOTKEY_QUEUE_CAPACITY;
    }
    uint32_t index = 0;
    if (count < REACH_ELEVATION_HELPER_HOTKEY_QUEUE_CAPACITY)
    {
        index = (g_writer.view->hotkey_queue_start + count) %
                REACH_ELEVATION_HELPER_HOTKEY_QUEUE_CAPACITY;
        g_writer.view->hotkey_queue_count = count + 1;
    }
    else
    {
        index = g_writer.view->hotkey_queue_start;
        g_writer.view->hotkey_queue_start =
            (g_writer.view->hotkey_queue_start + 1) %
            REACH_ELEVATION_HELPER_HOTKEY_QUEUE_CAPACITY;
    }

    reach_elevation_helper_hotkey_record record = {};
    record.event_number = g_writer.next_hotkey_event++;
    record.tick_ms = GetTickCount64();
    record.key = key;
    record.action = action;
    record.modifiers = modifiers;
    g_writer.view->hotkeys[index] = record;

    if (g_writer.view->hotkey_queue_count == 1)
    {
        g_writer.view->first_hotkey_event_number = record.event_number;
    }
    else
    {
        uint32_t first_index = g_writer.view->hotkey_queue_start;
        g_writer.view->first_hotkey_event_number =
            g_writer.view->hotkeys[first_index].event_number;
    }
    g_writer.view->last_hotkey_event_number = record.event_number;
    ++g_writer.view->hotkey_sequence;
    reach_shared_writer_end_publish();
    return REACH_OK;
}
