#ifndef REACH_ADAPTERS_WINDOWS_WIFI_PROFILE_H
#define REACH_ADAPTERS_WINDOWS_WIFI_PROFILE_H

#include "reach/core/wifi.h"

#include <stddef.h>
#include <stdint.h>

#define REACH_WIFI_PROFILE_CAPACITY 2048

/* Builds a WLAN profile XML document for the given network. Returns the written length in
   UTF-16 units, or 0 when the request cannot produce a valid profile. */
size_t reach_wifi_build_profile_xml(uint16_t *out_xml, size_t capacity, const uint16_t *ssid,
                                    const uint16_t *key, reach_wifi_security security,
                                    int32_t connect_automatically, int32_t hidden);

/* Escapes XML text into out_text. Returns the written length, or 0 when the text does not fit. */
size_t reach_wifi_escape_xml(uint16_t *out_text, size_t capacity, const uint16_t *text);

#endif
