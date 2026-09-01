#include "reach/core/render_commands.h"
#include "reach/core/typography.h"
#include "reach/apps/settings/settings.h"
#include "reach/features/common/loader_render.h"
#include "reach/features/common/scrollbar_render.h"
#include "reach/features/common/ui_controls.h"

#include "settings_pages_internal.h"
#include "settings_render_internal.h"

static void append_text(uint16_t *destination, size_t capacity, const uint16_t *source)
{
    size_t length = 0;
    while (length < capacity && destination[length] != 0)
        ++length;
    size_t index = 0;
    while (source != nullptr && source[index] != 0 && length + 1 < capacity)
        destination[length++] = source[index++];
    if (length < capacity)
        destination[length] = 0;
}

static void append_number(uint16_t *destination, size_t capacity, size_t value)
{
    uint16_t digits[24] = {};
    int32_t count = 0;
    do
    {
        digits[count++] = (uint16_t)(u'0' + (value % 10));
        value /= 10;
    } while (value != 0 && count < 23);
    for (int32_t index = count - 1; index >= 0; --index)
    {
        uint16_t single[2] = {digits[index], 0};
        append_text(destination, capacity, single);
    }
}

static reach_rect_f32 reach_settings_lerp_rect(reach_rect_f32 from, reach_rect_f32 to,
                                               float progress)
{
    return {from.x + (to.x - from.x) * progress, from.y + (to.y - from.y) * progress,
            from.width + (to.width - from.width) * progress,
            from.height + (to.height - from.height) * progress};
}

static void render_nav_selection_indicator(const reach_settings_render_input *input,
                                           reach_render_command_buffer *commands,
                                           const reach_settings_nav_item *items, size_t nav_count)
{
    if (input->layout->nav_item_count == 0 || nav_count == 0)
    {
        return;
    }

    size_t available_count =
        input->layout->nav_item_count < nav_count ? input->layout->nav_item_count : nav_count;
    float position = reach_animation_manager_value(&input->model->nav_selection_animation, 0);
    float maximum_position = (float)(available_count - 1);
    if (position < 0.0f)
    {
        position = 0.0f;
    }
    if (position > maximum_position)
    {
        position = maximum_position;
    }

    size_t from_index = (size_t)position;
    size_t to_index = from_index + 1 < available_count ? from_index + 1 : from_index;
    reach_rect_f32 indicator = reach_settings_lerp_rect(input->layout->nav_items[from_index].bounds,
                                                        input->layout->nav_items[to_index].bounds,
                                                        position - (float)from_index);

    reach_theme_accent accent = items[0].accent;
    for (size_t index = 0; index < available_count; ++index)
    {
        if (items[index].page == input->model->selected_page)
        {
            accent = items[index].accent;
            break;
        }
    }
    reach_color color = reach_settings_color_with_alpha(
        reach_theme_accent_color(input->theme, accent), input->theme->accent_tint_alpha);
    reach_settings_push_rect(commands, indicator,
                             reach_settings_scale(input, input->theme->radius_small), color);
}

static void build_metadata_text(const reach_windows_update_item *update, uint16_t *text,
                                size_t capacity)
{
    text[0] = 0;
    append_text(text, capacity, (const uint16_t *)u"KB: ");
    append_text(text, capacity,
                update->identity.kb_article_ids[0] != 0 ? update->identity.kb_article_ids
                                                        : (const uint16_t *)u"N/A");
}

static void build_status_text(const reach_windows_update_item *update, uint16_t *text,
                              size_t capacity)
{
    text[0] = 0;
    append_text(text, capacity, (const uint16_t *)u"Status: ");
    append_text(text, capacity, reach_windows_update_state_label(update->state));
    append_text(text, capacity, (const uint16_t *)u"   Downloaded: ");
    append_text(text, capacity,
                update->downloaded ? (const uint16_t *)u"Yes" : (const uint16_t *)u"No");
    append_text(text, capacity, (const uint16_t *)u"   Restart: ");
    append_text(text, capacity,
                !update->reboot_required_known ? (const uint16_t *)u"Unknown"
                : update->reboot_required      ? (const uint16_t *)u"Required"
                                               : (const uint16_t *)u"No");
}

static void push_avatar_image(reach_render_command_buffer *commands, reach_rect_f32 rect,
                              uint64_t icon_id)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_ICON;
    command.rect = rect;
    command.radius = rect.width * 0.5f;
    command.icon_id = icon_id;
    command.icon_crop_to_fill = 1;
    command.color = {1.0f, 1.0f, 1.0f, 1.0f};
    (void)reach_render_command_buffer_push(commands, &command);
}

