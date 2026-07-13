#include "reach/features/wallpaper.h"

#include <stdio.h>

static int failures;

struct fake_wallpaper_ports
{
    uint16_t current_path[260];
    uint16_t service_set_path[260];
    uint16_t surface_set_path[260];
    uint16_t monitor_paths[4][260];
    size_t monitor_indices[4];
    size_t service_set_count;
    size_t surface_set_count;
    size_t monitor_set_count;
    size_t current_query_count;
};

struct reach_wallpaper_service
{
    fake_wallpaper_ports *fake;
};

struct reach_wallpaper_surface
{
    fake_wallpaper_ports *fake;
};

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++failures;
        fprintf(stderr, "FAILED: %s\n", message);
    }
}

static void copy_ascii(uint16_t *dst, size_t dst_count, const char *src)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }

    size_t index = 0;
    while (src != nullptr && src[index] != 0 && index + 1 < dst_count)
    {
        dst[index] = (uint16_t)(unsigned char)src[index];
        ++index;
    }
    dst[index] = 0;
}

static int text_equals_ascii(const uint16_t *text, const char *expected)
{
    size_t index = 0;
    while (expected[index] != 0)
    {
        if (text[index] != (uint16_t)(unsigned char)expected[index])
        {
            return 0;
        }
        ++index;
    }
    return text[index] == 0;
}

static reach_result fake_service_set_wallpaper(reach_wallpaper_service *service,
                                               const uint16_t *path)
{
    ++service->fake->service_set_count;
    for (size_t index = 0; index < 260; ++index)
    {
        service->fake->service_set_path[index] = path[index];
        if (path[index] == 0)
        {
            break;
        }
    }
    return REACH_OK;
}

