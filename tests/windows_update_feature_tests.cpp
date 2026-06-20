#include "reach/features/settings.h"
#include "reach/features/windows_update.h"

#include <memory>
#include <stdio.h>

static int failures = 0;
static void expect_true(int value, const char *message)
{
    if (!value)
    {
        ++failures;
        printf("FAIL: %s\n", message);
    }
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
    while (expected[index] != 0 && value[index] == (uint16_t)(unsigned char)expected[index])
        ++index;
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
}

static void test_required_security_maintenance_classifications(void)
{
    static const char *kb_ids[] = {"890830", "KB5007651", "2267602"};
    for (const char *kb_id : kb_ids)
    {
        reach_windows_update_item update = {};
        copy_ascii(update.identity.title, REACH_WINDOWS_UPDATE_TEXT_CAPACITY,
                   "Update with no classification metadata");
        copy_ascii(update.identity.kb_article_ids, REACH_WINDOWS_UPDATE_METADATA_CAPACITY, kb_id);
        expect_true(reach_windows_update_matches_security_maintenance(&update),
                    "required security-maintenance KB is selected explicitly");
    }

    reach_windows_update_item os_security_update = {};
    copy_ascii(os_security_update.identity.title, REACH_WINDOWS_UPDATE_TEXT_CAPACITY,
               "2026-06 Security Update");
    copy_ascii(os_security_update.identity.kb_article_ids, REACH_WINDOWS_UPDATE_METADATA_CAPACITY,
               "5094126");
    expect_true(reach_windows_update_matches_security_maintenance(&os_security_update),
                "OS security update is selected by title");

    reach_windows_update_item unrelated = {};
    copy_ascii(unrelated.identity.title, REACH_WINDOWS_UPDATE_TEXT_CAPACITY, "Optional update");
    copy_ascii(unrelated.identity.kb_article_ids, REACH_WINDOWS_UPDATE_METADATA_CAPACITY,
               "1890830, 50076510, 12267602");
    expect_true(!reach_windows_update_matches_security_maintenance(&unrelated),
                "partial KB matches are rejected");
}
static void test_model_and_interaction(void)
{
    std::unique_ptr<reach_settings_model> model(new reach_settings_model());
    std::unique_ptr<reach_windows_update_list> updates(
        new reach_windows_update_list(sample_updates(2)));
    reach_settings_model_init(model.get());
    expect_true(model->update_page_state == REACH_SETTINGS_UPDATE_NOT_SCANNED,
                "initial state is not scanned");
    expect_true(reach_settings_model_selected_update_count(model.get()) == 0,
                "install starts disabled");

    reach_settings_model_begin_update_scan(model.get());
    expect_true(model->update_page_state == REACH_SETTINGS_UPDATE_SCANNING,
                "scan enters scanning state");
    reach_settings_model_apply_update_scan(model.get(), updates.get(), 0);
    expect_true(model->update_page_state == REACH_SETTINGS_UPDATE_AVAILABLE,
                "successful scan enters available state");
    expect_true(reach_settings_model_selected_update_count(model.get()) == 1,
                "security update is selected by default");
    reach_settings_model_toggle_update(model.get(), 1);
    expect_true(reach_settings_model_selected_update_count(model.get()) == 2,
                "independent checkbox supports multiple selection");
    expect_true(equals_ascii(model->update_list.updates[1].selected_reason, "Manual"),
                "manual checkbox reason is recorded");
    reach_settings_model_toggle_update(model.get(), 0);
    expect_true(!model->update_list.updates[0].selected, "selected update can be deselected");

    reach_settings_layout layout =
        reach_settings_layout_for_bounds({0, 0, 900, 650}, nullptr, 1, model.get());
    reach_settings_hit_result hit = reach_settings_hit_test(
        &layout, layout.update_refresh_button.x + 2, layout.update_refresh_button.y + 2);
    expect_true(hit.type == REACH_SETTINGS_HIT_UPDATE_REFRESH, "refresh button is interactive");
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
    std::unique_ptr<reach_settings_model> model(new reach_settings_model());
    std::unique_ptr<reach_windows_update_operation_result> result(
        new reach_windows_update_operation_result());
    reach_settings_model_init(model.get());
    model->update_list = sample_updates(12);
    reach_scrollbar_set_extents(&model->update_scrollbar, 600.0f, 100.0f);
    reach_settings_model_scroll_updates(model.get(), 120.0f);
    expect_true(model->update_scrollbar.target == 120.0f, "update list scrolls down");
    reach_settings_model_scroll_updates(model.get(), -40.0f);
    expect_true(model->update_scrollbar.target == 80.0f, "update list scrolls up");

    result->overall_reboot_required = 1;
    result->per_update_result_count = 1;
    result->per_update_results[0] = model->update_list.updates[0];
    result->per_update_results[0].state = REACH_WINDOWS_UPDATE_INSTALLED_REBOOT_REQUIRED;
    reach_settings_model_apply_update_operation(model.get(), result.get());
    *result = {};
    result->failure_class = REACH_WINDOWS_UPDATE_NOT_ELEVATED;
    result->overall_install_hresult = (int32_t)0x80240044u;
    reach_settings_model_apply_update_operation(model.get(), result.get());
    expect_true(model->update_page_state == REACH_SETTINGS_UPDATE_ERROR,
                "not elevated is structured as an error state");
}
static void test_multiple_update_results(void)
{
    std::unique_ptr<reach_settings_model> model(new reach_settings_model());
    std::unique_ptr<reach_windows_update_operation_result> operation(
        new reach_windows_update_operation_result());
    reach_settings_model_init(model.get());
    model->update_list = sample_updates(2);
    operation->per_update_result_count = 2;
    operation->per_update_results[0] = model->update_list.updates[0];
    operation->per_update_results[0].state = REACH_WINDOWS_UPDATE_VERIFIED_INSTALLED;
    operation->per_update_results[1] = model->update_list.updates[1];
    operation->per_update_results[1].state = REACH_WINDOWS_UPDATE_FAILED;
    operation->per_update_results[1].failure_class = REACH_WINDOWS_UPDATE_DOWNLOAD_FAILED;
    operation->failure_class = REACH_WINDOWS_UPDATE_DOWNLOAD_FAILED;
    reach_settings_model_apply_update_operation(model.get(), operation.get());
    expect_true(model->update_list.updates[0].state == REACH_WINDOWS_UPDATE_VERIFIED_INSTALLED,
                "first update keeps its independent successful result");
    expect_true(model->update_list.updates[1].state == REACH_WINDOWS_UPDATE_FAILED,
                "second update keeps its independent failed result");
}
static void test_update_page_rendering(void)
{
    std::unique_ptr<reach_settings_model> model(new reach_settings_model());
    std::unique_ptr<reach_windows_update_list> updates(
        new reach_windows_update_list(sample_updates(2)));
    std::unique_ptr<reach_render_command_buffer> commands(new reach_render_command_buffer());
    reach_settings_model_init(model.get());
    model->selected_page = REACH_SETTINGS_PAGE_UPDATE;
    reach_settings_model_apply_update_scan(model.get(), updates.get(), 0);
    reach_theme theme = {};
    theme.dark_background = {0.05f, 0.05f, 0.05f, 1.0f};
    theme.dark_text = {1, 1, 1, 1};
    theme.settings_text = {1, 1, 1, 1};
    theme.settings_secondary_text = {0.7f, 0.7f, 0.7f, 1};
    reach_settings_layout layout =
        reach_settings_layout_for_bounds({0, 0, 900, 650}, &theme, 1, model.get());
    reach_settings_render_input input = {};
    input.theme = &theme;
    input.model = model.get();
    input.layout = &layout;
    input.dpi_scale = 1;
    input.text_alignment_leading = REACH_TEXT_ALIGNMENT_LEADING;
    expect_true(reach_settings_build_render_commands(&input, commands.get()) == REACH_OK,
                "update page renders successfully");
    int found_search = 0, found_install = 0, found_title = 0, found_metadata = 0;
    for (size_t index = 0; index < commands->count; ++index)
    {
        const reach_render_command *command = &commands->commands[index];
        if (equals_ascii(command->text, "Refresh"))
            found_search = 1;
        if (equals_ascii(command->text, "Install selected"))
            found_install = 1;
        if (equals_ascii(command->text, "2026-06 Cumulative Update for Windows 11"))
            found_title = 1;
        if (command->text[0] == (uint16_t)'K' && command->text[1] == (uint16_t)'B' &&
            command->text[2] == (uint16_t)':')
            found_metadata = 1;
    }
    expect_true(found_search && found_install, "top-bar actions are rendered");
    expect_true(found_title && found_metadata, "update title and metadata are rendered");
}
int main(void)
{
    test_policy();
    test_required_security_maintenance_classifications();
    test_model_and_interaction();
    test_scroll_and_operation_states();
    test_multiple_update_results();
    test_update_page_rendering();
    return failures == 0 ? 0 : 1;
}