static void render_account_page(const reach_settings_render_input *input,
                                reach_render_command_buffer *commands)
{
    const reach_settings_model *model = input->model;
    const reach_settings_layout *layout = input->layout;
    reach_color accent = reach_theme_accent_color(input->theme, REACH_THEME_ACCENT_TEAL);

    reach_settings_push_rect(commands, layout->account_card,
                             reach_settings_scale(input, input->theme->radius_small),
                             input->theme->settings_card_background);

    const reach_rect_f32 avatar = layout->account_avatar;
    reach_rect_f32 ring = {avatar.x - reach_settings_scale(input, 3.0f),
                           avatar.y - reach_settings_scale(input, 3.0f),
                           avatar.width + reach_settings_scale(input, 6.0f),
                           avatar.height + reach_settings_scale(input, 6.0f)};
    reach_color ring_color =
        reach_theme_color_mix(input->theme->settings_card_background, accent, 0.55f);
    reach_settings_push_bordered_background(commands, ring, ring.width * 0.5f,
                                            reach_settings_scale(input, 1.5f),
                                            input->theme->settings_card_background, ring_color);
    if (model->account_picture != 0)
    {
        push_avatar_image(commands, avatar, model->account_picture);
    }
    else
    {
        reach_settings_push_rect(commands, avatar, avatar.width * 0.5f,
                                 reach_settings_color_with_alpha(accent, 0.22f));
        uint16_t initial[2] = {reach_settings_account_initial(model), 0};
        reach_settings_push_text(
            commands, avatar, initial, reach_settings_scale(input, REACH_TEXT_SIZE_XLARGE),
            REACH_TEXT_WEIGHT_DEMIBOLD, REACH_TEXT_ALIGNMENT_CENTER, accent, 0);
    }

    const uint16_t *display_name = model->account_display_name[0] != 0
                                       ? model->account_display_name
                                       : (const uint16_t *)L"Windows user";
    reach_settings_push_text(commands, layout->account_name, display_name,
                             reach_settings_scale(input, REACH_TEXT_SIZE_HEADING),
                             REACH_TEXT_WEIGHT_DEMIBOLD, input->text_alignment_leading,
                             input->theme->settings_text, 1);
    if (model->account_user_name[0] != 0)
    {
        reach_settings_push_text(commands, layout->account_user, model->account_user_name,
                                 reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                                 REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading,
                                 input->theme->settings_secondary_text, 1);
    }
    reach_ui_selection_item_style badge_style = reach_settings_pill_style(input, accent);
    reach_ui_selection_item_render(commands, layout->account_type_badge,
                                   reach_settings_account_type_label(model->account_is_admin),
                                   &badge_style, 1.0f);

    reach_settings_push_rect(commands, layout->account_password_card,
                             reach_settings_scale(input, input->theme->radius_small),
                             input->theme->settings_card_background);
    reach_settings_push_rect(commands, layout->account_password_icon,
                             reach_settings_scale(input, input->theme->radius_small),
                             input->theme->settings_icon_box_background);
    reach_settings_push_icon(commands, layout->account_password_icon,
                             input->theme->settings_secondary_text, REACH_VECTOR_ICON_LOCK, 0.22f);
    reach_settings_push_text(
        commands, layout->account_password_title, (const uint16_t *)L"Password",
        reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM), REACH_TEXT_WEIGHT_SEMIBOLD,
        input->text_alignment_leading, input->theme->settings_text, 1);
    reach_settings_push_text(commands, layout->account_password_subtitle,
                             (const uint16_t *)L"Change the password you use to sign in to Windows",
                             reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                             REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading,
                             input->theme->settings_secondary_text, 1);

    if (model->account_status != REACH_SETTINGS_ACCOUNT_STATUS_NONE)
    {
        reach_color status_color = model->account_status == REACH_SETTINGS_ACCOUNT_STATUS_SUCCESS
                                       ? input->theme->settings_status_success
                                       : input->theme->settings_status_error;
        reach_settings_push_text(commands, layout->account_password_status,
                                 reach_settings_account_status_message(model->account_status),
                                 reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                                 REACH_TEXT_WEIGHT_SEMIBOLD, REACH_TEXT_ALIGNMENT_TRAILING,
                                 status_color, 1);
    }

    static const uint16_t *placeholders[REACH_SETTINGS_ACCOUNT_FIELD_COUNT] = {
        (const uint16_t *)L"Current password", (const uint16_t *)L"New password",
        (const uint16_t *)L"Confirm password"};
    reach_ui_selection_item_style field_style = reach_settings_pill_style(input, accent);
    field_style.background = input->theme->settings_input_background;
    field_style.text_size = reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL);
    for (size_t field = 0; field < REACH_SETTINGS_ACCOUNT_FIELD_COUNT; ++field)
    {
        const reach_text_edit *edit = &model->account_password_edits[field];
        int32_t focused = model->account_focused_field == (int32_t)field;

        const reach_rect_f32 field_rect = layout->account_password_fields[field];
        reach_ui_selection_item_backdrop_render(commands, field_rect, &field_style,
                                                focused ? 1.0f : 0.0f);

        uint16_t masked[REACH_SETTINGS_ACCOUNT_PASSWORD_CAPACITY + 1] = {};
        for (size_t index = 0; index < edit->length; ++index)
        {
            masked[index] = 0x2022;
        }

        reach_ui_textbox_state state = {};
        state.text = masked;
        state.placeholder = placeholders[field];
        state.text_alignment = input->text_alignment_leading;
        state.caret_index = edit->caret;
        state.caret_visible = focused && model->account_caret_visible;
        reach_text_edit_selection_range(edit, &state.selection_start, &state.selection_end);
        if (!focused)
        {
            state.selection_start = 0;
            state.selection_end = 0;
        }
        state.text_color = input->theme->settings_text;
        state.placeholder_color =
            reach_settings_color_with_alpha(input->theme->settings_secondary_text, 0.55f);
        state.selection_color = reach_settings_color_with_alpha(accent, 0.30f);
        float text_inset = reach_settings_scale(input, 13.0f);
        reach_rect_f32 text_rect = {field_rect.x + text_inset, field_rect.y,
                                    field_rect.width - text_inset * 2.0f, field_rect.height};
        reach_ui_textbox_render(commands, text_rect, &field_style, focused ? 1.0f : 0.0f, &state);
    }

    reach_ui_button_style change_style =
        reach_settings_button_style(input, input->theme->settings_button_primary);
    reach_ui_button_render(
        commands, layout->account_password_button, (const uint16_t *)L"Change", &change_style, 1,
        reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_ACCOUNT_PASSWORD));
}

