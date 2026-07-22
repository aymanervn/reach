#include "settings_pages_internal.h"

#include "reach/support/util.h"

#include <memory>
#include <new>

const uint16_t *reach_settings_update_page_title(void)
{
    return (const uint16_t *)u"Updates";
}

static void reach_settings_version_to_ascii(const uint16_t *version, char *out, size_t capacity)
{
    size_t index = 0;
    while (version != nullptr && version[index] != 0 && index + 1 < capacity)
    {
        out[index] = version[index] < 128 ? (char)version[index] : '?';
        ++index;
    }
    out[index] = 0;
}

void reach_settings_model_set_current_version(reach_settings_model *model, const uint16_t *version)
{
    if (model == nullptr)
        return;
    reach_copy_utf16(model->reach_current_version, REACH_APP_UPDATE_VERSION_CAPACITY, version);
}

void reach_settings_model_begin_reach_check(reach_settings_model *model)
{
    if (model == nullptr || model->reach_update_state == REACH_SETTINGS_REACH_UPDATE_CHECKING ||
        model->reach_update_state == REACH_SETTINGS_REACH_UPDATE_DOWNLOADING)
        return;
    model->reach_update_state = REACH_SETTINGS_REACH_UPDATE_CHECKING;
}

void reach_settings_model_apply_reach_check(reach_settings_model *model,
                                            const reach_app_update_info *info, int32_t result)
{
    if (model == nullptr)
        return;
    if (result == 0 || info == nullptr || !info->has_release)
    {
        model->reach_update_state = REACH_SETTINGS_REACH_UPDATE_ERROR;
        return;
    }
    model->reach_update_info = *info;
    char current[REACH_APP_UPDATE_VERSION_CAPACITY] = {};
    char latest[REACH_APP_UPDATE_VERSION_CAPACITY] = {};
    reach_settings_version_to_ascii(model->reach_current_version, current, sizeof(current));
    reach_settings_version_to_ascii(info->version, latest, sizeof(latest));
    model->reach_update_state = reach_app_version_compare(current, latest) < 0
                                    ? REACH_SETTINGS_REACH_UPDATE_AVAILABLE
                                    : REACH_SETTINGS_REACH_UPDATE_UP_TO_DATE;
}

void reach_settings_model_begin_reach_download(reach_settings_model *model)
{
    if (model == nullptr || model->reach_update_state != REACH_SETTINGS_REACH_UPDATE_AVAILABLE)
        return;
    model->reach_download_received = 0;
    model->reach_download_total = 0;
    model->reach_update_state = REACH_SETTINGS_REACH_UPDATE_DOWNLOADING;
}

void reach_settings_model_apply_reach_download(reach_settings_model *model, int32_t success)
{
    if (model == nullptr)
        return;
    model->reach_update_state = success ? REACH_SETTINGS_REACH_UPDATE_AVAILABLE
                                        : REACH_SETTINGS_REACH_UPDATE_ERROR;
}

int32_t reach_settings_model_reach_update_action_enabled(const reach_settings_model *model)
{
    return model != nullptr && model->reach_update_state == REACH_SETTINGS_REACH_UPDATE_AVAILABLE;
}

const uint16_t *reach_settings_model_reach_update_status(const reach_settings_model *model)
{
    if (model == nullptr)
        return (const uint16_t *)u"";
    switch (model->reach_update_state)
    {
    case REACH_SETTINGS_REACH_UPDATE_CHECKING:
        return (const uint16_t *)u"Checking for updates...";
    case REACH_SETTINGS_REACH_UPDATE_UP_TO_DATE:
        return (const uint16_t *)u"Reach is up to date.";
    case REACH_SETTINGS_REACH_UPDATE_AVAILABLE:
        return (const uint16_t *)u"A new version is available.";
    case REACH_SETTINGS_REACH_UPDATE_DOWNLOADING:
        return (const uint16_t *)u"Downloading update...";
    case REACH_SETTINGS_REACH_UPDATE_ERROR:
        return (const uint16_t *)u"Unable to check for updates.";
    case REACH_SETTINGS_REACH_UPDATE_IDLE:
    default:
        return (const uint16_t *)u"Check for the latest Reach release.";
    }
}

