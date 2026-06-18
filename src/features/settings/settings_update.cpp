#include "settings_pages_internal.h"

#include "reach/features/windows_update.h"
#include "reach/support/util.h"

const uint16_t *reach_settings_update_page_title(void)
{
    return (const uint16_t *)u"Windows Updates";
}

const uint16_t *reach_settings_update_page_placeholder(void)
{
    return (const uint16_t *)u"No scan has been run yet.";
}

static void set_status(reach_settings_model *model, const uint16_t *status)
{
    reach_copy_utf16(model->update_status, REACH_WINDOWS_UPDATE_TEXT_CAPACITY, status);
}

void reach_settings_model_begin_update_scan(reach_settings_model *model)
{
    if (model == nullptr || reach_settings_model_update_busy(model))
        return;
    model->update_page_state = REACH_SETTINGS_UPDATE_SCANNING;
    model->update_list = {};
    model->update_scroll_offset = 0;
    model->update_operation_hresult = 0;
    set_status(model, (const uint16_t *)u"Scanning for available software updates...");
}

void reach_settings_model_apply_update_scan(reach_settings_model *model,
                                            const reach_windows_update_list *updates,
                                            int32_t hresult)
{
    if (model == nullptr)
        return;
    model->update_operation_hresult = hresult;
    if (updates == nullptr || hresult < 0)
    {
        model->update_page_state = REACH_SETTINGS_UPDATE_ERROR;
        set_status(model, (const uint16_t *)u"Windows Update scan failed.");
        return;
    }
    model->update_list = *updates;
    if (model->update_list.count > REACH_WINDOWS_UPDATE_MAX_UPDATES)
        model->update_list.count = REACH_WINDOWS_UPDATE_MAX_UPDATES;
    reach_windows_update_apply_default_selection(&model->update_list);
    model->update_page_state = REACH_SETTINGS_UPDATE_AVAILABLE;
    set_status(model, model->update_list.count == 0
                          ? (const uint16_t *)u"No available software updates were found."
                          : (const uint16_t *)u"Available updates");
}

void reach_settings_model_begin_update_install(reach_settings_model *model)
{
    if (model == nullptr || reach_settings_model_update_busy(model) ||
        reach_settings_model_selected_update_count(model) == 0)
        return;
    model->update_page_state = REACH_SETTINGS_UPDATE_PREPARING;
    set_status(model, (const uint16_t *)u"Re-scanning selected updates...");
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
    model->update_operation_hresult = result->overall_install_hresult;
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
    {
        model->update_page_state = REACH_SETTINGS_UPDATE_ERROR;
        set_status(model, reach_windows_update_failure_label(result->failure_class));
    }
    else if (result->overall_reboot_required)
    {
        model->update_page_state = REACH_SETTINGS_UPDATE_COMPLETE;
        set_status(model, (const uint16_t *)u"Installed - restart required");
    }
    else
    {
        model->update_page_state = REACH_SETTINGS_UPDATE_COMPLETE;
        set_status(model, (const uint16_t *)u"Verified installed");
    }
}

void reach_settings_model_toggle_update(reach_settings_model *model, size_t index)
{
    if (model == nullptr || reach_settings_model_update_busy(model) ||
        index >= model->update_list.count)
        return;
    reach_windows_update_item *update = &model->update_list.updates[index];
    if (update->state != REACH_WINDOWS_UPDATE_DISCOVERED &&
        update->state != REACH_WINDOWS_UPDATE_SELECTED &&
        update->state != REACH_WINDOWS_UPDATE_FAILED)
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
            (model->update_list.updates[index].state == REACH_WINDOWS_UPDATE_SELECTED ||
             model->update_list.updates[index].state == REACH_WINDOWS_UPDATE_FAILED))
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

void reach_settings_model_scroll_updates(reach_settings_model *model, int32_t delta,
                                         size_t visible_count)
{
    if (model == nullptr || visible_count == 0 || model->update_list.count <= visible_count)
        return;
    size_t maximum = model->update_list.count - visible_count;
    if (delta < 0)
    {
        size_t amount = (size_t)(-delta);
        model->update_scroll_offset =
            amount > model->update_scroll_offset ? 0 : model->update_scroll_offset - amount;
    }
    else
    {
        size_t next = model->update_scroll_offset + (size_t)delta;
        model->update_scroll_offset = next > maximum ? maximum : next;
    }
}