static void render_power_page(const reach_settings_render_input *input,
                              reach_render_command_buffer *commands)
{
    const reach_settings_model *model = input->model;
    const reach_settings_layout *layout = input->layout;
    const reach_settings_power_row_style *styles = reach_settings_power_row_styles();

    for (size_t timer = 0; timer < REACH_SETTINGS_POWER_TIMER_COUNT; ++timer)
    {
        const reach_settings_power_row_style *style = &styles[timer];
        const reach_color accent = reach_theme_accent_color(input->theme, style->accent);
        reach_settings_push_rect(commands, layout->power_cards[timer],
                                 reach_settings_scale(input, input->theme->radius_small),
                                 input->theme->settings_card_background);
        reach_settings_push_rect(commands, layout->power_icon_boxes[timer],
                                 reach_settings_scale(input, input->theme->radius_small),
                                 input->theme->settings_icon_box_background);
        reach_settings_push_icon(commands, layout->power_icon_boxes[timer],
                                 input->theme->settings_secondary_text,
                                 (reach_vector_icon_id)style->icon_id, 0.22f);
        reach_settings_push_text(commands, layout->power_titles[timer], style->title,
                                 reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM),
                                 REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
                                 input->theme->settings_text, 1);
        if (layout->power_subtitles[timer].height > 0.0f)
        {
            reach_settings_push_text(commands, layout->power_subtitles[timer], style->subtitle,
                                     reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                                     REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading,
                                     input->theme->settings_secondary_text, 1);
        }
        if (layout->power_wait_toggles[timer].width > 0.0f)
        {
            float wait_t = reach_animation_manager_value(&model->power_wait_animations, timer);
            reach_settings_push_text(
                commands, layout->power_wait_labels[timer],
                (const uint16_t *)L"Wait for apps keeping the PC awake",
                reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL), REACH_TEXT_WEIGHT_NORMAL,
                REACH_TEXT_ALIGNMENT_TRAILING,
                reach_settings_color_with_alpha(input->theme->settings_secondary_text,
                                                0.7f + 0.3f * wait_t),
                1);
            reach_ui_toggle_style toggle_style = {};
            toggle_style.track_off = input->theme->settings_toggle_track_off;
            toggle_style.track_on = reach_settings_color_with_alpha(accent, 0.85f);
            toggle_style.knob = input->theme->settings_toggle_knob;
            reach_ui_toggle_render(commands, layout->power_wait_toggles[timer], &toggle_style,
                                   wait_t);
        }

        float t = reach_animation_manager_value(&model->power_animations, timer);
        size_t selected = model->power_selected[timer];
        size_t previous = model->power_previous[timer];
        reach_ui_selection_item_style pill_style = reach_settings_pill_style(input, accent);
        for (size_t option = 0; option < REACH_SETTINGS_POWER_OPTION_COUNT; ++option)
        {
            float selection = 0.0f;
            if (option == selected)
            {
                selection = t;
            }
            else if (option == previous)
            {
                selection = 1.0f - t;
            }
            if (option == REACH_SETTINGS_POWER_CUSTOM_OPTION)
            {
                const reach_rect_f32 slot = layout->power_options[timer][option];
                reach_ui_selection_item_style custom_style = pill_style;
                custom_style.background =
                    reach_theme_color_mix(input->theme->settings_input_background,
                                          input->theme->settings_pill_background, selection);
                reach_ui_selection_item_backdrop_render(commands, slot, &custom_style, selection);
                const reach_rect_f32 hours =
                    layout->power_custom_fields[timer][REACH_SETTINGS_POWER_FIELD_HOURS];
                const reach_rect_f32 minutes =
                    layout->power_custom_fields[timer][REACH_SETTINGS_POWER_FIELD_MINUTES];
                float divider_x = (hours.x + hours.width + minutes.x) * 0.5f;
                reach_settings_push_rect(commands,
                                         {divider_x, slot.y + slot.height * 0.25f,
                                          reach_settings_scale(input, 1.0f), slot.height * 0.5f},
                                         0.0f, input->theme->settings_divider);

                static const uint16_t *suffixes[REACH_SETTINGS_POWER_FIELD_COUNT] = {
                    (const uint16_t *)L"hr", (const uint16_t *)L"min"};
                for (size_t field = 0; field < REACH_SETTINGS_POWER_FIELD_COUNT; ++field)
                {
                    const reach_text_edit *edit = &model->power_custom_edits[timer][field];
                    int32_t focused = model->power_focused_timer == (int32_t)timer &&
                                      model->power_focused_field == (int32_t)field;
                    reach_ui_textbox_state state = {};
                    state.text = edit->text;
                    state.placeholder = (const uint16_t *)L"0";
                    state.suffix = suffixes[field];
                    state.suffix_width = reach_settings_scale(
                        input, field == REACH_SETTINGS_POWER_FIELD_HOURS ? 16.0f : 24.0f);
                    state.text_alignment = REACH_TEXT_ALIGNMENT_TRAILING;
                    state.caret_index = edit->caret;
                    state.caret_visible = focused && model->power_caret_visible;
                    reach_text_edit_selection_range(edit, &state.selection_start,
                                                    &state.selection_end);
                    if (!focused)
                    {
                        state.selection_start = 0;
                        state.selection_end = 0;
                    }
                    state.text_color = input->theme->settings_secondary_text;
                    state.placeholder_color = reach_settings_color_with_alpha(
                        input->theme->settings_secondary_text, 0.55f);
                    state.selection_color = reach_settings_color_with_alpha(accent, 0.30f);
                    state.suffix_color = reach_settings_color_with_alpha(
                        input->theme->settings_secondary_text, 0.65f);
                    reach_ui_textbox_render(commands, layout->power_custom_fields[timer][field],
                                            &custom_style, selection, &state);
                }
            }
            else
            {
                reach_ui_selection_item_render(commands, layout->power_options[timer][option],
                                               reach_settings_power_option_label(timer, option),
                                               &pill_style, selection);
            }
        }
    }

    reach_ui_button_style apply_style =
        reach_settings_button_style(input, input->theme->settings_button_primary);
    reach_ui_button_render(
        commands, layout->power_apply_button, (const uint16_t *)L"Apply", &apply_style,
        reach_settings_model_power_dirty(model),
        reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_POWER_APPLY));
}

typedef struct display_toggle_card
{
    reach_rect_f32 card;
    reach_rect_f32 icon;
    reach_rect_f32 title;
    reach_rect_f32 subtitle;
    reach_rect_f32 toggle;
    reach_vector_icon_id icon_id;
    const wchar_t *icon_text;
    const wchar_t *title_text;
    const wchar_t *subtitle_text;
    float position;
} display_toggle_card;