const uint16_t *reach_settings_model_reach_update_button_label(const reach_settings_model *model)
{
    if (model == nullptr)
        return (const uint16_t *)u"Check";
    switch (model->reach_update_state)
    {
    case REACH_SETTINGS_REACH_UPDATE_CHECKING:
        return (const uint16_t *)u"Checking...";
    case REACH_SETTINGS_REACH_UPDATE_AVAILABLE:
        return (const uint16_t *)u"Update";
    case REACH_SETTINGS_REACH_UPDATE_DOWNLOADING:
        return (const uint16_t *)u"Updating...";
    case REACH_SETTINGS_REACH_UPDATE_UP_TO_DATE:
        return (const uint16_t *)u"Up to date";
    default:
        return (const uint16_t *)u"Check";
    }
}

static int identity_equal(const reach_windows_update_identity *left,
                          const reach_windows_update_identity *right);

void reach_settings_model_begin_update_scan(reach_settings_model *model)
{
    if (model == nullptr || reach_settings_model_update_busy(model))
        return;
    model->update_page_state = REACH_SETTINGS_UPDATE_SCANNING;
    reach_scrollbar_model_init(&model->update_scrollbar, REACH_SCROLLBAR_DRAG_FREE, 0.0f);
}

void reach_settings_model_apply_update_scan(reach_settings_model *model,
                                            const reach_windows_update_list *updates,
                                            int32_t hresult)
{
    if (model == nullptr)
        return;
    if (updates == nullptr || hresult < 0)
    {
        model->update_page_state = REACH_SETTINGS_UPDATE_ERROR;
        return;
    }
    model->update_scan_completed = 1;
    std::unique_ptr<reach_windows_update_list> previous(
        new (std::nothrow) reach_windows_update_list(model->update_list));
    model->update_list = *updates;
    if (model->update_list.count > REACH_WINDOWS_UPDATE_MAX_UPDATES)
        model->update_list.count = REACH_WINDOWS_UPDATE_MAX_UPDATES;
    reach_windows_update_apply_default_selection(&model->update_list);
    if (previous != nullptr)
    {
        for (size_t index = 0; index < model->update_list.count; ++index)
        {
            reach_windows_update_item *current = &model->update_list.updates[index];
            for (size_t previous_index = 0; previous_index < previous->count; ++previous_index)
            {
                const reach_windows_update_item *old = &previous->updates[previous_index];
                if (!identity_equal(&current->identity, &old->identity) ||
                    (old->state != REACH_WINDOWS_UPDATE_INSTALLED_REBOOT_REQUIRED &&
                     old->state != REACH_WINDOWS_UPDATE_FAILED))
                    continue;
                reach_windows_update_identity identity = current->identity;
                uint16_t categories[REACH_WINDOWS_UPDATE_METADATA_CAPACITY] = {};
                reach_copy_utf16(categories, REACH_WINDOWS_UPDATE_METADATA_CAPACITY,
                                 current->categories);
                *current = *old;
                current->identity = identity;
                reach_copy_utf16(current->categories, REACH_WINDOWS_UPDATE_METADATA_CAPACITY,
                                 categories);
                break;
            }
        }
        for (size_t previous_index = 0; previous_index < previous->count &&
                                        model->update_list.count < REACH_WINDOWS_UPDATE_MAX_UPDATES;
             ++previous_index)
        {
            const reach_windows_update_item *old = &previous->updates[previous_index];
            if (old->state != REACH_WINDOWS_UPDATE_INSTALLED_REBOOT_REQUIRED)
                continue;
            int32_t found = 0;
            for (size_t index = 0; index < model->update_list.count; ++index)
                if (identity_equal(&model->update_list.updates[index].identity, &old->identity))
                {
                    found = 1;
                    break;
                }
            if (!found)
                model->update_list.updates[model->update_list.count++] = *old;
        }
    }
    model->update_page_state = REACH_SETTINGS_UPDATE_AVAILABLE;
}

void reach_settings_model_begin_update_install(reach_settings_model *model)
{
    if (model == nullptr || reach_settings_model_update_busy(model) ||
        reach_settings_model_selected_update_count(model) == 0)
        return;
    model->update_page_state = REACH_SETTINGS_UPDATE_PREPARING;
}

static int identity_equal(const reach_windows_update_identity *left,
                          const reach_windows_update_identity *right)
{
    if (left->revision_number != right->revision_number)
        return 0;
    for (size_t index = 0; index < REACH_WINDOWS_UPDATE_ID_CAPACITY; ++index)
    {
        if (left->update_id[index] != right->update_id[index])
            return 0;
        if (left->update_id[index] == 0)
            return 1;
    }
    return 1;
}

