#ifndef REACH_CORE_WINDOWS_UPDATE_H
#define REACH_CORE_WINDOWS_UPDATE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define REACH_WINDOWS_UPDATE_MAX_UPDATES 64
#define REACH_WINDOWS_UPDATE_TEXT_CAPACITY 260
#define REACH_WINDOWS_UPDATE_METADATA_CAPACITY 512
#define REACH_WINDOWS_UPDATE_ID_CAPACITY 64

typedef enum reach_windows_update_state {
    REACH_WINDOWS_UPDATE_DISCOVERED = 0,
    REACH_WINDOWS_UPDATE_SELECTED,
    REACH_WINDOWS_UPDATE_DOWNLOADING,
    REACH_WINDOWS_UPDATE_DOWNLOADED,
    REACH_WINDOWS_UPDATE_INSTALLING,
    REACH_WINDOWS_UPDATE_INSTALLED_NO_REBOOT_REQUIRED,
    REACH_WINDOWS_UPDATE_INSTALLED_REBOOT_REQUIRED,
    REACH_WINDOWS_UPDATE_REBOOT_OBSERVED,
    REACH_WINDOWS_UPDATE_VERIFIED_INSTALLED,
    REACH_WINDOWS_UPDATE_FAILED
} reach_windows_update_state;

typedef enum reach_windows_update_failure_class {
    REACH_WINDOWS_UPDATE_FAILURE_NONE = 0,
    REACH_WINDOWS_UPDATE_NOT_ELEVATED,
    REACH_WINDOWS_UPDATE_DOWNLOAD_FAILED,
    REACH_WINDOWS_UPDATE_INSTALL_FAILED,
    REACH_WINDOWS_UPDATE_VERIFICATION_FAILED,
    REACH_WINDOWS_UPDATE_REBOOT_REQUIRED_BEFORE_INSTALL,
    REACH_WINDOWS_UPDATE_SUPERSEDED_OR_NO_LONGER_APPLICABLE,
    REACH_WINDOWS_UPDATE_POLICY_BLOCKED,
    REACH_WINDOWS_UPDATE_ANOTHER_OPERATION_IN_PROGRESS
} reach_windows_update_failure_class;

typedef enum reach_windows_update_verification_status {
    REACH_WINDOWS_UPDATE_VERIFICATION_STATUS_NOT_RUN = 0,
    REACH_WINDOWS_UPDATE_VERIFICATION_STATUS_PENDING_REBOOT,
    REACH_WINDOWS_UPDATE_VERIFICATION_STATUS_SUCCEEDED,
    REACH_WINDOWS_UPDATE_VERIFICATION_STATUS_FAILED
} reach_windows_update_verification_status;

typedef struct reach_windows_update_identity {
    uint16_t update_id[REACH_WINDOWS_UPDATE_ID_CAPACITY];
    int32_t revision_number;
    uint16_t kb_article_ids[REACH_WINDOWS_UPDATE_METADATA_CAPACITY];
    uint16_t title[REACH_WINDOWS_UPDATE_TEXT_CAPACITY];
} reach_windows_update_identity;

typedef struct reach_windows_update_item {
    reach_windows_update_identity identity;
    uint16_t categories[REACH_WINDOWS_UPDATE_METADATA_CAPACITY];
    uint16_t selected_reason[REACH_WINDOWS_UPDATE_TEXT_CAPACITY];
    int32_t downloaded;
    int32_t reboot_required_known;
    int32_t reboot_required;
    int32_t selected;
    reach_windows_update_state state;
    reach_windows_update_failure_class failure_class;
    int32_t download_result_code;
    int32_t download_hresult;
    int32_t install_result_code;
    int32_t install_hresult;
    reach_windows_update_verification_status verification_status;
} reach_windows_update_item;

typedef struct reach_windows_update_list {
    reach_windows_update_item updates[REACH_WINDOWS_UPDATE_MAX_UPDATES];
    size_t count;
} reach_windows_update_list;

typedef struct reach_windows_update_operation_result {
    uint16_t operation[32];
    uint16_t started_utc[32];
    uint16_t completed_utc[32];
    int32_t overall_download_result_code;
    int32_t overall_download_hresult;
    int32_t overall_install_result_code;
    int32_t overall_install_hresult;
    int32_t overall_reboot_required;
    reach_windows_update_failure_class failure_class;
    reach_windows_update_item per_update_results[REACH_WINDOWS_UPDATE_MAX_UPDATES];
    size_t per_update_result_count;
} reach_windows_update_operation_result;

#ifdef __cplusplus
}
#endif
#endif