static void render_display_toggle_card(const reach_settings_render_input *input,
                                       reach_render_command_buffer *commands,
                                       const reach_ui_toggle_style *toggle_style,
                                       const display_toggle_card *card)
{
    float radius = reach_settings_scale(input, input->theme->radius_small);
    reach_settings_push_rect(commands, card->card, radius, input->theme->settings_card_background);
    reach_settings_push_rect(commands, card->icon, radius,
                             input->theme->settings_icon_box_background);
    if (card->icon_text != nullptr)
    {
        reach_settings_push_text(commands, card->icon, (const uint16_t *)card->icon_text,
                                 reach_settings_scale(input, REACH_TEXT_SIZE_HEADING),
                                 REACH_TEXT_WEIGHT_EXTRABOLD, REACH_TEXT_ALIGNMENT_CENTER,
                                 input->theme->settings_secondary_text, 0);
    }
    else
    {
        reach_settings_push_icon(commands, card->icon, input->theme->settings_secondary_text,
                                 card->icon_id, 0.22f);
    }
    reach_settings_push_text(commands, card->title, (const uint16_t *)card->title_text,
                             reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM),
                             REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
                             input->theme->settings_text, 1);
    reach_settings_push_text(commands, card->subtitle, (const uint16_t *)card->subtitle_text,
                             reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                             REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading,
                             input->theme->settings_secondary_text, 1);
    reach_ui_toggle_render(commands, card->toggle, toggle_style, card->position);
}

static void render_display_theme_choice_card(
    const reach_settings_render_input *input, reach_render_command_buffer *commands,
    reach_rect_f32 card, reach_rect_f32 title, reach_rect_f32 subtitle,
    const reach_rect_f32 options[REACH_SETTINGS_THEME_OPTION_COUNT], const uint16_t *title_text,
    const uint16_t *subtitle_text, reach_config_theme_preference selected, reach_color accent)
{
    reach_settings_push_rect(commands, card,
                             reach_settings_scale(input, input->theme->radius_small),
                             input->theme->settings_card_background);
    reach_settings_push_text(
        commands, title, title_text, reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM),
        REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading, input->theme->settings_text, 1);
    reach_settings_push_text(commands, subtitle, subtitle_text,
                             reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                             REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading,
                             input->theme->settings_secondary_text, 1);

    const uint16_t *labels[REACH_SETTINGS_THEME_OPTION_COUNT] = {
        (const uint16_t *)u"Follow Reach", (const uint16_t *)u"Light", (const uint16_t *)u"Dark"};
    reach_ui_selection_item_style style = reach_settings_pill_style(input, accent);
    style.text_size = reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL);
    for (size_t option = 0; option < REACH_SETTINGS_THEME_OPTION_COUNT; ++option)
    {
        reach_ui_selection_item_render(commands, options[option], labels[option], &style,
                                       selected == (reach_config_theme_preference)option ? 1.0f
                                                                                         : 0.0f);
    }
}

static void render_display_page(const reach_settings_render_input *input,
                                reach_render_command_buffer *commands)
{
    const reach_settings_model *model = input->model;
    const reach_settings_layout *layout = input->layout;
    reach_color accent = reach_theme_accent_color(input->theme, REACH_THEME_ACCENT_CYAN);

    reach_ui_toggle_style toggle_style = {};
    toggle_style.track_off = input->theme->settings_toggle_track_off;
    toggle_style.track_on = reach_settings_color_with_alpha(accent, 0.85f);
    toggle_style.knob = input->theme->settings_toggle_knob;

    const display_toggle_card cards[] = {
        {layout->display_fps_card, layout->display_fps_icon, layout->display_fps_title,
         layout->display_fps_subtitle, layout->display_fps_toggle, REACH_VECTOR_ICON_ARROW_UP,
         nullptr, L"Adaptive animations", L"Match your display's refresh rate, up to 120fps",
         reach_animation_manager_value(&model->display_fps_animation, 0)},
        {layout->display_font_card, layout->display_font_icon, layout->display_font_title,
         layout->display_font_subtitle, layout->display_font_toggle, REACH_VECTOR_ICON_NONE, L"F",
         L"Use JetBrains Mono", L"Off uses the Windows system font",
         reach_animation_manager_value(&model->display_font_animation, 0)},
        {layout->display_theme_card, layout->display_theme_icon, layout->display_theme_title,
         layout->display_theme_subtitle, layout->display_theme_toggle, REACH_VECTOR_ICON_BRIGHTNESS,
         nullptr, L"Light theme", L"Off uses the dark theme",
         reach_animation_manager_value(&model->display_theme_animation, 0)},
    };
    for (size_t index = 0; index < sizeof(cards) / sizeof(cards[0]); ++index)
    {
        render_display_toggle_card(input, commands, &toggle_style, &cards[index]);
    }

    reach_settings_push_text(
        commands, layout->display_windows_section_title, (const uint16_t *)u"System appearance",
        reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL), REACH_TEXT_WEIGHT_SEMIBOLD,
        input->text_alignment_leading, input->theme->settings_secondary_text, 1);
    render_display_theme_choice_card(
        input, commands, layout->display_windows_system_card, layout->display_windows_system_title,
        layout->display_windows_system_subtitle, layout->display_windows_system_options,
        (const uint16_t *)u"System theme", (const uint16_t *)u"Taskbar, Start, and system surfaces",
        reach_settings_model_windows_system_theme(model), accent);
    render_display_theme_choice_card(
        input, commands, layout->display_windows_app_card, layout->display_windows_app_title,
        layout->display_windows_app_subtitle, layout->display_windows_app_options,
        (const uint16_t *)u"Application theme",
        (const uint16_t *)u"Supported apps and Windows dialogs",
        reach_settings_model_windows_app_theme(model), accent);
}

static void build_startup_summary(const reach_settings_model *model, uint16_t *text,
                                  size_t capacity)
{
    size_t count = model->startup_apps.count;
    size_t enabled = reach_startup_app_enabled_count(&model->startup_apps);
    text[0] = 0;
    append_number(text, capacity, count);
    append_text(text, capacity,
                count == 1 ? (const uint16_t *)u" app is set to run at sign-in"
                           : (const uint16_t *)u" apps are set to run at sign-in");
    append_text(text, capacity, (const uint16_t *)u"  \u00B7  ");
    append_number(text, capacity, enabled);
    append_text(text, capacity, (const uint16_t *)u" enabled");
}

