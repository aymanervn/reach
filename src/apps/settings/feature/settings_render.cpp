#include "reach/core/render_commands.h"
#include "reach/apps/settings/settings.h"
#include "reach/features/common/loader_render.h"
#include "reach/features/common/scrollbar_render.h"
#include "reach/features/common/ui_controls.h"

#include "settings_pages_internal.h"

static float scale_value(const reach_settings_render_input *input, float value)
{
    return value * (input->dpi_scale > 0.0f ? input->dpi_scale : 1.0f);
}

static void push_rect(reach_render_command_buffer *commands, reach_rect_f32 rect, float radius,
                      reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = rect;
    command.radius = radius;
    command.color = color;
    (void)reach_render_command_buffer_push(commands, &command);
}

static void push_stroke(reach_render_command_buffer *commands, reach_rect_f32 rect, float radius,
                        float width, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
    command.rect = rect;
    command.radius = radius;
    command.stroke_width = width;
    command.color = color;
    (void)reach_render_command_buffer_push(commands, &command);
}

static void push_masked_rect(reach_render_command_buffer *commands, reach_rect_f32 rect,
                             float radius, int32_t corner_mask, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = rect;
    command.radius = radius;
    command.corner_mask = corner_mask;
    command.color = color;
    (void)reach_render_command_buffer_push(commands, &command);
}

static void push_text(reach_render_command_buffer *commands, reach_rect_f32 rect,
                      const uint16_t *text, float size, int32_t weight, int32_t alignment,
                      reach_color color, int32_t ellipsis)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = rect;
    command.text_size = size;
    command.text_weight = weight;
    command.text_alignment = alignment;
    command.text_ellipsis = ellipsis;
    command.color = color;
    reach_copy_utf16(command.text, 260, text);
    (void)reach_render_command_buffer_push(commands, &command);
}

static void push_icon(reach_render_command_buffer *commands, reach_rect_f32 rect, reach_color color,
                      reach_vector_icon_id icon_id, float inset_ratio)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    float inset = rect.width * inset_ratio;
    command.rect = {rect.x + inset, rect.y + inset, rect.width - inset * 2.0f,
                    rect.height - inset * 2.0f};
    command.icon_id = icon_id;
    command.color = color;
    (void)reach_render_command_buffer_push(commands, &command);
}

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

static reach_color color_with_alpha(reach_color color, float alpha)
{
    color.a = alpha;
    return color;
}

static reach_ui_button_style settings_button_style(const reach_settings_render_input *input,
                                                   reach_color background)
{
    reach_ui_button_style style = {};
    style.background = background;
    style.radius = scale_value(input, input->theme->radius_small);
    style.disabled_background = input->theme->settings_button_disabled_background;
    style.text = input->theme->settings_button_text;
    style.disabled_text = input->theme->settings_secondary_text;
    style.text_size = scale_value(input, 13.0f);
    style.pressed_darken = input->theme->button_pressed_darken;
    style.text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
    return style;
}

