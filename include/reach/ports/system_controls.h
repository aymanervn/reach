#ifndef REACH_PORTS_SYSTEM_CONTROLS_H
#define REACH_PORTS_SYSTEM_CONTROLS_H

#include "reach/support/util.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_SYSTEM_NETWORK_LABEL_CAPACITY 128

#define REACH_SYSTEM_CONTROLS_CHANGE_NETWORK (1u << 0)
#define REACH_SYSTEM_CONTROLS_CHANGE_BLUETOOTH (1u << 1)
#define REACH_SYSTEM_CONTROLS_CHANGE_POWER (1u << 2)
#define REACH_SYSTEM_CONTROLS_CHANGE_BRIGHTNESS (1u << 3)

    typedef void (*reach_system_controls_change_callback)(void *user, uint32_t change_flags);

    typedef enum reach_network_kind
    {
        REACH_NETWORK_KIND_NONE = 0,
        REACH_NETWORK_KIND_ETHERNET,
        REACH_NETWORK_KIND_WIFI
    } reach_network_kind;

    typedef struct reach_network_state
    {
        reach_network_kind kind;
        int32_t connected;
        uint16_t label[REACH_SYSTEM_NETWORK_LABEL_CAPACITY];
        int32_t signal_strength;
    } reach_network_state;

    typedef struct reach_bluetooth_state
    {
        int32_t available;
        int32_t enabled;
    } reach_bluetooth_state;

    typedef struct reach_power_state
    {
        int32_t has_battery;
        int32_t battery_percent;
        int32_t battery_saver_on;
    } reach_power_state;

    typedef struct reach_brightness_state
    {
        int32_t available;
        float level;
    } reach_brightness_state;

    typedef struct reach_system_controls_port
    {
        void *userdata;

        reach_result (*get_network_state)(void *userdata, reach_network_state *out_state);

        reach_result (*get_bluetooth_state)(void *userdata, reach_bluetooth_state *out_state);

        reach_result (*set_bluetooth_enabled)(void *userdata, int32_t enabled);

        reach_result (*request_bluetooth_enabled)(void *userdata, int32_t enabled);

        reach_result (*get_power_state)(void *userdata, reach_power_state *out_state);

        reach_result (*set_battery_saver_enabled)(void *userdata, int32_t enabled);

        reach_result (*get_brightness_state)(void *userdata, reach_brightness_state *out_state);

        reach_result (*set_brightness_level)(void *userdata, float level);

        reach_result (*open_project_menu)(void *userdata);

        reach_result (*open_system_quick_settings)(void *userdata);

        reach_result (*start_watching)(void *userdata,
                                       reach_system_controls_change_callback callback,
                                       void *callback_user);

        void (*stop_watching)(void *userdata);

        void (*destroy)(void *userdata);
    } reach_system_controls_port;

#ifdef __cplusplus
}
#endif

#endif
