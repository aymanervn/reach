#ifndef REACH_ELEVATION_HELPER_PROTOCOL_H
#define REACH_ELEVATION_HELPER_PROTOCOL_H

#include "../windows_adapters_internal.h"

#include <stdint.h>
static const wchar_t REACH_ELEVATION_HELPER_PIPE_NAME[] = L"\\\\.\\pipe\\ReachElevationHelper";
static const wchar_t REACH_ELEVATION_HELPER_STATE_NAME[] = L"Local\\ReachElevationHelperState";
static const wchar_t REACH_ELEVATION_HELPER_UPDATE_EVENT_NAME[] =
    L"Local\\ReachElevationHelperUpdated";

enum reach_elevation_helper_command : uint32_t
{
    REACH_ELEVATION_HELPER_COMMAND_PING = 0,
    REACH_ELEVATION_HELPER_COMMAND_ACTIVATE = 1,
    REACH_ELEVATION_HELPER_COMMAND_MINIMIZE = 2,
    REACH_ELEVATION_HELPER_COMMAND_SNAP = 3,
    REACH_ELEVATION_HELPER_COMMAND_CLOSE = 4,
    REACH_ELEVATION_HELPER_COMMAND_RESTORE = 8,
};

enum reach_elevation_helper_window_kind : uint32_t
{
    REACH_ELEVATION_HELPER_WINDOW_APP = 1,
    REACH_ELEVATION_HELPER_WINDOW_MINIMIZED_APP = 2,
    REACH_ELEVATION_HELPER_WINDOW_DIALOG = 3,
    REACH_ELEVATION_HELPER_WINDOW_HELPER = 4,
    REACH_ELEVATION_HELPER_WINDOW_SHELL = 5,
    REACH_ELEVATION_HELPER_WINDOW_SYSTEM = 6,
    REACH_ELEVATION_HELPER_WINDOW_UNKNOWN = 7,
};

enum reach_elevation_helper_action_result_code : uint32_t
{
    REACH_ELEVATION_HELPER_ACTION_SUCCEEDED = 1,
    REACH_ELEVATION_HELPER_ACTION_FAILED_API_CALL = 2,
    REACH_ELEVATION_HELPER_ACTION_DID_NOT_CHANGE_STATE = 3,
    REACH_ELEVATION_HELPER_ACTION_STALE_WINDOW = 4,
    REACH_ELEVATION_HELPER_ACTION_ACCESS_DENIED = 5,
    REACH_ELEVATION_HELPER_ACTION_FOREGROUND_DENIED = 6,
    REACH_ELEVATION_HELPER_ACTION_TIMED_OUT = 7,
};

static const uint32_t REACH_ELEVATION_HELPER_MAX_WINDOWS = 96;
static const uint32_t REACH_ELEVATION_HELPER_HOTKEY_QUEUE_CAPACITY = 256;

struct reach_elevation_helper_request
{
    uint32_t version;
    uint32_t command;
    uint64_t window;
    uint32_t process_id;
    uint32_t thread_id;
    int32_t split_mode;
    wchar_t class_name[128];
};

struct reach_elevation_helper_window_snapshot
{
    uint64_t window;
    uint32_t process_id;
    uint32_t thread_id;
    uint32_t kind;
    uint32_t include_in_switcher;
    wchar_t title[260];
    wchar_t class_name[128];
    wchar_t process_path[260];
    wchar_t app_user_model_id[260];
    wchar_t integrity[32];
    int32_t visible;
    int32_t iconic;
    int32_t cloaked;
    int32_t focused;
    int32_t enabled;
    int32_t maximized;
    wchar_t classification_reason[160];
};

enum reach_elevation_helper_hotkey_key : uint32_t
{
    REACH_ELEVATION_HELPER_HOTKEY_ALT = 1,
    REACH_ELEVATION_HELPER_HOTKEY_SHIFT = 2,
    REACH_ELEVATION_HELPER_HOTKEY_TAB = 3,
    REACH_ELEVATION_HELPER_HOTKEY_ESCAPE = 4,
    REACH_ELEVATION_HELPER_HOTKEY_LEFT_WIN = 5,
    REACH_ELEVATION_HELPER_HOTKEY_RIGHT_WIN = 6,
};

enum reach_elevation_helper_hotkey_action : uint32_t
{
    REACH_ELEVATION_HELPER_HOTKEY_RELEASED = 0,
    REACH_ELEVATION_HELPER_HOTKEY_PRESSED = 1,
};

enum reach_elevation_helper_hotkey_modifier : uint32_t
{
    REACH_ELEVATION_HELPER_MODIFIER_ALT = 1u << 0,
    REACH_ELEVATION_HELPER_MODIFIER_SHIFT = 1u << 1,
    REACH_ELEVATION_HELPER_MODIFIER_LEFT_WIN = 1u << 2,
    REACH_ELEVATION_HELPER_MODIFIER_RIGHT_WIN = 1u << 3,
    REACH_ELEVATION_HELPER_MODIFIER_CHORD = 1u << 4,
};

struct reach_elevation_helper_hotkey_record
{
    uint64_t event_number;
    uint64_t tick_ms;
    uint32_t key;
    uint32_t action;
    uint32_t modifiers;
    uint32_t reserved;
};

struct reach_elevation_helper_shared_state
{
    uint32_t version;
    uint32_t layout_size;
    uint32_t writer_pid;
    uint32_t generation;
    volatile uint64_t publish_sequence;
    uint64_t window_sequence;
    uint64_t hotkey_sequence;
    uint64_t game_mode_sequence;
    uint32_t window_count;
    uint32_t hotkey_queue_start;
    uint32_t hotkey_queue_count;
    int32_t game_mode_active;
    uint64_t first_hotkey_event_number;
    uint64_t last_hotkey_event_number;
    reach_elevation_helper_window_snapshot windows[REACH_ELEVATION_HELPER_MAX_WINDOWS];
    reach_elevation_helper_hotkey_record
        hotkeys[REACH_ELEVATION_HELPER_HOTKEY_QUEUE_CAPACITY];
};

struct reach_elevation_helper_response
{
    uint32_t version;
    int32_t result;
    uint32_t action_result;
    uint32_t window_count;
    reach_elevation_helper_window_snapshot windows[REACH_ELEVATION_HELPER_MAX_WINDOWS];
};

uint32_t reach_elevation_helper_protocol_version(void);
int32_t reach_elevation_helper_command_valid(uint32_t command);
int32_t reach_elevation_helper_request_valid(const reach_elevation_helper_request *request);

#endif
