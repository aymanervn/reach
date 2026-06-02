#include "reach/core/app_controller.h"

reach_result reach_app_controller_init(reach_app_controller *controller)
{
    REACH_ASSERT(controller != 0);
    if (controller == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_ui_state_init(&controller->ui);
    reach_render_command_buffer_clear(&controller->render_commands);
    return REACH_OK;
}

reach_result reach_app_controller_handle_event(reach_app_controller *controller,
                                               const reach_ui_event *event,
                                               reach_ui_intent *out_intent)
{
    REACH_ASSERT(controller != 0);
    REACH_ASSERT(event != 0);
    if (controller == 0 || event == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    return reach_ui_handle_event(&controller->ui, event, out_intent);
}

reach_result reach_app_controller_tick(reach_app_controller *controller,
                                       const reach_app_controller_tick_desc *tick)
{
    REACH_ASSERT(controller != 0);
    REACH_ASSERT(tick != 0);
    if (controller == 0 || tick == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result =
        reach_ui_layout_compute(&controller->ui, &tick->layout_input, &controller->layout);
    return result == REACH_OK ? reach_ui_build_render_commands(&controller->ui, &controller->layout,
                                                               &controller->render_commands)
                              : result;
}