static void render_startup_apps_page(const reach_settings_render_input *input,
                                     reach_render_command_buffer *commands)
{
    const reach_settings_model *model = input->model;
    const reach_settings_layout *layout = input->layout;
    reach_color accent = reach_theme_accent_color(input->theme, REACH_THEME_ACCENT_PURPLE);

    uint16_t summary[160] = {};
    build_startup_summary(model, summary, 160);
    reach_settings_push_text(commands, layout->startup_summary, summary,
                             reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM),
                             REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
                             input->theme->settings_secondary_text, 1);

    if (model->startup_status != REACH_SETTINGS_STARTUP_STATUS_NONE)
    {
        reach_color status_color = model->startup_status == REACH_SETTINGS_STARTUP_STATUS_FAILED
                                       ? input->theme->settings_status_error
                                       : input->theme->settings_secondary_text;
        reach_settings_push_text(commands, layout->startup_summary,
                                 reach_settings_startup_status_message(model->startup_status),
                                 reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM),
                                 REACH_TEXT_WEIGHT_SEMIBOLD, REACH_TEXT_ALIGNMENT_TRAILING,
                                 status_color, 1);
    }

    if (layout->startup_row_count == 0)
    {
        reach_settings_push_text(
            commands, layout->startup_viewport,
            model->startup_loaded ? (const uint16_t *)u"Nothing runs at sign-in"
                                  : (const uint16_t *)u"Reading startup apps...",
            reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM), REACH_TEXT_WEIGHT_NORMAL,
            REACH_TEXT_ALIGNMENT_CENTER, input->theme->settings_secondary_text, 1);
        return;
    }

    reach_ui_selection_item_style badge_style =
        reach_settings_pill_style(input, input->theme->settings_secondary_text);
    reach_render_command_buffer_set_scissor(commands, layout->startup_viewport);
    for (size_t index = 0; index < layout->startup_row_count; ++index)
    {
        if (index >= model->startup_apps.count)
        {
            break;
        }
        const reach_startup_app_entry *entry = &model->startup_apps.entries[index];
        const reach_rect_f32 row = layout->startup_rows[index];

        float on = reach_animation_manager_value(&model->startup_animations, index);
        reach_settings_push_rect(commands, row,
                                 reach_settings_scale(input, input->theme->radius_small),
                                 input->theme->settings_card_background);

        float icon_box_size = reach_settings_scale(input, 36.0f);
        reach_rect_f32 icon_box = {row.x + reach_settings_scale(input, 14.0f),
                                   row.y + (row.height - icon_box_size) * 0.5f, icon_box_size,
                                   icon_box_size};
        reach_settings_push_rect(commands, icon_box,
                                 reach_settings_scale(input, input->theme->radius_small),
                                 input->theme->settings_icon_box_background);
        if (model->startup_icons[index] != 0)
        {
            float inset = reach_settings_scale(input, 6.0f);
            reach_rect_f32 image = {icon_box.x + inset, icon_box.y + inset,
                                    icon_box.width - inset * 2.0f, icon_box.height - inset * 2.0f};
            reach_settings_push_app_icon(commands, image, model->startup_icons[index], 1.0f);
        }
        else
        {
            reach_settings_push_icon(commands, icon_box, input->theme->settings_secondary_text,
                                     REACH_VECTOR_ICON_EXECUTABLE, 0.26f);
        }

        const reach_rect_f32 toggle = layout->startup_toggles[index];
        float badge_width = reach_settings_scale(input, 86.0f);
        float badge_height = reach_settings_scale(input, 22.0f);
        reach_rect_f32 badge = {toggle.x - reach_settings_scale(input, 16.0f) - badge_width,
                                row.y + (row.height - badge_height) * 0.5f, badge_width,
                                badge_height};
        float badge_accent = reach_startup_app_source_is_machine(entry->source) ? 0.0f : 1.0f;
        reach_ui_selection_item_render(commands, badge,
                                       reach_startup_app_source_label(entry->source), &badge_style,
                                       badge_accent);

        float text_x = icon_box.x + icon_box.width + reach_settings_scale(input, 14.0f);
        float text_width = badge.x - reach_settings_scale(input, 14.0f) - text_x;
        if (text_width < 0.0f)
        {
            text_width = 0.0f;
        }
        reach_settings_push_text(commands,
                                 {text_x, row.y + reach_settings_scale(input, 12.0f), text_width,
                                  reach_settings_scale(input, 18.0f)},
                                 entry->display_name,
                                 reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM),
                                 REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
                                 input->theme->settings_text, 1);

        const uint16_t *detail = entry->executable[0] != 0 ? entry->executable : entry->command;
        reach_settings_push_text(commands,
                                 {text_x, row.y + reach_settings_scale(input, 33.0f), text_width,
                                  reach_settings_scale(input, 15.0f)},
                                 detail, reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                                 REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading,
                                 input->theme->settings_secondary_text, 1);

        reach_ui_toggle_style toggle_style = {};
        toggle_style.track_off = input->theme->settings_toggle_track_off;
        toggle_style.track_on = reach_settings_color_with_alpha(accent, 0.85f);
        toggle_style.knob = input->theme->settings_toggle_knob;
        reach_ui_toggle_render(commands, toggle, &toggle_style, on);
    }
    reach_render_command_buffer_clear_scissor(commands);

    if (layout->startup_scrollbar_thumb.height > 0.0f)
    {
        reach_rect_f32 origin = {0.0f, 0.0f, 0.0f, 0.0f};
        reach_scrollbar_build_render_commands(layout->startup_scrollbar_track,
                                              layout->startup_scrollbar_thumb, origin,
                                              input->theme->settings_scrollbar_track,
                                              input->theme->settings_scrollbar_thumb, commands);
    }
}

static void build_reach_version_line(const reach_settings_model *model, uint16_t *text,
                                     size_t capacity)
{
    text[0] = 0;
    append_text(text, capacity, (const uint16_t *)u"Reach ");
    append_text(text, capacity,
                model->reach_current_version[0] != 0 ? model->reach_current_version
                                                     : (const uint16_t *)u"?");
    if (model->reach_update_state == REACH_SETTINGS_REACH_UPDATE_AVAILABLE &&
        model->reach_update_info.version[0] != 0)
    {
        append_text(text, capacity, (const uint16_t *)u"  \u2192  ");
        append_text(text, capacity, model->reach_update_info.version);
    }
}

