#ifndef REACH_CORE_BLUETOOTH_H
#define REACH_CORE_BLUETOOTH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_BLUETOOTH_MAX_DEVICES 64
#define REACH_BLUETOOTH_DEVICE_ID_CAPACITY 260
#define REACH_BLUETOOTH_NAME_CAPACITY 128
#define REACH_BLUETOOTH_ICON_PATH_CAPACITY 260
#define REACH_BLUETOOTH_PIN_CAPACITY 32

    typedef enum reach_bluetooth_device_kind
    {
        REACH_BLUETOOTH_DEVICE_UNKNOWN = 0,
        REACH_BLUETOOTH_DEVICE_AUDIO,
        REACH_BLUETOOTH_DEVICE_KEYBOARD,
        REACH_BLUETOOTH_DEVICE_MOUSE,
        REACH_BLUETOOTH_DEVICE_PHONE,
        REACH_BLUETOOTH_DEVICE_COMPUTER,
        REACH_BLUETOOTH_DEVICE_WEARABLE,
        REACH_BLUETOOTH_DEVICE_PRINTER
    } reach_bluetooth_device_kind;

    typedef enum reach_bluetooth_pair_result
    {
        REACH_BLUETOOTH_PAIR_RESULT_NONE = 0,
        REACH_BLUETOOTH_PAIR_RESULT_SUCCEEDED,
        REACH_BLUETOOTH_PAIR_RESULT_REJECTED,
        REACH_BLUETOOTH_PAIR_RESULT_TIMED_OUT,
        REACH_BLUETOOTH_PAIR_RESULT_FAILED
    } reach_bluetooth_pair_result;

    typedef struct reach_bluetooth_device
    {
        uint16_t id[REACH_BLUETOOTH_DEVICE_ID_CAPACITY];
        uint16_t name[REACH_BLUETOOTH_NAME_CAPACITY];
        uint16_t icon_path[REACH_BLUETOOTH_ICON_PATH_CAPACITY];
        reach_bluetooth_device_kind kind;
        int32_t paired;
        int32_t connected;
        int32_t can_pair;
    } reach_bluetooth_device;

    typedef struct reach_bluetooth_device_list
    {
        reach_bluetooth_device devices[REACH_BLUETOOTH_MAX_DEVICES];
        size_t count;
    } reach_bluetooth_device_list;

    typedef struct reach_bluetooth_pairing_request
    {
        uint16_t device_id[REACH_BLUETOOTH_DEVICE_ID_CAPACITY];
        uint16_t pin[REACH_BLUETOOTH_PIN_CAPACITY];
        int32_t active;
        int32_t needs_confirmation;
    } reach_bluetooth_pairing_request;

    const uint16_t *reach_bluetooth_device_kind_label(reach_bluetooth_device_kind kind);
    int32_t reach_bluetooth_device_id_equal(const uint16_t *left, const uint16_t *right);

    void reach_bluetooth_device_list_normalize(reach_bluetooth_device_list *list);
    size_t reach_bluetooth_device_list_find(const reach_bluetooth_device_list *list,
                                            const uint16_t *device_id);
    size_t reach_bluetooth_paired_count(const reach_bluetooth_device_list *list);

#ifdef __cplusplus
}
#endif
#endif
