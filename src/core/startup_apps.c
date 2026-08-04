#include "reach/core/startup_apps.h"

const uint16_t *reach_startup_app_source_label(reach_startup_app_source source)
{
    switch (source)
    {
    case REACH_STARTUP_APP_SOURCE_USER_RUN:
    case REACH_STARTUP_APP_SOURCE_USER_FOLDER:
        return (const uint16_t *)u"This user";
    case REACH_STARTUP_APP_SOURCE_MACHINE_RUN:
    case REACH_STARTUP_APP_SOURCE_MACHINE_FOLDER:
        return (const uint16_t *)u"All users";
    default:
        return (const uint16_t *)u"";
    }
}

int32_t reach_startup_app_source_is_machine(reach_startup_app_source source)
{
    return source == REACH_STARTUP_APP_SOURCE_MACHINE_RUN ||
           source == REACH_STARTUP_APP_SOURCE_MACHINE_FOLDER;
}

size_t reach_startup_app_enabled_count(const reach_startup_app_list *list)
{
    size_t count = 0;
    if (list == NULL)
    {
        return 0;
    }
    for (size_t index = 0; index < list->count && index < REACH_STARTUP_APP_MAX_ENTRIES; ++index)
    {
        if (list->entries[index].enabled)
        {
            ++count;
        }
    }
    return count;
}