void reach_settings_model_apply_update_operation(
    reach_settings_model *model, const reach_windows_update_operation_result *result)
{
    if (model == nullptr || result == nullptr)
        return;
    if (model->update_list.count == 0)
    {
        model->update_list.count = result->per_update_result_count;
        if (model->update_list.count > REACH_WINDOWS_UPDATE_MAX_UPDATES)
            model->update_list.count = REACH_WINDOWS_UPDATE_MAX_UPDATES;
        for (size_t index = 0; index < model->update_list.count; ++index)
            model->update_list.updates[index] = result->per_update_results[index];
    }
    for (size_t result_index = 0; result_index < result->per_update_result_count; ++result_index)
    {
        const reach_windows_update_item *source = &result->per_update_results[result_index];
        for (size_t update_index = 0; update_index < model->update_list.count; ++update_index)
            if (identity_equal(&source->identity,
                               &model->update_list.updates[update_index].identity))
            {
                uint16_t selected_reason[REACH_WINDOWS_UPDATE_TEXT_CAPACITY] = {};
                reach_copy_utf16(selected_reason, REACH_WINDOWS_UPDATE_TEXT_CAPACITY,
                                 model->update_list.updates[update_index].selected_reason);
                model->update_list.updates[update_index] = *source;
                if (model->update_list.updates[update_index].selected_reason[0] == 0)
                    reach_copy_utf16(model->update_list.updates[update_index].selected_reason,
                                     REACH_WINDOWS_UPDATE_TEXT_CAPACITY, selected_reason);
            }
    }
    if (result->failure_class != REACH_WINDOWS_UPDATE_FAILURE_NONE)
        model->update_page_state = REACH_SETTINGS_UPDATE_ERROR;
    else
        model->update_page_state = REACH_SETTINGS_UPDATE_COMPLETE;
}

void reach_settings_model_toggle_update(reach_settings_model *model, size_t index)
{
    if (model == nullptr || reach_settings_model_update_busy(model) ||
        index >= model->update_list.count)
        return;
    reach_windows_update_item *update = &model->update_list.updates[index];
    if (update->state != REACH_WINDOWS_UPDATE_DISCOVERED &&
        update->state != REACH_WINDOWS_UPDATE_SELECTED)
        return;
    update->selected = !update->selected;
    update->state =
        update->selected ? REACH_WINDOWS_UPDATE_SELECTED : REACH_WINDOWS_UPDATE_DISCOVERED;
    if (update->selected)
        reach_copy_utf16(update->selected_reason, REACH_WINDOWS_UPDATE_TEXT_CAPACITY,
                         (const uint16_t *)u"Manual");
    else
        update->selected_reason[0] = 0;
}

size_t reach_settings_model_selected_update_count(const reach_settings_model *model)
{
    if (model == nullptr)
        return 0;
    size_t count = 0;
    for (size_t index = 0; index < model->update_list.count; ++index)
        if (model->update_list.updates[index].selected &&
            model->update_list.updates[index].state == REACH_WINDOWS_UPDATE_SELECTED)
            ++count;
    return count;
}

size_t reach_settings_model_restart_required_count(const reach_settings_model *model)
{
    if (model == nullptr)
        return 0;
    size_t count = 0;
    for (size_t index = 0; index < model->update_list.count; ++index)
        if (model->update_list.updates[index].state ==
            REACH_WINDOWS_UPDATE_INSTALLED_REBOOT_REQUIRED)
            ++count;
    return count;
}

int32_t reach_settings_model_update_busy(const reach_settings_model *model)
{
    return model != nullptr && (model->update_page_state == REACH_SETTINGS_UPDATE_SCANNING ||
                                model->update_page_state == REACH_SETTINGS_UPDATE_PREPARING ||
                                model->update_page_state == REACH_SETTINGS_UPDATE_DOWNLOADING ||
                                model->update_page_state == REACH_SETTINGS_UPDATE_INSTALLING ||
                                model->update_page_state == REACH_SETTINGS_UPDATE_VERIFYING);
}

void reach_settings_model_scroll_updates(reach_settings_model *model, float delta)
{
    if (model != nullptr)
        reach_scrollbar_scroll(&model->update_scrollbar, delta);
}

int32_t reach_settings_model_update_scroll(reach_settings_model *model, double delta_seconds)
{
    return model != nullptr ? reach_scrollbar_update(&model->update_scrollbar, delta_seconds) : 0;
}
