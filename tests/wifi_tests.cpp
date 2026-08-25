#include "reach/core/wifi.h"

#include "../src/adapters/windows/wifi_profile.h"

#include <stdio.h>

static int failures = 0;

static void expect_true(int value, const char *message)
{
    if (!value)
    {
        ++failures;
        printf("FAIL: %s\n", message);
    }
}

static void copy_ascii(uint16_t *destination, size_t capacity, const char *source)
{
    size_t index = 0;
    while (source[index] != 0 && index + 1 < capacity)
    {
        destination[index] = (uint16_t)(unsigned char)source[index];
        ++index;
    }
    destination[index] = 0;
}

static int contains_ascii(const uint16_t *text, const char *needle)
{
    for (size_t start = 0; text[start] != 0; ++start)
    {
        size_t index = 0;
        while (needle[index] != 0 &&
               text[start + index] == (uint16_t)(unsigned char)needle[index])
        {
            ++index;
        }
        if (needle[index] == 0)
        {
            return 1;
        }
    }
    return 0;
}

static reach_wifi_network make_network(const char *ssid, int32_t signal, int32_t connected,
                                       int32_t saved, int32_t in_range)
{
    reach_wifi_network network = {};
    copy_ascii(network.ssid, REACH_WIFI_SSID_CAPACITY, ssid);
    network.signal_strength = signal;
    network.connected = connected;
    network.saved = saved;
    network.in_range = in_range;
    network.security = REACH_WIFI_SECURITY_WPA2_PERSONAL;
    return network;
}

/* One SSID advertised by several access points must collapse to one row that keeps the
   strongest signal and the union of the connected/saved/in-range facts. */
static void test_normalize_merges_duplicate_ssids(void)
{
    reach_wifi_network_list list = {};
    list.networks[list.count++] = make_network("Mesh", 40, 0, 0, 1);
    list.networks[list.count++] = make_network("Mesh", 88, 0, 1, 1);
    list.networks[list.count++] = make_network("Mesh", 61, 0, 0, 1);
    list.networks[list.count++] = make_network("Other", 55, 0, 0, 1);

    reach_wifi_network_list_normalize(&list);

    expect_true(list.count == 2, "duplicate SSIDs collapse into one row");
    size_t mesh = reach_wifi_network_list_find(&list, list.networks[0].ssid);
    expect_true(mesh == 0, "find locates the first row by SSID");
    expect_true(list.networks[0].signal_strength == 88, "merged row keeps the strongest signal");
    expect_true(list.networks[0].saved == 1, "merged row keeps the saved flag");
}

static void test_normalize_orders_connected_then_saved_then_signal(void)
{
    reach_wifi_network_list list = {};
    list.networks[list.count++] = make_network("Weak", 20, 0, 0, 1);
    list.networks[list.count++] = make_network("Strong", 95, 0, 0, 1);
    list.networks[list.count++] = make_network("Saved", 30, 0, 1, 1);
    list.networks[list.count++] = make_network("Live", 10, 1, 1, 1);
    list.networks[list.count++] = make_network("OutOfRange", 0, 0, 1, 0);

    reach_wifi_network_list_normalize(&list);

    expect_true(list.count == 5, "every distinct SSID survives");
    expect_true(list.networks[0].connected, "the connected network sorts first");
    expect_true(list.networks[1].saved && list.networks[1].in_range,
                "an in-range saved network sorts above unsaved ones");
    expect_true(list.networks[2].signal_strength == 95,
                "unsaved networks sort by descending signal");
    expect_true(list.networks[4].in_range == 0, "out-of-range networks sort last");
}

static void test_normalize_drops_empty_ssids_and_clamps_signal(void)
{
    reach_wifi_network_list list = {};
    list.networks[list.count++] = make_network("", 50, 0, 0, 1);
    list.networks[list.count++] = make_network("Clamped", 250, 0, 0, 1);
    list.networks[list.count++] = make_network("Negative", -20, 0, 0, 1);

    reach_wifi_network_list_normalize(&list);

    expect_true(list.count == 2, "networks with no SSID are dropped");
    expect_true(list.networks[0].signal_strength == 100, "signal is clamped to 100");
    expect_true(list.networks[1].signal_strength == 0, "signal is clamped to 0");
}

static void test_key_length_rules(void)
{
    expect_true(reach_wifi_key_length_valid(REACH_WIFI_SECURITY_OPEN, 0),
                "an open network takes no key");
    expect_true(!reach_wifi_key_length_valid(REACH_WIFI_SECURITY_OPEN, 8),
                "an open network rejects a key");
    expect_true(!reach_wifi_key_length_valid(REACH_WIFI_SECURITY_WPA2_PERSONAL, 7),
                "WPA2 rejects a 7 character passphrase");
    expect_true(reach_wifi_key_length_valid(REACH_WIFI_SECURITY_WPA2_PERSONAL, 8),
                "WPA2 accepts the 8 character minimum");
    expect_true(reach_wifi_key_length_valid(REACH_WIFI_SECURITY_WPA2_PERSONAL, 63),
                "WPA2 accepts the 63 character maximum");
    expect_true(!reach_wifi_key_length_valid(REACH_WIFI_SECURITY_WPA2_PERSONAL, 65),
                "WPA2 rejects a 65 character passphrase");
    expect_true(reach_wifi_key_length_valid(REACH_WIFI_SECURITY_WPA2_PERSONAL, 64),
                "WPA2 accepts a 64 character hex PSK");
    expect_true(reach_wifi_key_length_valid(REACH_WIFI_SECURITY_WPA3_PERSONAL, 12),
                "WPA3 follows the WPA2 passphrase rule");
    expect_true(!reach_wifi_key_length_valid(REACH_WIFI_SECURITY_ENTERPRISE, 12),
                "enterprise networks are not key-connectable");
    expect_true(!reach_wifi_security_is_supported(REACH_WIFI_SECURITY_ENTERPRISE),
                "enterprise networks are reported unsupported");
}

