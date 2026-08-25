#include "reach/core/wifi.h"

static int reach_wifi_text_compare(const uint16_t *left, const uint16_t *right)
{
    size_t index = 0;
    while (left[index] != 0 && left[index] == right[index])
    {
        ++index;
    }
    if (left[index] == right[index])
    {
        return 0;
    }
    return left[index] < right[index] ? -1 : 1;
}

int32_t reach_wifi_ssid_equal(const uint16_t *left, const uint16_t *right)
{
    if (left == NULL || right == NULL)
    {
        return 0;
    }
    return reach_wifi_text_compare(left, right) == 0;
}

int32_t reach_wifi_security_needs_key(reach_wifi_security security)
{
    return security != REACH_WIFI_SECURITY_OPEN;
}

int32_t reach_wifi_security_is_supported(reach_wifi_security security)
{
    return security == REACH_WIFI_SECURITY_OPEN || security == REACH_WIFI_SECURITY_WEP ||
           security == REACH_WIFI_SECURITY_WPA2_PERSONAL ||
           security == REACH_WIFI_SECURITY_WPA3_PERSONAL;
}

const uint16_t *reach_wifi_security_label(reach_wifi_security security)
{
    switch (security)
    {
    case REACH_WIFI_SECURITY_OPEN:
        return (const uint16_t *)u"Open";
    case REACH_WIFI_SECURITY_WEP:
        return (const uint16_t *)u"WEP";
    case REACH_WIFI_SECURITY_WPA2_PERSONAL:
        return (const uint16_t *)u"WPA2";
    case REACH_WIFI_SECURITY_WPA3_PERSONAL:
        return (const uint16_t *)u"WPA3";
    case REACH_WIFI_SECURITY_ENTERPRISE:
        return (const uint16_t *)u"Enterprise";
    default:
        return (const uint16_t *)u"Secured";
    }
}

int32_t reach_wifi_key_length_valid(reach_wifi_security security, size_t length)
{
    switch (security)
    {
    case REACH_WIFI_SECURITY_OPEN:
        return length == 0;
    case REACH_WIFI_SECURITY_WEP:
        return length == 5 || length == 10 || length == 13 || length == 26;
    case REACH_WIFI_SECURITY_WPA2_PERSONAL:
    case REACH_WIFI_SECURITY_WPA3_PERSONAL:
        return (length >= REACH_WIFI_KEY_MINIMUM_LENGTH &&
                length <= REACH_WIFI_KEY_MAXIMUM_LENGTH) ||
               length == 64;
    default:
        return 0;
    }
}

static void reach_wifi_network_clamp(reach_wifi_network *network)
{
    if (network->signal_strength < 0)
    {
        network->signal_strength = 0;
    }
    if (network->signal_strength > 100)
    {
        network->signal_strength = 100;
    }
}

static void reach_wifi_network_merge(reach_wifi_network *kept, const reach_wifi_network *other)
{
    if (other->signal_strength > kept->signal_strength)
    {
        kept->signal_strength = other->signal_strength;
    }
    kept->connected = kept->connected || other->connected;
    kept->saved = kept->saved || other->saved;
    kept->in_range = kept->in_range || other->in_range;
    kept->connect_automatically = kept->connect_automatically || other->connect_automatically;
    kept->hidden = kept->hidden && other->hidden;
    if (kept->security == REACH_WIFI_SECURITY_UNKNOWN)
    {
        kept->security = other->security;
    }
}

static int reach_wifi_network_order(const reach_wifi_network *left,
                                    const reach_wifi_network *right)
{
    if (left->connected != right->connected)
    {
        return left->connected ? -1 : 1;
    }
    /* A network that is out of range cannot be joined now, so it ranks below every visible
       one even when it is saved. */
    if (left->in_range != right->in_range)
    {
        return left->in_range ? -1 : 1;
    }
    if (left->saved != right->saved)
    {
        return left->saved ? -1 : 1;
    }
    if (left->signal_strength != right->signal_strength)
    {
        return left->signal_strength > right->signal_strength ? -1 : 1;
    }
    return reach_wifi_text_compare(left->ssid, right->ssid);
}

void reach_wifi_network_list_normalize(reach_wifi_network_list *list)
{
    if (list == NULL)
    {
        return;
    }
    if (list->count > REACH_WIFI_MAX_NETWORKS)
    {
        list->count = REACH_WIFI_MAX_NETWORKS;
    }

    size_t kept_count = 0;
    for (size_t index = 0; index < list->count; ++index)
    {
        reach_wifi_network candidate = list->networks[index];
        if (candidate.ssid[0] == 0)
        {
            continue;
        }
        reach_wifi_network_clamp(&candidate);

        size_t existing = 0;
        int32_t merged = 0;
        for (existing = 0; existing < kept_count; ++existing)
        {
            if (!reach_wifi_ssid_equal(list->networks[existing].ssid, candidate.ssid))
            {
                continue;
            }
            reach_wifi_network_merge(&list->networks[existing], &candidate);
            merged = 1;
            break;
        }
        if (!merged)
        {
            list->networks[kept_count++] = candidate;
        }
    }
    list->count = kept_count;

    for (size_t index = 1; index < list->count; ++index)
    {
        reach_wifi_network moving = list->networks[index];
        size_t position = index;
        while (position > 0 && reach_wifi_network_order(&moving, &list->networks[position - 1]) < 0)
        {
            list->networks[position] = list->networks[position - 1];
            --position;
        }
        list->networks[position] = moving;
    }
}

size_t reach_wifi_network_list_find(const reach_wifi_network_list *list, const uint16_t *ssid)
{
    if (list == NULL || ssid == NULL || ssid[0] == 0)
    {
        return REACH_WIFI_MAX_NETWORKS;
    }
    for (size_t index = 0; index < list->count; ++index)
    {
        if (reach_wifi_ssid_equal(list->networks[index].ssid, ssid))
        {
            return index;
        }
    }
    return REACH_WIFI_MAX_NETWORKS;
}