static reach_ui_selection_item_style settings_pill_style(const reach_settings_render_input *input,
                                                         reach_color accent)
{
    reach_ui_selection_item_style style = {};
    style.background = input->theme->settings_pill_background;
    style.accent = accent;
    style.text = input->theme->settings_secondary_text;
    style.stroke_width = scale_value(input, 1.0f);
    style.text_size = scale_value(input, 11.0f);
    style.text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
    return style;
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

    push_rect(commands, layout->account_card, scale_value(input, input->theme->radius_small),
              input->theme->settings_card_background);

    const reach_rect_f32 avatar = layout->account_avatar;
    if (model->account_picture != 0)
    {
        push_avatar_image(commands, avatar, model->account_picture);
    }
    else
    {
        push_rect(commands, avatar, avatar.width * 0.5f, color_with_alpha(accent, 0.22f));
        uint16_t initial[2] = {reach_settings_account_initial(model), 0};
        push_text(commands, avatar, initial, avatar.width * 0.42f, REACH_TEXT_WEIGHT_DEMIBOLD,
                  REACH_TEXT_ALIGNMENT_CENTER, accent, 0);
    }
    reach_rect_f32 ring = {avatar.x - scale_value(input, 3.0f), avatar.y - scale_value(input, 3.0f),
                           avatar.width + scale_value(input, 6.0f),
                           avatar.height + scale_value(input, 6.0f)};
    push_stroke(commands, ring, ring.width * 0.5f, scale_value(input, 1.5f),
                color_with_alpha(accent, 0.55f));

    const uint16_t *display_name = model->account_display_name[0] != 0
                                       ? model->account_display_name
                                       : (const uint16_t *)L"Windows user";
    push_text(commands, layout->account_name, display_name, scale_value(input, 20.0f),
              REACH_TEXT_WEIGHT_DEMIBOLD, input->text_alignment_leading,
              input->theme->settings_text, 1);
    if (model->account_user_name[0] != 0)
    {
        push_text(commands, layout->account_user, model->account_user_name,
                  scale_value(input, 11.5f), REACH_TEXT_WEIGHT_NORMAL,
                  input->text_alignment_leading, input->theme->settings_secondary_text, 1);
    }
    reach_ui_selection_item_style badge_style = settings_pill_style(input, accent);
    reach_ui_selection_item_render(commands, layout->account_type_badge,
                                   reach_settings_account_type_label(model->account_is_admin),
                                   &badge_style, 1.0f);

    push_rect(commands, layout->account_password_card,
              scale_value(input, input->theme->radius_small),
              input->theme->settings_card_background);
    push_rect(commands, layout->account_password_icon,
              scale_value(input, input->theme->radius_small),
              input->theme->settings_input_background);
    push_icon(commands, layout->account_password_icon, input->theme->settings_secondary_text,
              REACH_VECTOR_ICON_LOCK, 0.22f);
    push_text(commands, layout->account_password_title, (const uint16_t *)L"Password",
              scale_value(input, 13.5f), REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
              input->theme->settings_text, 1);
    push_text(commands, layout->account_password_subtitle,
              (const uint16_t *)L"Change the password you use to sign in to Windows",
              scale_value(input, 10.5f), REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading,
              input->theme->settings_secondary_text, 1);

    if (model->account_status != REACH_SETTINGS_ACCOUNT_STATUS_NONE)
    {
        reach_color status_color = model->account_status == REACH_SETTINGS_ACCOUNT_STATUS_SUCCESS
                                       ? input->theme->settings_status_success
                                       : input->theme->settings_status_error;
        push_text(commands, layout->account_password_status,
                  reach_settings_account_status_message(model->account_status),
                  scale_value(input, 10.5f), REACH_TEXT_WEIGHT_SEMIBOLD,
                  REACH_TEXT_ALIGNMENT_TRAILING, status_color, 1);
    }

    static const uint16_t *placeholders[REACH_SETTINGS_ACCOUNT_FIELD_COUNT] = {
        (const uint16_t *)L"Current password", (const uint16_t *)L"New password",
        (const uint16_t *)L"Confirm password"};
    reach_ui_selection_item_style field_style = settings_pill_style(input, accent);
    field_style.background = input->theme->settings_input_background;
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
        state.placeholder_color = color_with_alpha(input->theme->settings_secondary_text, 0.55f);
        state.selection_color = color_with_alpha(accent, 0.30f);
        float text_inset = scale_value(input, 13.0f);
        reach_rect_f32 text_rect = {field_rect.x + text_inset, field_rect.y,
                                    field_rect.width - text_inset * 2.0f, field_rect.height};
        reach_ui_textbox_render(commands, text_rect, &field_style, focused ? 1.0f : 0.0f, &state);
    }

    reach_ui_button_style change_style =
        settings_button_style(input, input->theme->settings_button_primary);
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
        push_rect(commands, layout->power_cards[timer],
                  scale_value(input, input->theme->radius_small),
                  input->theme->settings_card_background);
        push_rect(commands, layout->power_icon_boxes[timer],
                  scale_value(input, input->theme->radius_small),
                  input->theme->settings_input_background);
        push_icon(commands, layout->power_icon_boxes[timer], input->theme->settings_secondary_text,
                  (reach_vector_icon_id)style->icon_id, 0.22f);
        push_text(commands, layout->power_titles[timer], style->title, scale_value(input, 13.5f),
                  REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
                  input->theme->settings_text, 1);
        if (layout->power_subtitles[timer].height > 0.0f)
        {
            push_text(commands, layout->power_subtitles[timer], style->subtitle,
                      scale_value(input, 10.5f), REACH_TEXT_WEIGHT_NORMAL,
                      input->text_alignment_leading, input->theme->settings_secondary_text, 1);
        }
        if (layout->power_wait_toggles[timer].width > 0.0f)
        {
            float wait_t = reach_animation_manager_value(&model->power_wait_animations, timer);
            push_text(
                commands, layout->power_wait_labels[timer],
                (const uint16_t *)L"Wait for apps keeping the PC awake", scale_value(input, 10.5f),
                REACH_TEXT_WEIGHT_NORMAL, REACH_TEXT_ALIGNMENT_TRAILING,
                color_with_alpha(input->theme->settings_secondary_text, 0.7f + 0.3f * wait_t), 1);
            reach_ui_toggle_style toggle_style = {};
            toggle_style.track_off = input->theme->settings_toggle_track_off;
            toggle_style.track_on = color_with_alpha(accent, 0.85f);
            toggle_style.knob = input->theme->settings_toggle_knob;
            reach_ui_toggle_render(commands, layout->power_wait_toggles[timer], &toggle_style,
                                   wait_t);
        }

        float t = reach_animation_manager_value(&model->power_animations, timer);
        size_t selected = model->power_selected[timer];
        size_t previous = model->power_previous[timer];
        reach_ui_selection_item_style pill_style = settings_pill_style(input, accent);
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
                reach_ui_selection_item_backdrop_render(commands, slot, &pill_style, selection);
                const reach_rect_f32 hours =
                    layout->power_custom_fields[timer][REACH_SETTINGS_POWER_FIELD_HOURS];
                const reach_rect_f32 minutes =
                    layout->power_custom_fields[timer][REACH_SETTINGS_POWER_FIELD_MINUTES];
                float divider_x = (hours.x + hours.width + minutes.x) * 0.5f;
                push_rect(commands,
                          {divider_x, slot.y + slot.height * 0.25f, scale_value(input, 1.0f),
                           slot.height * 0.5f},
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
                    state.suffix_width = scale_value(
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
                    state.placeholder_color =
                        color_with_alpha(input->theme->settings_secondary_text, 0.55f);
                    state.selection_color = color_with_alpha(accent, 0.30f);
                    state.suffix_color =
                        color_with_alpha(input->theme->settings_secondary_text, 0.65f);
                    reach_ui_textbox_render(commands, layout->power_custom_fields[timer][field],
                                            &pill_style, selection, &state);
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
        settings_button_style(input, input->theme->settings_button_primary);
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
    float radius = scale_value(input, input->theme->radius_small);
    push_rect(commands, card->card, radius, input->theme->settings_card_background);
    push_rect(commands, card->icon, radius, input->theme->settings_input_background);
    if (card->icon_text != nullptr)
    {
        push_text(commands, card->icon, (const uint16_t *)card->icon_text,
                  scale_value(input, 18.0f), REACH_TEXT_WEIGHT_EXTRABOLD,
                  REACH_TEXT_ALIGNMENT_CENTER, input->theme->settings_secondary_text, 0);
    }
    else
    {
        push_icon(commands, card->icon, input->theme->settings_secondary_text, card->icon_id,
                  0.22f);
    }
    push_text(commands, card->title, (const uint16_t *)card->title_text,
              scale_value(input, 13.5f), REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
              input->theme->settings_text, 1);
    push_text(commands, card->subtitle, (const uint16_t *)card->subtitle_text,
              scale_value(input, 10.5f), REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading,
              input->theme->settings_secondary_text, 1);
    reach_ui_toggle_render(commands, card->toggle, toggle_style, card->position);
}

static void render_display_page(const reach_settings_render_input *input,
                                reach_render_command_buffer *commands)
{
    const reach_settings_model *model = input->model;
    const reach_settings_layout *layout = input->layout;
    reach_color accent = reach_theme_accent_color(input->theme, REACH_THEME_ACCENT_CYAN);

    reach_ui_toggle_style toggle_style = {};
    toggle_style.track_off = input->theme->settings_toggle_track_off;
    toggle_style.track_on = color_with_alpha(accent, 0.85f);
    toggle_style.knob = input->theme->settings_toggle_knob;

    const display_toggle_card cards[] = {
        {layout->display_fps_card, layout->display_fps_icon, layout->display_fps_title,
         layout->display_fps_subtitle, layout->display_fps_toggle, REACH_VECTOR_ICON_ARROW_UP,
         nullptr, L"Smoother animations", L"Run animations at 120fps",
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
}

static void push_app_icon(reach_render_command_buffer *commands, reach_rect_f32 rect,
                          uint64_t icon_id, float opacity)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_ICON;
    command.rect = rect;
    command.icon_id = icon_id;
    command.color = {1.0f, 1.0f, 1.0f, opacity};
    (void)reach_render_command_buffer_push(commands, &command);
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
    push_text(commands, layout->startup_summary, summary, scale_value(input, 11.0f),
              REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
              input->theme->settings_secondary_text, 1);

    if (model->startup_status != REACH_SETTINGS_STARTUP_STATUS_NONE)
    {
        reach_color status_color = model->startup_status == REACH_SETTINGS_STARTUP_STATUS_FAILED
                                       ? input->theme->settings_status_error
                                       : input->theme->settings_secondary_text;
        push_text(commands, layout->startup_summary,
                  reach_settings_startup_status_message(model->startup_status),
                  scale_value(input, 11.0f), REACH_TEXT_WEIGHT_SEMIBOLD,
                  REACH_TEXT_ALIGNMENT_TRAILING, status_color, 1);
    }

    if (layout->startup_row_count == 0)
    {
        push_text(commands, layout->startup_viewport,
                  model->startup_loaded ? (const uint16_t *)u"Nothing runs at sign-in."
                                        : (const uint16_t *)u"Reading startup apps...",
                  scale_value(input, 14.0f), REACH_TEXT_WEIGHT_NORMAL, REACH_TEXT_ALIGNMENT_CENTER,
                  input->theme->settings_secondary_text, 1);
        return;
    }

    reach_ui_selection_item_style badge_style =
        settings_pill_style(input, input->theme->settings_secondary_text);
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
        push_rect(commands, row, scale_value(input, input->theme->radius_small),
                  input->theme->settings_card_background);

        float icon_box_size = scale_value(input, 36.0f);
        reach_rect_f32 icon_box = {row.x + scale_value(input, 14.0f),
                                   row.y + (row.height - icon_box_size) * 0.5f, icon_box_size,
                                   icon_box_size};
        push_rect(commands, icon_box, scale_value(input, input->theme->radius_small),
                  input->theme->settings_input_background);
        if (model->startup_icons[index] != 0)
        {
            float inset = scale_value(input, 6.0f);
            reach_rect_f32 image = {icon_box.x + inset, icon_box.y + inset,
                                    icon_box.width - inset * 2.0f, icon_box.height - inset * 2.0f};
            push_app_icon(commands, image, model->startup_icons[index], 1.0f);
        }
        else
        {
            push_icon(commands, icon_box, input->theme->settings_secondary_text,
                      REACH_VECTOR_ICON_EXECUTABLE, 0.26f);
        }

        const reach_rect_f32 toggle = layout->startup_toggles[index];
        float badge_width = scale_value(input, 86.0f);
        float badge_height = scale_value(input, 22.0f);
        reach_rect_f32 badge = {toggle.x - scale_value(input, 16.0f) - badge_width,
                                row.y + (row.height - badge_height) * 0.5f, badge_width,
                                badge_height};
        float badge_accent = reach_startup_app_source_is_machine(entry->source) ? 0.0f : 1.0f;
        reach_ui_selection_item_render(commands, badge,
                                       reach_startup_app_source_label(entry->source), &badge_style,
                                       badge_accent);

        float text_x = icon_box.x + icon_box.width + scale_value(input, 14.0f);
        float text_width = badge.x - scale_value(input, 14.0f) - text_x;
        if (text_width < 0.0f)
        {
            text_width = 0.0f;
        }
        push_text(
            commands,
            {text_x, row.y + scale_value(input, 12.0f), text_width, scale_value(input, 18.0f)},
            entry->display_name, scale_value(input, 13.5f), REACH_TEXT_WEIGHT_SEMIBOLD,
            input->text_alignment_leading, input->theme->settings_text, 1);

        const uint16_t *detail = entry->executable[0] != 0 ? entry->executable : entry->command;
        push_text(
            commands,
            {text_x, row.y + scale_value(input, 33.0f), text_width, scale_value(input, 15.0f)},
            detail, scale_value(input, 10.5f), REACH_TEXT_WEIGHT_NORMAL,
            input->text_alignment_leading, input->theme->settings_secondary_text, 1);

        reach_ui_toggle_style toggle_style = {};
        toggle_style.track_off = input->theme->settings_toggle_track_off;
        toggle_style.track_on = color_with_alpha(accent, 0.85f);
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

    push_text(commands, layout->reach_section_title, (const uint16_t *)u"Reach",
              scale_value(input, 11.0f), REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
              input->theme->settings_secondary_text, 1);

    const reach_rect_f32 row = layout->reach_update_row;
    push_rect(commands, row, scale_value(input, input->theme->radius_small),
              input->theme->settings_card_background);

    reach_rect_f32 icon_box = {row.x + scale_value(input, 12.0f),
                               row.y + (row.height - scale_value(input, 34.0f)) * 0.5f,
                               scale_value(input, 34.0f), scale_value(input, 34.0f)};
    push_rect(commands, icon_box, scale_value(input, input->theme->radius_small),
              input->theme->settings_input_background);
    push_icon(commands, icon_box, input->theme->settings_secondary_text, REACH_VECTOR_ICON_RESTART,
              0.22f);

    float left = icon_box.x + icon_box.width + scale_value(input, 12.0f);
    float width = layout->reach_update_button.x - left - scale_value(input, 12.0f);
    uint16_t version_line[128] = {};
    build_reach_version_line(model, version_line, 128);
    push_text(commands, {left, row.y + scale_value(input, 12.0f), width, scale_value(input, 18.0f)},
              version_line, scale_value(input, 13.5f), REACH_TEXT_WEIGHT_SEMIBOLD,
              input->text_alignment_leading, input->theme->settings_text, 1);

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
    push_text(commands, {left, row.y + scale_value(input, 34.0f), width, scale_value(input, 14.0f)},
              status, scale_value(input, 10.5f), REACH_TEXT_WEIGHT_NORMAL,
              input->text_alignment_leading, input->theme->settings_secondary_text, 1);

    int32_t busy = model->reach_update_state == REACH_SETTINGS_REACH_UPDATE_CHECKING ||
                   model->reach_update_state == REACH_SETTINGS_REACH_UPDATE_DOWNLOADING;
    int32_t enabled = !busy && model->reach_update_state != REACH_SETTINGS_REACH_UPDATE_UP_TO_DATE;
    reach_color button_color = model->reach_update_state == REACH_SETTINGS_REACH_UPDATE_AVAILABLE
                                   ? input->theme->settings_button_success
                                   : input->theme->settings_card_background;
    reach_ui_button_style reach_style = settings_button_style(input, button_color);
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
        settings_button_style(input, input->theme->settings_button_success);
    reach_ui_button_render(
        commands, layout->update_refresh_button, scan_button_text, &refresh_style, !busy,
        reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_UPDATE_REFRESH));
    reach_ui_button_style install_style =
        settings_button_style(input, input->theme->settings_button_primary);
    reach_ui_button_render(
        commands, layout->update_install_button, (const uint16_t *)u"Install selected",
        &install_style, install_enabled,
        reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_UPDATE_INSTALL));
    reach_ui_button_style restart_style =
        settings_button_style(input, input->theme->settings_button_danger);
    reach_ui_button_render(
        commands, layout->update_restart_button, (const uint16_t *)u"Restart now", &restart_style,
        restart_enabled,
        reach_settings_model_button_press_value(model, REACH_SETTINGS_HIT_UPDATE_RESTART));

    render_reach_section(input, commands);

    push_text(commands, layout->windows_section_title, (const uint16_t *)u"Windows updates",
              scale_value(input, 11.0f), REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
              input->theme->settings_secondary_text, 1);

    reach_rect_f32 update_status_message = layout->update_viewport;

    if (model->update_page_state == REACH_SETTINGS_UPDATE_SCANNING)
    {
        push_text(commands, update_status_message, (const uint16_t *)u"Searching for updates\u2026",
                  scale_value(input, 14.0f), REACH_TEXT_WEIGHT_NORMAL, REACH_TEXT_ALIGNMENT_CENTER,
                  input->theme->settings_secondary_text, 1);

        float loader_width = scale_value(input, 220.0f);
        float loader_height = scale_value(input, 4.0f);
        if (loader_width > update_status_message.width)
            loader_width = update_status_message.width;

        reach_rect_f32 loader_container = {
            update_status_message.x + (update_status_message.width - loader_width) * 0.5f,
            update_status_message.y + update_status_message.height * 0.5f +
                scale_value(input, 18.0f),
            loader_width, loader_height};

        reach_rect_f32 loader_bar =
            reach_loader_bar_rect(&model->update_loader, loader_container);
        reach_rect_f32 loader_origin = {0.0f, 0.0f, 0.0f, 0.0f};
        (void)reach_loader_build_render_commands(loader_container, loader_bar, loader_origin,
                                                 accent, commands);
        return;
    }
    else if (model->update_page_state == REACH_SETTINGS_UPDATE_ERROR &&
             layout->update_row_count == 0)
        push_text(commands, layout->update_viewport,
                  (const uint16_t *)u"Unable to refresh Windows updates.",
                  scale_value(input, 14.0f), REACH_TEXT_WEIGHT_NORMAL,
                  input->text_alignment_leading, input->theme->settings_status_error, 1);
    else if (model->update_scan_completed && layout->update_row_count == 0)
        push_text(commands, update_status_message, (const uint16_t *)u"Windows is up to date.",
                  scale_value(input, 14.0f), REACH_TEXT_WEIGHT_NORMAL, REACH_TEXT_ALIGNMENT_CENTER,
                  input->theme->settings_secondary_text, 1);
    else if (!model->update_scan_completed && layout->update_row_count == 0)
        push_text(commands, update_status_message, (const uint16_t *)u"Search for updates",
                  scale_value(input, 14.0f), REACH_TEXT_WEIGHT_NORMAL, REACH_TEXT_ALIGNMENT_CENTER,
                  input->theme->settings_secondary_text, 1);

    static const uint16_t *section_titles[] = {(const uint16_t *)u"Select updates",
                                               (const uint16_t *)u"Restart required",
                                               (const uint16_t *)u"Failed"};
    reach_render_command_buffer_set_scissor(commands, layout->update_viewport);
    for (size_t index = 0; index < layout->update_section_count; ++index)
    {
        const reach_rect_f32 title = layout->update_section_titles[index];
        push_text(commands, title, section_titles[layout->update_section_ids[index]],
                  scale_value(input, 11.0f), REACH_TEXT_WEIGHT_SEMIBOLD,
                  input->text_alignment_leading, input->theme->settings_secondary_text, 1);
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
        push_rect(commands, row, scale_value(input, input->theme->radius_small), row_color);
        if (update->state == REACH_WINDOWS_UPDATE_INSTALLED_REBOOT_REQUIRED)
            push_icon(commands, checkbox, input->theme->settings_status_success,
                      REACH_VECTOR_ICON_CHECK, 0.06f);
        else if (update->state == REACH_WINDOWS_UPDATE_FAILED)
            push_icon(commands, checkbox, input->theme->settings_status_error,
                      REACH_VECTOR_ICON_CLOSE, 0.06f);
        else
        {
            push_stroke(commands, checkbox, scale_value(input, input->theme->radius_small),
                        scale_value(input, 1.5f),
                        update->selected ? accent : input->theme->settings_secondary_text);
            if (update->selected)
            {
                push_rect(commands, checkbox, scale_value(input, input->theme->radius_small),
                          accent);
                push_icon(commands, checkbox, input->theme->inverse_text, REACH_VECTOR_ICON_CHECK,
                          0.18f);
            }
        }

        float left = checkbox.x + checkbox.width + scale_value(input, 10.0f);
        float width = row.x + row.width - left - scale_value(input, 10.0f);
        push_text(commands,
                  {left, row.y + scale_value(input, 6.0f), width, scale_value(input, 18.0f)},
                  update->identity.title, scale_value(input, 13.0f), REACH_TEXT_WEIGHT_SEMIBOLD,
                  input->text_alignment_leading, input->theme->settings_text, 1);
        uint16_t metadata[260] = {};
        build_metadata_text(update, metadata, 260);
        push_text(commands,
                  {left, row.y + scale_value(input, 30.0f), width, scale_value(input, 14.0f)},
                  metadata, scale_value(input, 10.5f), REACH_TEXT_WEIGHT_NORMAL,
                  input->text_alignment_leading, input->theme->settings_secondary_text, 1);
        uint16_t status[260] = {};
        build_status_text(update, status, 260);
        push_text(commands,
                  {left, row.y + scale_value(input, 47.0f), width, scale_value(input, 14.0f)},
                  status, scale_value(input, 10.0f), REACH_TEXT_WEIGHT_NORMAL,
                  input->text_alignment_leading, input->theme->settings_secondary_text, 1);
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
    push_rect(commands, input->layout->bounds, scale_value(input, input->theme->radius_large),
              input->theme->settings_background);
    push_masked_rect(commands, input->layout->nav, scale_value(input, input->theme->radius_large),
                     REACH_RENDER_CORNER_TOP_LEFT | REACH_RENDER_CORNER_BOTTOM_LEFT,
                     input->theme->settings_nav_background);
    push_rect(commands, input->layout->close_button, input->layout->close_button.width * 0.5f,
              input->model->hovered_button == REACH_SETTINGS_HIT_CLOSE
                  ? input->theme->settings_window_close_hover
                  : input->theme->settings_window_button_background);
    push_rect(commands, input->layout->minimize_button, input->layout->minimize_button.width * 0.5f,
              input->model->hovered_button == REACH_SETTINGS_HIT_MINIMIZE
                  ? input->theme->settings_window_minimize_hover
                  : input->theme->settings_window_button_background);
    push_icon(commands, input->layout->close_button, input->theme->inverse_text,
              REACH_VECTOR_ICON_CLOSE, 0.24f);
    push_icon(commands, input->layout->minimize_button, input->theme->inverse_text,
              REACH_VECTOR_ICON_MINIMIZE, 0.24f);

    size_t nav_count = 0;
    const reach_settings_nav_item *items = reach_settings_nav_items(&nav_count);
    for (size_t index = 0; index < input->layout->nav_item_count && index < nav_count; ++index)
    {
        const reach_settings_nav_item_layout *item_layout = &input->layout->nav_items[index];
        const reach_settings_nav_item *item = &items[index];
        const reach_color accent = reach_theme_accent_color(input->theme, item->accent);
        const reach_color accent_background =
            color_with_alpha(accent, input->theme->accent_tint_alpha);
        if (input->model->selected_page == item->page)
            push_rect(commands, item_layout->bounds, scale_value(input, input->theme->radius_small),
                      accent_background);
        push_rect(commands, item_layout->icon_background,
                  scale_value(input, input->theme->radius_small), accent_background);
        push_icon(commands, item_layout->icon, accent, (reach_vector_icon_id)item->icon_id, 0.0f);
        push_text(commands, item_layout->label, item->label, scale_value(input, 13.0f),
                  REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
                  input->theme->settings_text, 1);
    }

    uint16_t footer_text[64] = {};
    append_text(footer_text, 64, (const uint16_t *)u"Reach v");
    append_text(footer_text, 64,
                input->model->reach_current_version[0] != 0 ? input->model->reach_current_version
                                                            : (const uint16_t *)u"?");
    push_text(commands, input->layout->nav_footer, footer_text, scale_value(input, 11.0f),
              REACH_TEXT_WEIGHT_NORMAL, input->text_alignment_leading,
              color_with_alpha(input->theme->settings_secondary_text, 0.7f), 1);

    push_text(commands, input->layout->content_title,
              reach_settings_page_title(input->model->selected_page), scale_value(input, 28.0f),
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
    else
        push_text(commands, input->layout->content_placeholder,
                  reach_settings_page_placeholder(input->model->selected_page),
                  scale_value(input, 15.0f), REACH_TEXT_WEIGHT_NORMAL,
                  input->text_alignment_leading, input->theme->settings_secondary_text, 1);
    return REACH_OK;
}