static void test_profile_xml_escapes_and_matches_security(void)
{
    uint16_t ssid[REACH_WIFI_SSID_CAPACITY] = {};
    uint16_t key[REACH_WIFI_KEY_CAPACITY] = {};
    uint16_t xml[REACH_WIFI_PROFILE_CAPACITY] = {};

    copy_ascii(ssid, REACH_WIFI_SSID_CAPACITY, "Ben & Jerry's <Wi-Fi>");
    copy_ascii(key, REACH_WIFI_KEY_CAPACITY, "correct horse");

    size_t length = reach_wifi_build_profile_xml(xml, REACH_WIFI_PROFILE_CAPACITY, ssid, key,
                                                 REACH_WIFI_SECURITY_WPA2_PERSONAL, 1, 0);
    expect_true(length > 0, "a WPA2 profile is produced");
    expect_true(contains_ascii(xml, "Ben &amp; Jerry&apos;s &lt;Wi-Fi&gt;"),
                "the SSID is XML escaped");
    expect_true(!contains_ascii(xml, "<name>Ben & Jerry"), "no raw ampersand reaches the XML");
    expect_true(contains_ascii(xml, "<authentication>WPA2PSK</authentication>"),
                "WPA2 selects WPA2PSK authentication");
    expect_true(contains_ascii(xml, "<connectionMode>auto</connectionMode>"),
                "connect automatically writes an auto profile");
    expect_true(contains_ascii(xml, "<keyMaterial>correct horse</keyMaterial>"),
                "the passphrase is written as key material");

    length = reach_wifi_build_profile_xml(xml, REACH_WIFI_PROFILE_CAPACITY, ssid, key,
                                          REACH_WIFI_SECURITY_WPA3_PERSONAL, 0, 1);
    expect_true(length > 0, "a WPA3 profile is produced");
    expect_true(contains_ascii(xml, "<authentication>WPA3SAE</authentication>"),
                "WPA3 selects WPA3SAE authentication");
    expect_true(contains_ascii(xml, "<connectionMode>manual</connectionMode>"),
                "a manual profile is written when auto-connect is off");
    expect_true(contains_ascii(xml, "<nonBroadcast>true</nonBroadcast>"),
                "a hidden network is marked non-broadcast");
}

static void test_profile_xml_rejects_invalid_requests(void)
{
    uint16_t ssid[REACH_WIFI_SSID_CAPACITY] = {};
    uint16_t key[REACH_WIFI_KEY_CAPACITY] = {};
    uint16_t empty[1] = {0};
    uint16_t xml[REACH_WIFI_PROFILE_CAPACITY] = {};

    copy_ascii(ssid, REACH_WIFI_SSID_CAPACITY, "Home");
    copy_ascii(key, REACH_WIFI_KEY_CAPACITY, "short");

    expect_true(reach_wifi_build_profile_xml(xml, REACH_WIFI_PROFILE_CAPACITY, ssid, key,
                                             REACH_WIFI_SECURITY_WPA2_PERSONAL, 1, 0) == 0,
                "a too-short WPA2 passphrase produces no profile");
    expect_true(reach_wifi_build_profile_xml(xml, REACH_WIFI_PROFILE_CAPACITY, empty, key,
                                             REACH_WIFI_SECURITY_WPA2_PERSONAL, 1, 0) == 0,
                "an empty SSID produces no profile");
    expect_true(reach_wifi_build_profile_xml(xml, REACH_WIFI_PROFILE_CAPACITY, ssid, empty,
                                             REACH_WIFI_SECURITY_ENTERPRISE, 1, 0) == 0,
                "an enterprise network produces no profile");

    size_t length = reach_wifi_build_profile_xml(xml, REACH_WIFI_PROFILE_CAPACITY, ssid, empty,
                                                 REACH_WIFI_SECURITY_OPEN, 1, 0);
    expect_true(length > 0, "an open network produces a profile with no key");
    expect_true(!contains_ascii(xml, "<sharedKey>"), "an open profile carries no shared key");
    expect_true(contains_ascii(xml, "<authentication>open</authentication>"),
                "an open network selects open authentication");
}

int main(void)
{
    test_normalize_merges_duplicate_ssids();
    test_normalize_orders_connected_then_saved_then_signal();
    test_normalize_drops_empty_ssids_and_clamps_signal();
    test_key_length_rules();
    test_profile_xml_escapes_and_matches_security();
    test_profile_xml_rejects_invalid_requests();

    if (failures == 0)
    {
        printf("wifi tests passed\n");
    }
    return failures == 0 ? 0 : 1;
}
