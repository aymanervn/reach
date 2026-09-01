#ifndef REACH_CORE_WIFI_H
#define REACH_CORE_WIFI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_WIFI_MAX_NETWORKS 64
#define REACH_WIFI_SSID_CAPACITY 64
#define REACH_WIFI_KEY_CAPACITY 72
#define REACH_WIFI_KEY_MINIMUM_LENGTH 8
#define REACH_WIFI_KEY_MAXIMUM_LENGTH 63

    typedef enum reach_wifi_radio_state
    {
        REACH_WIFI_RADIO_UNAVAILABLE = 0,
        REACH_WIFI_RADIO_OFF,
        REACH_WIFI_RADIO_ON
    } reach_wifi_radio_state;

    typedef enum reach_wifi_security
    {
        REACH_WIFI_SECURITY_OPEN = 0,
        REACH_WIFI_SECURITY_WEP,
        REACH_WIFI_SECURITY_WPA2_PERSONAL,
        REACH_WIFI_SECURITY_WPA3_PERSONAL,
        REACH_WIFI_SECURITY_ENTERPRISE,
        REACH_WIFI_SECURITY_UNKNOWN
    } reach_wifi_security;

    typedef enum reach_wifi_scan_result
    {
        REACH_WIFI_SCAN_RESULT_NONE = 0,
        REACH_WIFI_SCAN_RESULT_SUCCEEDED,
        REACH_WIFI_SCAN_RESULT_FAILED,
        REACH_WIFI_SCAN_RESULT_TIMED_OUT
    } reach_wifi_scan_result;

    typedef enum reach_wifi_connect_result
    {
        REACH_WIFI_CONNECT_RESULT_NONE = 0,
        REACH_WIFI_CONNECT_RESULT_SUCCEEDED,
        REACH_WIFI_CONNECT_RESULT_INVALID_KEY,
        REACH_WIFI_CONNECT_RESULT_NOT_FOUND,
        REACH_WIFI_CONNECT_RESULT_TIMED_OUT,
        REACH_WIFI_CONNECT_RESULT_FAILED
    } reach_wifi_connect_result;

    typedef struct reach_wifi_network
    {
        uint16_t ssid[REACH_WIFI_SSID_CAPACITY];
        reach_wifi_security security;
        int32_t signal_strength;
        int32_t connected;
        int32_t saved;
        int32_t in_range;
        int32_t connect_automatically;
        int32_t hidden;
    } reach_wifi_network;

    typedef struct reach_wifi_network_list
    {
        reach_wifi_network networks[REACH_WIFI_MAX_NETWORKS];
        size_t count;
    } reach_wifi_network_list;

    typedef struct reach_wifi_connect_request
    {
        uint16_t ssid[REACH_WIFI_SSID_CAPACITY];
        uint16_t key[REACH_WIFI_KEY_CAPACITY];
        reach_wifi_security security;
        int32_t connect_automatically;
        int32_t hidden;
    } reach_wifi_connect_request;

    int32_t reach_wifi_security_needs_key(reach_wifi_security security);
    int32_t reach_wifi_security_is_supported(reach_wifi_security security);
    const uint16_t *reach_wifi_security_label(reach_wifi_security security);
    int32_t reach_wifi_key_length_valid(reach_wifi_security security, size_t length);
    int32_t reach_wifi_ssid_equal(const uint16_t *left, const uint16_t *right);

    void reach_wifi_network_list_normalize(reach_wifi_network_list *list);
    size_t reach_wifi_network_list_find(const reach_wifi_network_list *list, const uint16_t *ssid);

#ifdef __cplusplus
}
#endif
#endif
