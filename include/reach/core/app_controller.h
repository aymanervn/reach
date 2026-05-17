#ifndef REACH_CORE_APP_CONTROLLER_H
#define REACH_CORE_APP_CONTROLLER_H

#include "reach/core/render_commands.h"
#include "reach/core/ui_events.h"
#include "reach/core/ui_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_app_controller {
    reach_ui_state ui;
    reach_ui_layout layout;
    reach_render_command_buffer render_commands;
} reach_app_controller;

typedef struct reach_app_controller_tick_desc {
    reach_ui_layout_input layout_input;
    double delta_seconds;
} reach_app_controller_tick_desc;

reach_result reach_app_controller_init(reach_app_controller *controller);
reach_result reach_app_controller_handle_event(reach_app_controller *controller, const reach_ui_event *event, reach_ui_intent *out_intent);
reach_result reach_app_controller_tick(reach_app_controller *controller, const reach_app_controller_tick_desc *tick);

#ifdef __cplusplus
}
#endif

#endif
