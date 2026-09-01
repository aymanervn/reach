#ifndef REACH_CORE_MENU_COMMANDS_H
#define REACH_CORE_MENU_COMMANDS_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/geometry.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_CONTEXT_MENU_MAX_ITEMS 8
#define REACH_MENU_MAX_WINDOWS 16
#define REACH_MENU_TEXT_CAPACITY 260

    typedef enum reach_context_menu_command
    {
        REACH_CONTEXT_MENU_COMMAND_UNPIN = 100,
        REACH_CONTEXT_MENU_COMMAND_CLOSE = 101,
        REACH_CONTEXT_MENU_COMMAND_OPEN_NEW = 102,
        REACH_CONTEXT_MENU_COMMAND_PIN = 103,
        REACH_CONTEXT_MENU_COMMAND_CLOSE_ALL = 104,
        REACH_CONTEXT_MENU_COMMAND_OPEN_AS_ADMIN = 105,
        REACH_CONTEXT_MENU_COMMAND_POWER_LOCK = 200,
        REACH_CONTEXT_MENU_COMMAND_POWER_SLEEP = 201,
        REACH_CONTEXT_MENU_COMMAND_POWER_RESTART = 202,
        REACH_CONTEXT_MENU_COMMAND_POWER_SHUTDOWN = 203,
        REACH_CONTEXT_MENU_COMMAND_POWER_SIGN_OUT = 204
    } reach_context_menu_command;

    typedef struct reach_menu_window
    {
        uintptr_t window;
        uint16_t title[REACH_MENU_TEXT_CAPACITY];
    } reach_menu_window;

    /* Everything a menu needs about what it was opened for, published once by the feature that
       owns the item. A menu never asks that feature a follow-up question, so a command can still
       be executed after the owner's model has moved on. */
    typedef struct reach_menu_request
    {
        size_t target_index;
        uint32_t pin_id;
        uintptr_t window;
        uint16_t path[REACH_MENU_TEXT_CAPACITY];
        uint16_t arguments[REACH_MENU_TEXT_CAPACITY];
        uint16_t app_user_model_id[REACH_MENU_TEXT_CAPACITY];
        uint16_t icon_ref[REACH_MENU_TEXT_CAPACITY];

        uint32_t commands[REACH_CONTEXT_MENU_MAX_ITEMS];
        size_t command_count;

        reach_menu_window windows[REACH_MENU_MAX_WINDOWS];
        size_t window_count;

        reach_rect_f32 anchor_button;
        float bar_edge_y;
        int32_t drop_direction;
        int32_t anchored;
        float pointer_x;
        float pointer_y;
    } reach_menu_request;

    static inline int32_t reach_menu_request_allows(const reach_menu_request *request,
                                                    uint32_t command)
    {
        if (request == NULL)
        {
            return 0;
        }
        for (size_t index = 0; index < request->command_count; ++index)
        {
            if (request->commands[index] == command)
            {
                return 1;
            }
        }
        return 0;
    }

#ifdef __cplusplus
}
#endif

#endif
