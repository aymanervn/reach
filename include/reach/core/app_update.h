#ifndef REACH_CORE_APP_UPDATE_H
#define REACH_CORE_APP_UPDATE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_APP_UPDATE_VERSION_CAPACITY 32
#define REACH_APP_UPDATE_URL_CAPACITY 512
#define REACH_APP_UPDATE_NAME_CAPACITY 128
#define REACH_APP_UPDATE_NOTES_CAPACITY 1024

    typedef struct reach_app_update_info
    {
        uint16_t version[REACH_APP_UPDATE_VERSION_CAPACITY];
        uint16_t asset_name[REACH_APP_UPDATE_NAME_CAPACITY];
        uint16_t download_url[REACH_APP_UPDATE_URL_CAPACITY];
        uint16_t notes[REACH_APP_UPDATE_NOTES_CAPACITY];
        uint64_t asset_size;
        int32_t has_release;
    } reach_app_update_info;

    int32_t reach_app_version_parse(const char *text, int32_t out_components[3]);
    int32_t reach_app_version_compare(const char *left, const char *right);

#ifdef __cplusplus
}
#endif
#endif