static void render_reach_section(const reach_settings_render_input *input,
                                 reach_render_command_buffer *commands)
{
    const reach_settings_model *model = input->model;
    const reach_settings_layout *layout = input->layout;
    reach_color accent = reach_theme_accent_color(input->theme, REACH_THEME_ACCENT_CYAN);

    reach_settings_push_text(commands, layout->reach_section_title, (const uint16_t *)u"Reach",
                             reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                             REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
                             input->theme->settings_secondary_text, 1);

    const reach_rect_f32 row = layout->reach_update_row;
    reach_settings_push_rect(commands, row, reach_settings_scale(input, input->theme->radius_small),
                             input->theme->settings_card_background);

    reach_rect_f32 icon_box = {row.x + reach_settings_scale(input, 12.0f),
                               row.y + (row.height - reach_settings_scale(input, 34.0f)) * 0.5f,
                               reach_settings_scale(input, 34.0f),
                               reach_settings_scale(input, 34.0f)};
    reach_settings_push_rect(commands, icon_box,
                             reach_settings_scale(input, input->theme->radius_small),
                             input->theme->settings_icon_box_background);
    reach_settings_push_icon(commands, icon_box, input->theme->settings_secondary_text,
                             REACH_VECTOR_ICON_RESTART, 0.22f);

    float left = icon_box.x + icon_box.width + reach_settings_scale(input, 12.0f);
    float width = layout->reach_update_button.x - left - reach_settings_scale(input, 12.0f);
    uint16_t version_line[128] = {};
    build_reach_version_line(model, version_line, 128);
    reach_settings_push_text(commands,
                             {left, row.y + reach_settings_scale(input, 12.0f), width,
                              reach_settings_scale(input, 18.0f)},
                             version_line, reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM),
                             REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
                             input->theme->settings_text, 1);

    const uint16_t *status = reach_settings_model_reach_update_status(model);
    uint16_t status_line[128] = {};
    if (model->reach_update_state == REACH_SETTINGS_REACH_UPDATE_DOWNLOADING &&
        model->reach_download_total > 0)
    {
        int32_t percent =
            (int32_t)((model->reach_download_received * 100) / model->reach_download_total);
        uint16_t number[8] = {};
        int32_t digits = 0;
        int32_t value = percent < 0 ? 0 : percent > 100 ? 100 : percent;
        do
        {
            number[digits++] = (uint16_t)(u'0' + value % 10);
            value /= 10;
        } while (value != 0 && digits < 7);
        append_text(status_line, 128, (const uint16_t *)u"Downloading update... ");
        for (int32_t index = digits - 1; index >= 0; --index)
        {
            uint16_t single[2] = {number[index], 0};
            append_text(status_line, 128, single);
        }
        append_text(status_line, 128, (const uint16_t *)u"%");
        status = status_line;
    }
    reach_settings_push_text(commands,
                             {left, row.y + reach_settings_scale(input, 34.0f), width,
                              reach_settings_scale(input, 14.0f)},
                             status, reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                             REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading,
                             input->theme->settings_secondary_text, 1);

    int32_t busy = model->reach_update_state == REACH_SETTINGS_REACH_UPDATE_CHECKING ||
                   model->reach_update_state == REACH_SETTINGS_REACH_UPDATE_DOWNLOADING;
    int32_t enabled = !busy && model->reach_update_state != REACH_SETTINGS_REACH_UPDATE_UP_TO_DATE;
    reach_color button_color = model->reach_update_state == REACH_SETTINGS_REACH_UPDATE_AVAILABLE
                                   ? input->theme->settings_button_success
                                   : input->theme->settings_card_background;
    reach_ui_button_style reach_style = reach_settings_button_style(input, button_color);
    reach_style.disabled_background = input->theme->settings_card_background;
    reach_ui_button_render(
        commands, layout->reach_update_button,
        reach_settings_model_reach_update_button_label(model), &reach_style, enabled,
        reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_REACH_UPDATE));
}

