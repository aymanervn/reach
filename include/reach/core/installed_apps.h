#ifndef REACH_CORE_INSTALLED_APPS_H
#define REACH_CORE_INSTALLED_APPS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_INSTALLED_APP_MAX_ENTRIES 256
#define REACH_INSTALLED_APP_NAME_CAPACITY 128
#define REACH_INSTALLED_APP_TEXT_CAPACITY 260

    typedef enum reach_installed_app_kind
    {
        REACH_INSTALLED_APP_DESKTOP = 0,
        REACH_INSTALLED_APP_PACKAGED = 1
    } reach_installed_app_kind;

    typedef struct reach_installed_app
    {
        uint16_t display_name[REACH_INSTALLED_APP_NAME_CAPACITY];
        uint16_t publisher[REACH_INSTALLED_APP_NAME_CAPACITY];
        uint16_t version[64];
        uint16_t launch_path[REACH_INSTALLED_APP_TEXT_CAPACITY];
        uint16_t icon_ref[REACH_INSTALLED_APP_TEXT_CAPACITY];
        uint16_t app_user_model_id[REACH_INSTALLED_APP_TEXT_CAPACITY];
        uint16_t package_family_name[REACH_INSTALLED_APP_TEXT_CAPACITY];
        uint16_t package_full_name[REACH_INSTALLED_APP_TEXT_CAPACITY];
        reach_installed_app_kind kind;
        int32_t can_open;
        int32_t can_uninstall;
        int32_t can_manage;
    } reach_installed_app;

    typedef struct reach_installed_app_list
    {
        reach_installed_app entries[REACH_INSTALLED_APP_MAX_ENTRIES];
        size_t count;
    } reach_installed_app_list;

#ifdef __cplusplus
}
#endif

#endif
