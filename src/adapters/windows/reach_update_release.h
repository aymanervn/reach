#ifndef REACH_ADAPTERS_WINDOWS_REACH_UPDATE_RELEASE_H
#define REACH_ADAPTERS_WINDOWS_REACH_UPDATE_RELEASE_H

#include <stddef.h>
#include <stdint.h>

#include <string>

struct reach_windows_app_update_release
{
    std::string tag;
    std::string asset_name;
    std::string asset_url;
    std::string digest;
    uint64_t asset_size;
};

int32_t reach_windows_app_update_parse_release(const std::string &json,
                                               reach_windows_app_update_release *out_release);
int32_t reach_windows_app_update_file_matches_digest(const wchar_t *path, const char *digest);

#endif
