#ifndef REACH_PORTS_AUDIO_VOLUME_H
#define REACH_PORTS_AUDIO_VOLUME_H

#include "reach/support/util.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_AUDIO_VOLUME_MAX_SESSIONS 16
#define REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY 128
#define REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY 128
#define REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES 16
#define REACH_AUDIO_VOLUME_DEVICE_ID_CAPACITY 256
#define REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY 128

    typedef struct reach_audio_volume_state
    {
        float level;
        int32_t muted;
    } reach_audio_volume_state;

    typedef struct reach_audio_volume_session
    {
        uint16_t session_instance_id[REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY];
        uint32_t process_id;
        uint64_t icon_id;
        uint16_t label[REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY];
        float level;
        int32_t muted;
        int32_t is_system_sounds;
    } reach_audio_volume_session;

    typedef struct reach_audio_volume_session_list
    {
        reach_audio_volume_session sessions[REACH_AUDIO_VOLUME_MAX_SESSIONS];
        size_t count;
    } reach_audio_volume_session_list;

    typedef struct reach_audio_output_device
    {
        uint16_t device_id[REACH_AUDIO_VOLUME_DEVICE_ID_CAPACITY];
        uint16_t label[REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY];
        uint64_t icon_id;
        int32_t is_default;
    } reach_audio_output_device;

    typedef struct reach_audio_output_device_list
    {
        reach_audio_output_device devices[REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES];
        size_t count;
    } reach_audio_output_device_list;

    typedef struct reach_audio_volume_port
    {
        void *userdata;

        reach_result (*get_state)(void *userdata, reach_audio_volume_state *out_state);

        reach_result (*set_level)(void *userdata, float level);

        reach_result (*set_muted)(void *userdata, int32_t muted);

        reach_result (*list_sessions)(void *userdata, reach_audio_volume_session_list *out_list);

        reach_result (*list_output_devices)(void *userdata,
                                            reach_audio_output_device_list *out_list);

        reach_result (*set_session_level)(void *userdata, const uint16_t *session_instance_id,
                                          float level);

        reach_result (*set_session_muted)(void *userdata, const uint16_t *session_instance_id,
                                          int32_t muted);

        reach_result (*set_default_output_device)(void *userdata, const uint16_t *device_id);

        void (*destroy)(void *userdata);
    } reach_audio_volume_port;

#ifdef __cplusplus
}
#endif

#endif
