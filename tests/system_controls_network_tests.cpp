#include "../src/adapters/windows/system_controls_network.h"

#include <stdio.h>

static int failures;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++failures;
        printf("FAIL: %s\n", message);
    }
}

static reach_windows_network_adapter adapter(uint32_t if_type, int32_t oper_up, int32_t has_unicast)
{
    reach_windows_network_adapter result = {};
    result.if_type = if_type;
    result.oper_up = oper_up;
    result.has_unicast = has_unicast;
    return result;
}

static void test_wifi_wins_over_ethernet(void)
{
    reach_windows_network_adapter adapters[] = {
        adapter(REACH_WINDOWS_NETWORK_IF_TYPE_ETHERNET, 1, 1),
        adapter(REACH_WINDOWS_NETWORK_IF_TYPE_WIFI, 1, 1),
    };

    expect_true(reach_windows_network_kind_from_adapters(adapters, 2) == REACH_NETWORK_KIND_WIFI,
                "connected wifi wins over ethernet");
}

static void test_ethernet_when_only_ethernet_is_connected(void)
{
    reach_windows_network_adapter adapters[] = {
        adapter(REACH_WINDOWS_NETWORK_IF_TYPE_ETHERNET, 1, 1),
    };

    expect_true(reach_windows_network_kind_from_adapters(adapters, 1) ==
                    REACH_NETWORK_KIND_ETHERNET,
                "ethernet is reported when it is the only connected adapter");
}

static void test_unusable_adapters_are_ignored(void)
{
    reach_windows_network_adapter adapters[] = {
        adapter(REACH_WINDOWS_NETWORK_IF_TYPE_WIFI, 0, 1),
        adapter(REACH_WINDOWS_NETWORK_IF_TYPE_WIFI, 1, 0),
        adapter(24, 1, 1),
    };

    expect_true(reach_windows_network_kind_from_adapters(adapters, 3) == REACH_NETWORK_KIND_NONE,
                "down, addressless, and non-lan adapters are ignored");
}

static void test_null_or_empty_adapter_list_is_disconnected(void)
{
    expect_true(reach_windows_network_kind_from_adapters(nullptr, 1) == REACH_NETWORK_KIND_NONE,
                "null adapter list is disconnected");
    expect_true(reach_windows_network_kind_from_adapters(nullptr, 0) == REACH_NETWORK_KIND_NONE,
                "empty adapter list is disconnected");
}

int main(void)
{
    test_wifi_wins_over_ethernet();
    test_ethernet_when_only_ethernet_is_connected();
    test_unusable_adapters_are_ignored();
    test_null_or_empty_adapter_list_is_disconnected();

    if (failures != 0)
    {
        printf("%d failure(s)\n", failures);
        return 1;
    }

    return 0;
}
