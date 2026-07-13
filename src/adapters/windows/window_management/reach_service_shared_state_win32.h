#ifndef REACH_SERVICE_SHARED_STATE_WIN32_H
#define REACH_SERVICE_SHARED_STATE_WIN32_H

#include "reach/protocol/reach_service_protocol.h"

enum reach_service_shared_reader_event : uint32_t
{
    REACH_SERVICE_SHARED_EVENT_CONNECTED = 1,
    REACH_SERVICE_SHARED_EVENT_DISCONNECTED = 2,
    REACH_SERVICE_SHARED_EVENT_WINDOWS_CHANGED = 3,
    REACH_SERVICE_SHARED_EVENT_HOTKEYS_CHANGED = 4,
    REACH_SERVICE_SHARED_EVENT_GAME_MODE_CHANGED = 5,
};

typedef void (*reach_service_shared_reader_callback)(void *user,
                                                     reach_service_shared_reader_event event);

reach_result reach_service_shared_writer_start(void);
void reach_service_shared_writer_stop(void);
reach_result reach_service_shared_publish_windows(const reach_service_window_snapshot *windows,
                                                  uint32_t window_count);
reach_result reach_service_shared_publish_game_mode(int32_t active);
reach_result reach_service_shared_append_hotkey(uint32_t key, uint32_t action, uint32_t modifiers);

reach_result reach_service_shared_reader_subscribe(reach_service_shared_reader_callback callback,
                                                   void *user);
void reach_service_shared_reader_unsubscribe(void *user);
int32_t reach_service_shared_reader_connected(void);
reach_result reach_service_shared_copy_windows(reach_service_window_snapshot *windows,
                                               uint32_t max_windows, uint32_t *out_window_count);
reach_result reach_service_shared_copy_hotkeys_since(
    uint64_t last_consumed_event, reach_service_hotkey_record *records, uint32_t max_records,
    uint32_t *out_record_count, int32_t *out_missed, uint64_t *out_first_available,
    uint64_t *out_last_available);
reach_result reach_service_shared_copy_game_mode(int32_t *out_active);

#endif