static void render_update_page(const reach_settings_render_input *input,
                               reach_render_command_buffer *commands)
{
    const reach_settings_model *model = input->model;
    const reach_settings_layout *layout = input->layout;
    const int32_t busy = reach_settings_model_update_busy(model);
    const int32_t install_enabled = !busy && reach_settings_model_selected_update_count(model) > 0;
    const int32_t restart_enabled = !busy && reach_settings_model_restart_required_count(model) > 0;
    reach_color accent = reach_theme_accent_color(input->theme, REACH_THEME_ACCENT_CYAN);

    const uint16_t *scan_button_text =
        model->update_scan_completed ? (const uint16_t *)u"Refresh" : (const uint16_t *)u"Search";
    if (model->update_page_state == REACH_SETTINGS_UPDATE_SCANNING)
        scan_button_text = model->update_scan_completed ? (const uint16_t *)u"Refreshing\u2026"
                                                        : (const uint16_t *)u"Searching\u2026";
    reach_ui_button_style refresh_style =
        reach_settings_button_style(input, input->theme->settings_button_success);
    reach_ui_button_render(
        commands, layout->update_refresh_button, scan_button_text, &refresh_style, !busy,
        reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_UPDATE_REFRESH));
    reach_ui_button_style install_style =
        reach_settings_button_style(input, input->theme->settings_button_primary);
    reach_ui_button_render(
        commands, layout->update_install_button, (const uint16_t *)u"Install selected",
        &install_style, install_enabled,
        reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_UPDATE_INSTALL));
    reach_ui_button_style restart_style =
        reach_settings_button_style(input, input->theme->settings_button_danger);
    reach_ui_button_render(
        commands, layout->update_restart_button, (const uint16_t *)u"Restart now", &restart_style,
        restart_enabled,
        reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_UPDATE_RESTART));

    render_reach_section(input, commands);

    reach_settings_push_text(
        commands, layout->windows_section_title, (const uint16_t *)u"Windows updates",
        reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL), REACH_TEXT_WEIGHT_SEMIBOLD,
        input->text_alignment_leading, input->theme->settings_secondary_text, 1);

    reach_rect_f32 update_status_message = layout->update_viewport;

    if (model->update_page_state == REACH_SETTINGS_UPDATE_SCANNING)
    {
        reach_settings_push_text(
            commands, update_status_message, (const uint16_t *)u"Searching for updates\u2026",
            reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM), REACH_TEXT_WEIGHT_NORMAL,
            REACH_TEXT_ALIGNMENT_CENTER, input->theme->settings_secondary_text, 1);

        float loader_width = reach_settings_scale(input, 220.0f);
        float loader_height = reach_settings_scale(input, 4.0f);
        if (loader_width > update_status_message.width)
            loader_width = update_status_message.width;

        reach_rect_f32 loader_container = {
            update_status_message.x + (update_status_message.width - loader_width) * 0.5f,
            update_status_message.y + update_status_message.height * 0.5f +
                reach_settings_scale(input, 18.0f),
            loader_width, loader_height};

        reach_rect_f32 loader_bar = reach_loader_bar_rect(&model->update_loader, loader_container);
        reach_rect_f32 loader_origin = {0.0f, 0.0f, 0.0f, 0.0f};
        (void)reach_loader_build_render_commands(loader_container, loader_bar, loader_origin,
                                                 accent, commands);
        return;
    }
    else if (model->update_page_state == REACH_SETTINGS_UPDATE_ERROR &&
             layout->update_row_count == 0)
        reach_settings_push_text(commands, layout->update_viewport,
                                 (const uint16_t *)u"Unable to refresh Windows updates",
                                 reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM),
                                 REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading,
                                 input->theme->settings_status_error, 1);
    else if (model->update_scan_completed && layout->update_row_count == 0)
        reach_settings_push_text(
            commands, update_status_message, (const uint16_t *)u"Windows is up to date",
            reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM), REACH_TEXT_WEIGHT_NORMAL,
            REACH_TEXT_ALIGNMENT_CENTER, input->theme->settings_secondary_text, 1);
    else if (!model->update_scan_completed && layout->update_row_count == 0)
        reach_settings_push_text(
            commands, update_status_message, (const uint16_t *)u"Search for updates",
            reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM), REACH_TEXT_WEIGHT_NORMAL,
            REACH_TEXT_ALIGNMENT_CENTER, input->theme->settings_secondary_text, 1);

    static const uint16_t *section_titles[] = {(const uint16_t *)u"Select updates",
                                               (const uint16_t *)u"Restart required",
                                               (const uint16_t *)u"Failed"};
    reach_render_command_buffer_set_scissor(commands, layout->update_viewport);
    for (size_t index = 0; index < layout->update_section_count; ++index)
    {
        const reach_rect_f32 title = layout->update_section_titles[index];
        reach_settings_push_text(commands, title, section_titles[layout->update_section_ids[index]],
                                 reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                                 REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
                                 input->theme->settings_secondary_text, 1);
    }

    for (size_t index = 0; index < layout->update_row_count; ++index)
    {
        size_t update_index = layout->update_indices[index];
        if (update_index >= model->update_list.count)
            break;
        const reach_windows_update_item *update = &model->update_list.updates[update_index];
        const reach_rect_f32 row = layout->update_rows[index];
        const reach_rect_f32 checkbox = layout->update_checkboxes[index];
        reach_color row_color = input->theme->settings_card_background;
        reach_settings_push_rect(
            commands, row, reach_settings_scale(input, input->theme->radius_small), row_color);
        if (update->state == REACH_WINDOWS_UPDATE_INSTALLED_REBOOT_REQUIRED)
            reach_settings_push_icon(commands, checkbox, input->theme->settings_status_success,
                                     REACH_VECTOR_ICON_CHECK, 0.06f);
        else if (update->state == REACH_WINDOWS_UPDATE_FAILED)
            reach_settings_push_icon(commands, checkbox, input->theme->settings_status_error,
                                     REACH_VECTOR_ICON_CLOSE, 0.06f);
        else
        {
            reach_color checkbox_background = update->selected ? accent : row_color;
            reach_color checkbox_border =
                update->selected ? accent : input->theme->settings_secondary_text;
            reach_settings_push_bordered_background(
                commands, checkbox, reach_settings_scale(input, input->theme->radius_small),
                reach_settings_scale(input, 1.5f), checkbox_background, checkbox_border);
            if (update->selected)
            {
                reach_settings_push_icon(commands, checkbox, input->theme->inverse_text,
                                         REACH_VECTOR_ICON_CHECK, 0.18f);
            }
        }

        float left = checkbox.x + checkbox.width + reach_settings_scale(input, 10.0f);
        float width = row.x + row.width - left - reach_settings_scale(input, 10.0f);
        reach_settings_push_text(commands,
                                 {left, row.y + reach_settings_scale(input, 6.0f), width,
                                  reach_settings_scale(input, 18.0f)},
                                 update->identity.title,
                                 reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM),
                                 REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
                                 input->theme->settings_text, 1);
        uint16_t metadata[260] = {};
        build_metadata_text(update, metadata, 260);
        reach_settings_push_text(commands,
                                 {left, row.y + reach_settings_scale(input, 30.0f), width,
                                  reach_settings_scale(input, 14.0f)},
                                 metadata, reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                                 REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading,
                                 input->theme->settings_secondary_text, 1);
        uint16_t status[260] = {};
        build_status_text(update, status, 260);
        reach_settings_push_text(commands,
                                 {left, row.y + reach_settings_scale(input, 47.0f), width,
                                  reach_settings_scale(input, 14.0f)},
                                 status, reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL),
                                 REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading,
                                 input->theme->settings_secondary_text, 1);
    }
    reach_render_command_buffer_clear_scissor(commands);

    if (layout->update_scrollbar_thumb.height > 0.0f)
    {
        reach_rect_f32 origin = {0.0f, 0.0f, 0.0f, 0.0f};
        reach_scrollbar_build_render_commands(layout->update_scrollbar_track,
                                              layout->update_scrollbar_thumb, origin,
                                              input->theme->settings_scrollbar_track,
                                              input->theme->settings_scrollbar_thumb, commands);
    }
}

