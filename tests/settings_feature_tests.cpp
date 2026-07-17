#include "reach/apps/settings/settings.h"

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
static void test_navigation_pages(void)
{
    std::unique_ptr<reach_settings_model> model(new reach_settings_model());
    reach_settings_model_init(model.get());

    size_t nav_count = 0;
    const reach_settings_nav_item *nav = reach_settings_nav_items(&nav_count);
    expect_true(nav_count == REACH_SETTINGS_NAV_ITEM_COUNT, "settings exposes all nav items");
    expect_true(nav[0].page == REACH_SETTINGS_PAGE_WIFI && equals_ascii(nav[0].label, "Wi-Fi"),
                "first nav item is Wi-Fi");
    expect_true(nav[6].page == REACH_SETTINGS_PAGE_UPDATE &&
                    equals_ascii(nav[6].label, "Windows Updates"),
                "last nav item is Windows Updates");

    reach_settings_model_select_page(model.get(), REACH_SETTINGS_PAGE_UPDATE);
    expect_true(model->selected_page == REACH_SETTINGS_PAGE_UPDATE,
                "valid page selection updates model");
    reach_settings_model_select_page(model.get(), (reach_settings_page)99);
    expect_true(model->selected_page == REACH_SETTINGS_PAGE_UPDATE,
                "invalid page selection is ignored");
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
static void test_power_timers(void)
{
    std::unique_ptr<reach_settings_model> model(new reach_settings_model());
    reach_settings_model_init(model.get());

    expect_true(reach_settings_model_power_minutes(model.get(),
                                                   REACH_SETTINGS_POWER_TIMER_SLEEP) == 30,
                "sleep timer defaults to 30 minutes");
    expect_true(reach_settings_power_option_minutes(REACH_SETTINGS_POWER_TIMER_SLEEP, 0) == 0,
                "first option of every timer is never");
    expect_true(equals_ascii(
                    reach_settings_power_option_label(REACH_SETTINGS_POWER_TIMER_SHUTDOWN, 0),
                    "Never"),
                "never option is labelled");

    reach_settings_model_set_power_minutes(model.get(), REACH_SETTINGS_POWER_TIMER_SLEEP, 45);
    expect_true(model->power_selected[REACH_SETTINGS_POWER_TIMER_SLEEP] ==
                    REACH_SETTINGS_POWER_CUSTOM_OPTION,
                "off-preset config value selects the custom option");
    expect_true(equals_ascii(model->power_custom_edits[REACH_SETTINGS_POWER_TIMER_SLEEP]
                                                      [REACH_SETTINGS_POWER_FIELD_HOURS]
                                                          .text,
                             "0") &&
                    equals_ascii(model->power_custom_edits[REACH_SETTINGS_POWER_TIMER_SLEEP]
                                                          [REACH_SETTINGS_POWER_FIELD_MINUTES]
                                                              .text,
                                 "45"),
                "off-preset config value seeds the custom hour and minute boxes");
    expect_true(reach_settings_model_power_minutes(model.get(),
                                                   REACH_SETTINGS_POWER_TIMER_SLEEP) == 45,
                "raw config value is preserved");
    expect_true(!reach_settings_model_power_animations_active(model.get()),
                "seeding from config does not animate");

    reach_settings_model_select_power_option(model.get(), REACH_SETTINGS_POWER_TIMER_LOCK, 2);
    expect_true(reach_settings_model_power_minutes(model.get(),
                                                   REACH_SETTINGS_POWER_TIMER_LOCK) == 5,
                "selecting an option applies its minutes");
    expect_true(reach_settings_model_power_animations_active(model.get()),
                "selecting an option starts the cross-fade animation");
    expect_true(reach_settings_model_tick_power_animations(model.get(), 1.0),
                "animation tick reports activity");
    expect_true(!reach_settings_model_power_animations_active(model.get()),
                "animation completes after its duration");
}
static void test_power_apply_tracking(void)
{
    std::unique_ptr<reach_settings_model> model(new reach_settings_model());
    reach_settings_model_init(model.get());

    expect_true(!reach_settings_model_power_dirty(model.get()),
                "freshly initialized model has nothing to apply");
    reach_settings_model_select_power_option(model.get(), REACH_SETTINGS_POWER_TIMER_SLEEP, 1);
    expect_true(reach_settings_model_power_dirty(model.get()),
                "changing an option marks the page dirty");
    reach_settings_model_power_mark_applied(model.get());
    expect_true(!reach_settings_model_power_dirty(model.get()),
                "applying clears the dirty state");
    reach_settings_model_select_power_option(model.get(), REACH_SETTINGS_POWER_TIMER_SLEEP, 0);
    reach_settings_model_select_power_option(model.get(), REACH_SETTINGS_POWER_TIMER_SLEEP, 1);
    expect_true(!reach_settings_model_power_dirty(model.get()),
                "returning to the applied value clears the dirty state");
}
static void test_power_custom_textbox(void)
{
    std::unique_ptr<reach_settings_model> model(new reach_settings_model());
    reach_settings_model_init(model.get());

    expect_true(!reach_settings_model_power_insert_char(model.get(), (uint16_t)'7'),
                "typing without focus is ignored");
    reach_settings_model_power_focus_custom(model.get(), REACH_SETTINGS_POWER_TIMER_SLEEP,
                                            REACH_SETTINGS_POWER_FIELD_MINUTES);
    expect_true(model->power_focused_timer == REACH_SETTINGS_POWER_TIMER_SLEEP &&
                    model->power_focused_field == REACH_SETTINGS_POWER_FIELD_MINUTES,
                "clicking a custom box focuses it");
    expect_true(model->power_selected[REACH_SETTINGS_POWER_TIMER_SLEEP] ==
                    REACH_SETTINGS_POWER_CUSTOM_OPTION,
                "focusing a custom box selects the custom option");
    expect_true(equals_ascii(model->power_custom_edits[REACH_SETTINGS_POWER_TIMER_SLEEP]
                                                      [REACH_SETTINGS_POWER_FIELD_HOURS]
                                                          .text,
                             "0") &&
                    equals_ascii(model->power_custom_edits[REACH_SETTINGS_POWER_TIMER_SLEEP]
                                                          [REACH_SETTINGS_POWER_FIELD_MINUTES]
                                                              .text,
                                 "30"),
                "custom boxes are seeded from the current minutes");

    expect_true(!reach_settings_model_power_insert_char(model.get(), (uint16_t)'x'),
                "non-digit characters are rejected");
    expect_true(reach_settings_model_power_insert_char(model.get(), (uint16_t)'7'),
                "digits replace the seeded selection");
    expect_true(reach_settings_model_power_insert_char(model.get(), (uint16_t)'5'),
                "digits append at the caret");
    expect_true(reach_settings_model_power_minutes(model.get(),
                                                   REACH_SETTINGS_POWER_TIMER_SLEEP) == 75,
                "typed minutes update the pending value");
    expect_true(reach_settings_model_power_dirty(model.get()),
                "typed custom minutes mark the page dirty");

    reach_text_edit_modifiers no_mods = {};
    expect_true(reach_settings_model_power_handle_edit_key(model.get(),
                                                           REACH_TEXT_EDIT_KEY_BACKSPACE, no_mods),
                "backspace edits the focused box");
    expect_true(reach_settings_model_power_minutes(model.get(),
                                                   REACH_SETTINGS_POWER_TIMER_SLEEP) == 7,
                "backspace re-parses the pending minutes");

    reach_settings_model_power_focus_custom(model.get(), REACH_SETTINGS_POWER_TIMER_SLEEP,
                                            REACH_SETTINGS_POWER_FIELD_HOURS);
    expect_true(reach_settings_model_power_insert_char(model.get(), (uint16_t)'2'),
                "hour box accepts digits");
    expect_true(reach_settings_model_power_minutes(model.get(),
                                                   REACH_SETTINGS_POWER_TIMER_SLEEP) ==
                    2 * 60 + 7,
                "hours and minutes combine into the pending value");

    reach_settings_model_power_blur(model.get());
    expect_true(model->power_focused_timer == -1, "blur releases focus");
    expect_true(equals_ascii(model->power_custom_edits[REACH_SETTINGS_POWER_TIMER_SLEEP]
                                                      [REACH_SETTINGS_POWER_FIELD_HOURS]
                                                          .text,
                             "2") &&
                    equals_ascii(model->power_custom_edits[REACH_SETTINGS_POWER_TIMER_SLEEP]
                                                          [REACH_SETTINGS_POWER_FIELD_MINUTES]
                                                              .text,
                                 "7"),
                "blur keeps the normalized hour and minute values");
}

static void test_power_wait_apps_toggle(void)
{
    std::unique_ptr<reach_settings_model> model(new reach_settings_model());
    reach_settings_model_init(model.get());

    expect_true(reach_settings_power_timer_supports_wait(REACH_SETTINGS_POWER_TIMER_SLEEP) &&
                    reach_settings_power_timer_supports_wait(REACH_SETTINGS_POWER_TIMER_SHUTDOWN) &&
                    reach_settings_power_timer_supports_wait(REACH_SETTINGS_POWER_TIMER_RESTART),
                "sleep, shutdown, and restart offer the wait-for-apps toggle");
    expect_true(!reach_settings_power_timer_supports_wait(REACH_SETTINGS_POWER_TIMER_LOCK),
                "lock has no wait-for-apps toggle");
    expect_true(!reach_settings_model_power_wait_apps(model.get(),
                                                      REACH_SETTINGS_POWER_TIMER_SLEEP),
                "wait-for-apps defaults off");

    expect_true(reach_settings_model_toggle_power_wait_apps(model.get(),
                                                            REACH_SETTINGS_POWER_TIMER_SLEEP),
                "toggling a supported row is accepted");
    expect_true(reach_settings_model_power_wait_apps(model.get(),
                                                     REACH_SETTINGS_POWER_TIMER_SLEEP) == 1,
                "toggling turns the flag on");
    expect_true(reach_settings_model_power_animations_active(model.get()),
                "toggling animates the switch");
    expect_true(reach_settings_model_tick_power_animations(model.get(), 1.0),
                "toggle animation tick reports activity");
    expect_true(!reach_settings_model_power_animations_active(model.get()),
                "toggle animation completes after its duration");
    expect_true(reach_settings_model_power_dirty(model.get()),
                "toggling marks the page dirty");
    reach_settings_model_power_mark_applied(model.get());
    expect_true(!reach_settings_model_power_dirty(model.get()),
                "applying clears the wait-for-apps dirty state");
    expect_true(reach_settings_model_toggle_power_wait_apps(model.get(),
                                                            REACH_SETTINGS_POWER_TIMER_SLEEP) &&
                    reach_settings_model_toggle_power_wait_apps(
                        model.get(), REACH_SETTINGS_POWER_TIMER_SLEEP) &&
                    !reach_settings_model_power_dirty(model.get()),
                "toggling twice returns to the applied state");

    expect_true(!reach_settings_model_toggle_power_wait_apps(model.get(),
                                                             REACH_SETTINGS_POWER_TIMER_LOCK),
                "toggling the lock row is rejected");
    reach_settings_model_set_power_wait_apps(model.get(), REACH_SETTINGS_POWER_TIMER_LOCK, 1);
    expect_true(!reach_settings_model_power_wait_apps(model.get(),
                                                      REACH_SETTINGS_POWER_TIMER_LOCK),
                "setting the lock row wait flag is ignored");
}

int main(void)
{
    test_policy();
    test_required_security_maintenance_classifications();
    test_navigation_pages();
    test_model_and_interaction();
    test_scroll_and_operation_states();
    test_multiple_update_results();
    test_power_timers();
    test_power_apply_tracking();
    test_power_custom_textbox();
    test_power_wait_apps_toggle();
    return failures == 0 ? 0 : 1;
}