static reach_result fake_service_current_wallpaper(reach_wallpaper_service *service,
                                                   uint16_t *out_path, size_t out_path_count)
{
    ++service->fake->current_query_count;
    if (out_path == nullptr || out_path_count == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    for (size_t index = 0; index + 1 < out_path_count; ++index)
    {
        out_path[index] = service->fake->current_path[index];
        if (service->fake->current_path[index] == 0)
        {
            return REACH_OK;
        }
    }
    out_path[out_path_count - 1] = 0;
    return REACH_OK;
}

static reach_result fake_surface_set_wallpaper(reach_wallpaper_surface *surface,
                                               const uint16_t *path)
{
    ++surface->fake->surface_set_count;
    for (size_t index = 0; index < 260; ++index)
    {
        surface->fake->surface_set_path[index] = path[index];
        if (path[index] == 0)
        {
            break;
        }
    }
    return REACH_OK;
}

static reach_result fake_surface_set_monitor_wallpaper(reach_wallpaper_surface *surface,
                                                       size_t monitor_index, const uint16_t *path)
{
    if (surface->fake->monitor_set_count >= 4)
    {
        return REACH_ERROR;
    }
    const size_t slot = surface->fake->monitor_set_count++;
    surface->fake->monitor_indices[slot] = monitor_index;
    for (size_t index = 0; index < 260; ++index)
    {
        surface->fake->monitor_paths[slot][index] = path[index];
        if (path[index] == 0)
        {
            break;
        }
    }
    return REACH_OK;
}

static reach_wallpaper_service_port service_port_for(fake_wallpaper_ports *fake,
                                                     reach_wallpaper_service *service)
{
    service->fake = fake;
    reach_wallpaper_service_port port = {};
    port.service = service;
    port.ops.set_wallpaper = fake_service_set_wallpaper;
    port.ops.current_wallpaper = fake_service_current_wallpaper;
    return port;
}

static reach_wallpaper_surface_port surface_port_for(fake_wallpaper_ports *fake,
                                                     reach_wallpaper_surface *surface)
{
    surface->fake = fake;
    reach_wallpaper_surface_port port = {};
    port.surface = surface;
    port.ops.set_wallpaper = fake_surface_set_wallpaper;
    port.ops.set_monitor_wallpaper = fake_surface_set_monitor_wallpaper;
    return port;
}

static void test_configured_wallpaper_applies_to_service_surface_and_monitors(void)
{
    fake_wallpaper_ports fake = {};
    reach_wallpaper_service service = {};
    reach_wallpaper_surface surface = {};
    reach_wallpaper_service_port service_port = service_port_for(&fake, &service);
    reach_wallpaper_surface_port surface_port = surface_port_for(&fake, &surface);

    uint16_t wallpaper_path[260] = {};
    uint16_t monitor_paths[3][260] = {};
    uint16_t cached_path[260] = {};
    copy_ascii(wallpaper_path, 260, "C:\\Wallpapers\\main.jpg");
    copy_ascii(monitor_paths[0], 260, "C:\\Wallpapers\\left.jpg");
    copy_ascii(monitor_paths[2], 260, "C:\\Wallpapers\\right.jpg");

    int32_t changed = reach_wallpaper_seed_or_apply(&service_port, &surface_port, wallpaper_path,
                                                    260, monitor_paths, 3, cached_path, 260);

    expect_true(changed == 0, "configured wallpaper does not report seeded config change");
    expect_true(text_equals_ascii(cached_path, "C:\\Wallpapers\\main.jpg"),
                "configured wallpaper is cached");
    expect_true(fake.current_query_count == 0, "configured wallpaper does not query current OS");
    expect_true(fake.service_set_count == 1 &&
                    text_equals_ascii(fake.service_set_path, "C:\\Wallpapers\\main.jpg"),
                "configured wallpaper applies to service");
    expect_true(fake.surface_set_count == 1 &&
                    text_equals_ascii(fake.surface_set_path, "C:\\Wallpapers\\main.jpg"),
                "configured wallpaper applies to surface");
    expect_true(fake.monitor_set_count == 2, "configured monitor wallpapers apply to surface");
    expect_true(fake.monitor_indices[0] == 0 &&
                    text_equals_ascii(fake.monitor_paths[0], "C:\\Wallpapers\\left.jpg"),
                "first configured monitor wallpaper is applied");
    expect_true(fake.monitor_indices[1] == 2 &&
                    text_equals_ascii(fake.monitor_paths[1], "C:\\Wallpapers\\right.jpg"),
                "sparse configured monitor wallpaper keeps monitor index");
}

static void test_empty_config_seeds_from_current_wallpaper(void)
{
    fake_wallpaper_ports fake = {};
    copy_ascii(fake.current_path, 260, "C:\\Wallpapers\\current.jpg");
    reach_wallpaper_service service = {};
    reach_wallpaper_surface surface = {};
    reach_wallpaper_service_port service_port = service_port_for(&fake, &service);
    reach_wallpaper_surface_port surface_port = surface_port_for(&fake, &surface);

    uint16_t wallpaper_path[260] = {};
    uint16_t monitor_paths[2][260] = {};
    uint16_t cached_path[260] = {};
    copy_ascii(monitor_paths[1], 260, "C:\\Wallpapers\\second.jpg");

    int32_t changed = reach_wallpaper_seed_or_apply(&service_port, &surface_port, wallpaper_path,
                                                    260, monitor_paths, 2, cached_path, 260);

    expect_true(changed == 1, "empty wallpaper config reports seeded config change");
    expect_true(text_equals_ascii(wallpaper_path, "C:\\Wallpapers\\current.jpg"),
                "current OS wallpaper seeds config");
    expect_true(text_equals_ascii(cached_path, "C:\\Wallpapers\\current.jpg"),
                "current OS wallpaper is cached");
    expect_true(fake.current_query_count == 1, "empty config queries current OS wallpaper");
    expect_true(fake.service_set_count == 0, "seeding does not set service wallpaper");
    expect_true(fake.surface_set_count == 1 &&
                    text_equals_ascii(fake.surface_set_path, "C:\\Wallpapers\\current.jpg"),
                "seeded wallpaper applies to surface");
    expect_true(fake.monitor_set_count == 1 && fake.monitor_indices[0] == 1,
                "seeded path still applies configured monitor overrides");
}

static void test_empty_config_without_current_wallpaper_only_applies_monitors(void)
{
    fake_wallpaper_ports fake = {};
    reach_wallpaper_service service = {};
    reach_wallpaper_surface surface = {};
    reach_wallpaper_service_port service_port = service_port_for(&fake, &service);
    reach_wallpaper_surface_port surface_port = surface_port_for(&fake, &surface);

    uint16_t wallpaper_path[260] = {};
    uint16_t monitor_paths[1][260] = {};
    uint16_t cached_path[260] = {};
    copy_ascii(monitor_paths[0], 260, "C:\\Wallpapers\\monitor.jpg");

    int32_t changed = reach_wallpaper_seed_or_apply(&service_port, &surface_port, wallpaper_path,
                                                    260, monitor_paths, 1, cached_path, 260);

    expect_true(changed == 0, "missing current wallpaper does not seed config");
    expect_true(wallpaper_path[0] == 0, "empty config remains empty without current wallpaper");
    expect_true(cached_path[0] == 0, "cache remains empty without current wallpaper");
    expect_true(fake.surface_set_count == 0, "surface global wallpaper is not set");
    expect_true(fake.monitor_set_count == 1, "monitor override still applies");
}

int main(void)
{
    test_configured_wallpaper_applies_to_service_surface_and_monitors();
    test_empty_config_seeds_from_current_wallpaper();
    test_empty_config_without_current_wallpaper_only_applies_monitors();
    return failures == 0 ? 0 : 1;
}
