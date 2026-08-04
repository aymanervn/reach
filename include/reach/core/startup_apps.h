#ifndef REACH_CORE_STARTUP_APPS_H
#define REACH_CORE_STARTUP_APPS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_STARTUP_APP_MAX_ENTRIES 64
#define REACH_STARTUP_APP_NAME_CAPACITY 128
#define REACH_STARTUP_APP_PATH_CAPACITY 260
#define REACH_STARTUP_APP_COMMAND_CAPACITY 520

    typedef enum reach_startup_app_source
    {
        REACH_STARTUP_APP_SOURCE_USER_RUN = 0,
        REACH_STARTUP_APP_SOURCE_MACHINE_RUN = 1,
        REACH_STARTUP_APP_SOURCE_USER_FOLDER = 2,
        REACH_STARTUP_APP_SOURCE_MACHINE_FOLDER = 3
    } reach_startup_app_source;

    typedef struct reach_startup_app_entry
    {
        uint16_t key[REACH_STARTUP_APP_NAME_CAPACITY];
        uint16_t display_name[REACH_STARTUP_APP_NAME_CAPACITY];
        uint16_t command[REACH_STARTUP_APP_COMMAND_CAPACITY];
        uint16_t executable[REACH_STARTUP_APP_PATH_CAPACITY];
        uint16_t arguments[REACH_STARTUP_APP_PATH_CAPACITY];
        reach_startup_app_source source;
        int32_t enabled;
        int32_t resolved;
    } reach_startup_app_entry;

    typedef struct reach_startup_app_list
    {
        reach_startup_app_entry entries[REACH_STARTUP_APP_MAX_ENTRIES];
        size_t count;
    } reach_startup_app_list;

    const uint16_t *reach_startup_app_source_label(reach_startup_app_source source);
    int32_t reach_startup_app_source_is_machine(reach_startup_app_source source);
    size_t reach_startup_app_enabled_count(const reach_startup_app_list *list);

#ifdef __cplusplus
}
#endif
#endif
