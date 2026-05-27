#ifndef REACHCTL_CONFIG_COMMANDS_H
#define REACHCTL_CONFIG_COMMANDS_H

#include <wchar.h>

int reachctl_pin_command(const wchar_t *path);
int reachctl_wallpaper_monitor_command(
    const wchar_t *index_text,
    const wchar_t *path);

#endif