reach_result reach_settings_build_render_commands(const reach_settings_render_input *input,
                                                  reach_render_command_buffer *commands)
{
    if (input == nullptr || input->theme == nullptr || input->model == nullptr ||
        input->layout == nullptr || commands == nullptr)
        return REACH_INVALID_ARGUMENT;
    reach_render_command_buffer_clear(commands);
    float scale = input->dpi_scale > 0.0f ? input->dpi_scale : 1.0f;
    float border_thickness = reach_theme_border_thickness(input->theme, scale);
    reach_render_command background = {};
    background.type = REACH_RENDER_COMMAND_RECT;
    background.rect = input->layout->bounds;
    background.radius = reach_settings_scale(input, input->theme->radius_large);
    (void)reach_render_push_bordered_background(
        commands, &background, input->theme->settings_background, input->theme->bar_border,
        border_thickness, nullptr, scale);

    reach_rect_f32 content_bounds =
        reach_theme_border_content_rect(input->theme, scale, input->layout->bounds);
    reach_rect_f32 nav = input->layout->nav;
    nav.x = content_bounds.x;
    nav.y = content_bounds.y;
    nav.width -= nav.x - input->layout->nav.x;
    nav.height = content_bounds.height;
    if (nav.width < 0.0f)
    {
        nav.width = 0.0f;
    }
    float nav_radius = reach_settings_scale(input, input->theme->radius_large) - border_thickness;
    if (nav_radius < 0.0f)
    {
        nav_radius = 0.0f;
    }
    reach_settings_push_masked_rect(commands, nav, nav_radius,
                                    REACH_RENDER_CORNER_TOP_LEFT | REACH_RENDER_CORNER_BOTTOM_LEFT,
                                    input->theme->settings_nav_background);
    reach_settings_push_rect(commands, input->layout->close_button,
                             input->layout->close_button.width * 0.5f,
                             input->model->hovered_button == REACH_SETTINGS_HIT_CLOSE
                                 ? input->theme->settings_window_close_hover
                                 : input->theme->settings_window_button_background);
    reach_settings_push_rect(commands, input->layout->minimize_button,
                             input->layout->minimize_button.width * 0.5f,
                             input->model->hovered_button == REACH_SETTINGS_HIT_MINIMIZE
                                 ? input->theme->settings_window_minimize_hover
                                 : input->theme->settings_window_button_background);
    reach_settings_push_icon(commands, input->layout->close_button, input->theme->inverse_text,
                             REACH_VECTOR_ICON_CLOSE, 0.24f);
    reach_settings_push_icon(commands, input->layout->minimize_button, input->theme->inverse_text,
                             REACH_VECTOR_ICON_MINIMIZE, 0.24f);

    size_t nav_count = 0;
    const reach_settings_nav_item *items = reach_settings_nav_items(&nav_count);
    render_nav_selection_indicator(input, commands, items, nav_count);
    for (size_t index = 0; index < input->layout->nav_item_count && index < nav_count; ++index)
    {
        const reach_settings_nav_item_layout *item_layout = &input->layout->nav_items[index];
        const reach_settings_nav_item *item = &items[index];
        const reach_color accent = reach_theme_accent_color(input->theme, item->accent);
        const reach_color accent_background =
            reach_settings_color_with_alpha(accent, input->theme->accent_tint_alpha);
        reach_settings_push_rect(commands, item_layout->icon_background,
                                 reach_settings_scale(input, input->theme->radius_small),
                                 accent_background);
        reach_settings_push_icon(commands, item_layout->icon, accent,
                                 (reach_vector_icon_id)item->icon_id, 0.0f);
        reach_settings_push_text(commands, item_layout->label, item->label,
                                 reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM),
                                 REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
                                 input->theme->settings_text, 1);
    }

    uint16_t footer_text[64] = {};
    append_text(footer_text, 64, (const uint16_t *)u"Reach v");
    append_text(footer_text, 64,
                input->model->reach_current_version[0] != 0 ? input->model->reach_current_version
                                                            : (const uint16_t *)u"?");
    reach_settings_push_text(
        commands, input->layout->nav_footer, footer_text,
        reach_settings_scale(input, REACH_TEXT_SIZE_XSMALL), REACH_TEXT_WEIGHT_NORMAL,
        input->text_alignment_leading,
        reach_settings_color_with_alpha(input->theme->settings_secondary_text, 0.7f), 1);

    reach_settings_push_text(commands, input->layout->content_title,
                             reach_settings_page_title(input->model->selected_page),
                             reach_settings_scale(input, REACH_TEXT_SIZE_XLARGE),
                             REACH_TEXT_WEIGHT_DEMIBOLD, input->text_alignment_leading,
                             input->theme->settings_text, 1);
    if (input->model->selected_page == REACH_SETTINGS_PAGE_UPDATE)
        render_update_page(input, commands);
    else if (input->model->selected_page == REACH_SETTINGS_PAGE_POWER_SLEEP)
        render_power_page(input, commands);
    else if (input->model->selected_page == REACH_SETTINGS_PAGE_ACCOUNT)
        render_account_page(input, commands);
    else if (input->model->selected_page == REACH_SETTINGS_PAGE_DISPLAY)
        render_display_page(input, commands);
    else if (input->model->selected_page == REACH_SETTINGS_PAGE_STARTUP_APPS)
        render_startup_apps_page(input, commands);
    else if (input->model->selected_page == REACH_SETTINGS_PAGE_WIFI)
        reach_settings_render_wifi_page(input, commands);
    else if (input->model->selected_page == REACH_SETTINGS_PAGE_BLUETOOTH)
        reach_settings_render_bluetooth_page(input, commands);
    else
        reach_settings_push_text(commands, input->layout->content_placeholder,
                                 reach_settings_page_placeholder(input->model->selected_page),
                                 reach_settings_scale(input, REACH_TEXT_SIZE_MEDIUM),
                                 REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading,
                                 input->theme->settings_secondary_text, 1);
    return REACH_OK;
}
