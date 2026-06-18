#include "reach/features/settings.h"
#include "reach/features/windows_update.h"

#include <stdio.h>

static int failures = 0;
static void expect_true(int value, const char *message)
{
    if (!value) { ++failures; printf("FAIL: %s\n", message); }
}
static void copy_ascii(uint16_t *destination, size_t capacity, const char *source)
{
    size_t index = 0;
    while (source[index] != 0 && index + 1 < capacity)
    {
        destination[index] = (uint16_t)(unsigned char)source[index];
        ++index;
    }
    destination[index] = 0;
}
static int equals_ascii(const uint16_t *value, const char *expected)
{
    size_t index = 0;
    while (expected[index] != 0 && value[index] == (uint16_t)(unsigned char)expected[index]) ++index;
    return expected[index] == 0 && value[index] == 0;
}
static reach_windows_update_list sample_updates(size_t count)
{
    reach_windows_update_list updates = {};
    updates.count = count;
    for (size_t index = 0; index < count; ++index)
    {
        copy_ascii(updates.updates[index].identity.update_id, REACH_WINDOWS_UPDATE_ID_CAPACITY,
                   index == 0 ? "security-id" : "optional-id");
        updates.updates[index].identity.revision_number = (int32_t)index + 1;
        copy_ascii(updates.updates[index].identity.title, REACH_WINDOWS_UPDATE_TEXT_CAPACITY,
                   index == 0 ? "2026-06 Cumulative Update for Windows 11"
                              : "Optional language feature");
        copy_ascii(updates.updates[index].identity.kb_article_ids,
                   REACH_WINDOWS_UPDATE_METADATA_CAPACITY, "KB123456");
        copy_ascii(updates.updates[index].categories, REACH_WINDOWS_UPDATE_METADATA_CAPACITY,
                   index == 0 ? "Products; Security Updates" : "Feature Packs");
    }
    return updates;
}
static void test_policy(void)
{
    reach_windows_update_list updates = sample_updates(2);
    reach_windows_update_apply_default_selection(&updates);
    expect_true(updates.updates[0].selected, "security-maintenance update is selected");
    expect_true(!updates.updates[1].selected, "unrelated update is not selected");
    expect_true(equals_ascii(updates.updates[0].selected_reason, "SecurityMaintenance"),
                "selected reason is recorded");
    expect_true(equals_ascii(reach_windows_update_state_label(
                                 REACH_WINDOWS_UPDATE_INSTALLED_REBOOT_REQUIRED),
                             "Installed - restart required"),
                "reboot-required is not labelled failed");
    expect_true(equals_ascii(reach_windows_update_failure_label(REACH_WINDOWS_UPDATE_NOT_ELEVATED),
                             "Administrator permission is required to install Windows updates."),
                "not-elevated message is exact");
}
static void test_model_and_interaction(void)
{
    reach_settings_model model = {};
    reach_settings_model_init(&model);
    expect_true(model.update_page_state == REACH_SETTINGS_UPDATE_NOT_SCANNED,
                "initial state is not scanned");
    expect_true(equals_ascii(model.update_status, "No scan has been run yet."),
                "initial idle status is neutral");
    expect_true(reach_settings_model_selected_update_count(&model) == 0,
                "install starts disabled");

    reach_settings_model_begin_update_scan(&model);
    expect_true(model.update_page_state == REACH_SETTINGS_UPDATE_SCANNING,
                "scan enters scanning state");
    reach_windows_update_list updates = sample_updates(2);
    reach_settings_model_apply_update_scan(&model, &updates, 0);
    expect_true(model.update_page_state == REACH_SETTINGS_UPDATE_AVAILABLE,
                "successful scan enters available state");
    expect_true(reach_settings_model_selected_update_count(&model) == 1,
                "security update is selected by default");
    reach_settings_model_toggle_update(&model, 1);
    expect_true(reach_settings_model_selected_update_count(&model) == 2,
                "independent checkbox supports multiple selection");
    expect_true(equals_ascii(model.update_list.updates[1].selected_reason, "Manual"),
                "manual checkbox reason is recorded");
    reach_settings_model_toggle_update(&model, 0);
    expect_true(!model.update_list.updates[0].selected, "selected update can be deselected");

    reach_settings_layout layout = reach_settings_layout_for_bounds({0, 0, 900, 650}, nullptr, 1);
    reach_settings_hit_result hit = reach_settings_hit_test(
        &layout, layout.update_search_button.x + 2, layout.update_search_button.y + 2);
    expect_true(hit.type == REACH_SETTINGS_HIT_UPDATE_SEARCH, "search button is interactive");
    hit = reach_settings_hit_test(&layout, layout.update_install_button.x + 2,
                                  layout.update_install_button.y + 2);
    expect_true(hit.type == REACH_SETTINGS_HIT_UPDATE_INSTALL, "install button is interactive");
    hit = reach_settings_hit_test(&layout, layout.update_checkboxes[0].x + 2,
                                  layout.update_checkboxes[0].y + 2);
    expect_true(hit.type == REACH_SETTINGS_HIT_UPDATE_CHECKBOX && hit.update_index == 0,
                "update checkbox identifies its row");
}
static void test_scroll_and_operation_states(void)
{
    reach_settings_model model = {};
    reach_settings_model_init(&model);
    model.update_list = sample_updates(12);
    reach_settings_model_scroll_updates(&model, 3, 5);
    expect_true(model.update_scroll_offset == 3, "update list scrolls down");
    reach_settings_model_scroll_updates(&model, -2, 5);
    expect_true(model.update_scroll_offset == 1, "update list scrolls up");

    reach_windows_update_operation_result result = {};
    result.overall_reboot_required = 1;
    result.per_update_result_count = 1;
    result.per_update_results[0] = model.update_list.updates[0];
    result.per_update_results[0].state = REACH_WINDOWS_UPDATE_INSTALLED_REBOOT_REQUIRED;
    reach_settings_model_apply_update_operation(&model, &result);
    expect_true(equals_ascii(model.update_status, "Installed - restart required"),
                "successful reboot-required install is not failed");

    result = {};
    result.failure_class = REACH_WINDOWS_UPDATE_NOT_ELEVATED;
    result.overall_install_hresult = (int32_t)0x80240044u;
    reach_settings_model_apply_update_operation(&model, &result);
    expect_true(model.update_page_state == REACH_SETTINGS_UPDATE_ERROR,
                "not elevated is structured as an error state");
    expect_true(equals_ascii(model.update_status,
                             "Administrator permission is required to install Windows updates."),
                "not elevated status explains required action");
}
static void test_multiple_update_results(void)
{
    reach_settings_model model = {};
    reach_settings_model_init(&model);
    model.update_list = sample_updates(2);
    reach_windows_update_operation_result operation = {};
    operation.per_update_result_count = 2;
    operation.per_update_results[0] = model.update_list.updates[0];
    operation.per_update_results[0].state = REACH_WINDOWS_UPDATE_VERIFIED_INSTALLED;
    operation.per_update_results[1] = model.update_list.updates[1];
    operation.per_update_results[1].state = REACH_WINDOWS_UPDATE_FAILED;
    operation.per_update_results[1].failure_class = REACH_WINDOWS_UPDATE_DOWNLOAD_FAILED;
    operation.failure_class = REACH_WINDOWS_UPDATE_DOWNLOAD_FAILED;
    reach_settings_model_apply_update_operation(&model, &operation);
    expect_true(model.update_list.updates[0].state == REACH_WINDOWS_UPDATE_VERIFIED_INSTALLED,
                "first update keeps its independent successful result");
    expect_true(model.update_list.updates[1].state == REACH_WINDOWS_UPDATE_FAILED,
                "second update keeps its independent failed result");
}
static void test_update_page_rendering(void)
{
    reach_settings_model model = {};
    reach_settings_model_init(&model);
    model.selected_page = REACH_SETTINGS_PAGE_UPDATE;
    reach_windows_update_list updates = sample_updates(2);
    reach_settings_model_apply_update_scan(&model, &updates, 0);
    reach_theme theme = {};
    theme.dark_background = {0.05f, 0.05f, 0.05f, 1.0f};
    theme.dark_text = {1, 1, 1, 1};
    theme.settings_text = {1, 1, 1, 1};
    theme.settings_secondary_text = {0.7f, 0.7f, 0.7f, 1};
    reach_settings_layout layout = reach_settings_layout_for_bounds({0, 0, 900, 650}, &theme, 1);
    reach_settings_render_input input = {};
    input.theme = &theme;
    input.model = &model;
    input.layout = &layout;
    input.dpi_scale = 1;
    input.text_alignment_leading = REACH_TEXT_ALIGNMENT_LEADING;
    reach_render_command_buffer commands = {};
    expect_true(reach_settings_build_render_commands(&input, &commands) == REACH_OK,
                "update page renders successfully");
    int found_search = 0, found_install = 0, found_title = 0, found_metadata = 0;
    for (size_t index = 0; index < commands.count; ++index)
    {
        const reach_render_command *command = &commands.commands[index];
        if (equals_ascii(command->text, "Search for updates")) found_search = 1;
        if (equals_ascii(command->text, "Install selected")) found_install = 1;
        if (equals_ascii(command->text, "2026-06 Cumulative Update for Windows 11")) found_title = 1;
        if (command->text[0] == (uint16_t)'K' && command->text[1] == (uint16_t)'B' &&
            command->text[2] == (uint16_t)':') found_metadata = 1;
    }
    expect_true(found_search && found_install, "top-bar actions are rendered");
    expect_true(found_title && found_metadata, "update title and metadata are rendered");
}
int main(void)
{
    test_policy();
    test_model_and_interaction();
    test_scroll_and_operation_states();
    test_multiple_update_results();
    test_update_page_rendering();
    return failures == 0 ? 0 : 1;
}
