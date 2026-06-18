#include "reach/core/render_commands.h"
#include "reach/features/settings.h"
#include "reach/features/windows_update.h"

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

static void push_icon(reach_render_command_buffer *commands, reach_rect_f32 rect,
                      reach_color color, reach_vector_icon_id icon_id, float inset_ratio)
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
    while (length < capacity && destination[length] != 0) ++length;
    size_t index = 0;
    while (source != nullptr && source[index] != 0 && length + 1 < capacity)
        destination[length++] = source[index++];
    if (length < capacity) destination[length] = 0;
}

static void build_metadata_text(const reach_windows_update_item *update, uint16_t *text,
                                size_t capacity)
{
    text[0] = 0;
    append_text(text, capacity, (const uint16_t *)u"KB: ");
    append_text(text, capacity, update->identity.kb_article_ids[0] != 0
                                    ? update->identity.kb_article_ids
                                    : (const uint16_t *)u"N/A");
    append_text(text, capacity, (const uint16_t *)u"   Category: ");
    append_text(text, capacity, update->categories[0] != 0
                                    ? update->categories
                                    : (const uint16_t *)u"N/A");
}

static void append_integer(uint16_t *destination, size_t capacity, int32_t value)
{
    uint16_t digits[16] = {};
    size_t count = 0;
    uint32_t remaining = value < 0 ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
    do { digits[count++] = (uint16_t)(u'0' + remaining % 10); remaining /= 10; } while (remaining && count < 15);
    if (value < 0 && count < 15) digits[count++] = u'-';
    uint16_t forward[16] = {};
    for (size_t index = 0; index < count; ++index) forward[index] = digits[count - index - 1];
    append_text(destination, capacity, forward);
}

static void build_status_text(const reach_windows_update_item *update, uint16_t *text,
                              size_t capacity)
{
    text[0] = 0;
    append_text(text, capacity, (const uint16_t *)u"Status: ");
    append_text(text, capacity, reach_windows_update_state_label(update->state));
    append_text(text, capacity, (const uint16_t *)u"   Downloaded: ");
    append_text(text, capacity, update->downloaded ? (const uint16_t *)u"Yes" : (const uint16_t *)u"No");
    append_text(text, capacity, (const uint16_t *)u"   Restart: ");
    append_text(text, capacity, !update->reboot_required_known ? (const uint16_t *)u"Unknown" :
                                update->reboot_required ? (const uint16_t *)u"Required" : (const uint16_t *)u"No");
}

static void build_identity_text(const reach_windows_update_item *update, uint16_t *text,
                                size_t capacity)
{
    text[0] = 0;
    append_text(text, capacity, (const uint16_t *)u"ID: ");
    append_text(text, capacity, update->identity.update_id);
    append_text(text, capacity, (const uint16_t *)u"   Revision: ");
    append_integer(text, capacity, update->identity.revision_number);
    append_text(text, capacity, (const uint16_t *)u"   Reason: ");
    append_text(text, capacity, update->selected_reason[0]
                                    ? update->selected_reason
                                    : (update->selected ? (const uint16_t *)u"Manual"
                                                        : (const uint16_t *)u"Not selected"));
}

static void render_update_page(const reach_settings_render_input *input,
                               reach_render_command_buffer *commands)
{
    const reach_settings_model *model = input->model;
    const reach_settings_layout *layout = input->layout;
    const int32_t busy = reach_settings_model_update_busy(model);
    const int32_t install_enabled = !busy && reach_settings_model_selected_update_count(model) > 0;
    reach_color accent = {0.20f, 0.72f, 0.96f, 1.0f};
    reach_color enabled_button = {0.12f, 0.43f, 0.62f, 1.0f};
    reach_color disabled_button = {0.22f, 0.25f, 0.28f, 0.72f};

    push_rect(commands, layout->update_search_button, scale_value(input, 8.0f),
              busy ? disabled_button : enabled_button);
    push_text(commands, layout->update_search_button,
              busy && model->update_page_state == REACH_SETTINGS_UPDATE_SCANNING
                  ? (const uint16_t *)u"Scanning..."
                  : (const uint16_t *)u"Search for updates",
              scale_value(input, 13.0f), REACH_TEXT_WEIGHT_SEMIBOLD,
              REACH_TEXT_ALIGNMENT_CENTER, input->theme->settings_text, 1);
    push_rect(commands, layout->update_install_button, scale_value(input, 8.0f),
              install_enabled ? enabled_button : disabled_button);
    push_text(commands, layout->update_install_button, (const uint16_t *)u"Install selected",
              scale_value(input, 13.0f), REACH_TEXT_WEIGHT_SEMIBOLD,
              REACH_TEXT_ALIGNMENT_CENTER,
              install_enabled ? input->theme->settings_text : input->theme->settings_secondary_text,
              1);

    size_t visible_count = model->update_list.count < layout->update_row_count
                               ? model->update_list.count
                               : layout->update_row_count;
    for (size_t index = 0; index < visible_count; ++index)
    {
        size_t update_index = model->update_scroll_offset + index;
        if (update_index >= model->update_list.count) break;
        const reach_windows_update_item *update = &model->update_list.updates[update_index];
        const reach_rect_f32 row = layout->update_rows[index];
        const reach_rect_f32 checkbox = layout->update_checkboxes[index];
        reach_color row_color = {0.12f, 0.15f, 0.18f, 0.82f};
        push_rect(commands, row, scale_value(input, 8.0f), row_color);
        push_stroke(commands, checkbox, scale_value(input, 4.0f), scale_value(input, 1.5f),
                    update->selected ? accent : input->theme->settings_secondary_text);
        if (update->selected)
        {
            push_rect(commands, checkbox, scale_value(input, 4.0f), accent);
            push_icon(commands, checkbox, input->theme->dark_text, REACH_VECTOR_ICON_CHECK, 0.18f);
        }

        float left = checkbox.x + checkbox.width + scale_value(input, 10.0f);
        float width = row.x + row.width - left - scale_value(input, 10.0f);
        push_text(commands, {left, row.y + scale_value(input, 5.0f), width,
                             scale_value(input, 18.0f)},
                  update->identity.title, scale_value(input, 13.0f), REACH_TEXT_WEIGHT_SEMIBOLD,
                  input->text_alignment_leading, input->theme->settings_text, 1);
        uint16_t metadata[260] = {};
        build_metadata_text(update, metadata, 260);
        push_text(commands, {left, row.y + scale_value(input, 21.0f), width,
                             scale_value(input, 14.0f)},
                  metadata, scale_value(input, 10.5f), REACH_TEXT_WEIGHT_NORMAL,
                  input->text_alignment_leading, input->theme->settings_secondary_text, 1);
        uint16_t status[260] = {};
        build_status_text(update, status, 260);
        push_text(commands, {left, row.y + scale_value(input, 36.0f), width,
                             scale_value(input, 14.0f)},
                  status, scale_value(input, 10.0f), REACH_TEXT_WEIGHT_NORMAL,
                  input->text_alignment_leading, input->theme->settings_secondary_text, 1);
        uint16_t identity[260] = {};
        build_identity_text(update, identity, 260);
        push_text(commands, {left, row.y + scale_value(input, 51.0f), width,
                             scale_value(input, 13.0f)},
                  identity, scale_value(input, 9.5f), REACH_TEXT_WEIGHT_NORMAL,
                  input->text_alignment_leading, input->theme->settings_secondary_text, 1);
    }

    push_text(commands, layout->update_status,
              model->update_status[0] != 0 ? model->update_status
                                           : (const uint16_t *)u"No scan has been run yet.",
              scale_value(input, 12.0f), REACH_TEXT_WEIGHT_NORMAL,
              input->text_alignment_leading,
              model->update_page_state == REACH_SETTINGS_UPDATE_ERROR
                  ? reach_color{0.96f, 0.38f, 0.34f, 1.0f}
                  : input->theme->settings_secondary_text,
              1);
}

reach_result reach_settings_build_render_commands(const reach_settings_render_input *input,
                                                  reach_render_command_buffer *commands)
{
    if (input == nullptr || input->theme == nullptr || input->model == nullptr ||
        input->layout == nullptr || commands == nullptr) return REACH_INVALID_ARGUMENT;
    reach_render_command_buffer_clear(commands);
    float scale = input->dpi_scale > 0.0f ? input->dpi_scale : 1.0f;
    reach_color background = input->theme->dark_background;
    background.a = 0.96f;
    push_rect(commands, input->layout->bounds, 18.0f * scale, background);
    push_masked_rect(commands, input->layout->nav, 18.0f * scale,
                     REACH_RENDER_CORNER_TOP_LEFT | REACH_RENDER_CORNER_BOTTOM_LEFT,
                     {0.08f, 0.11f, 0.14f, 0.64f});
    push_rect(commands, input->layout->close_button,
              input->layout->close_button.width * 0.5f, {0.92f, 0.28f, 0.28f, 1.0f});
    push_rect(commands, input->layout->minimize_button,
              input->layout->minimize_button.width * 0.5f, {0.94f, 0.72f, 0.20f, 1.0f});
    push_icon(commands, input->layout->close_button, input->theme->dark_text,
              REACH_VECTOR_ICON_CLOSE, 0.24f);
    push_icon(commands, input->layout->minimize_button, input->theme->dark_text,
              REACH_VECTOR_ICON_MINIMIZE, 0.24f);

    size_t nav_count = 0;
    const reach_settings_nav_item *items = reach_settings_nav_items(&nav_count);
    for (size_t index = 0; index < input->layout->nav_item_count && index < nav_count; ++index)
    {
        const reach_settings_nav_item_layout *item_layout = &input->layout->nav_items[index];
        const reach_settings_nav_item *item = &items[index];
        if (input->model->selected_page == item->page)
            push_rect(commands, item_layout->bounds, scale_value(input, 8.0f),
                      item->accent_background);
        push_rect(commands, item_layout->icon_background, scale_value(input, 9.0f),
                  item->accent_background);
        push_icon(commands, item_layout->icon, item->accent,
                  (reach_vector_icon_id)item->icon_id, 0.0f);
        push_text(commands, item_layout->label, item->label, scale_value(input, 13.0f),
                  REACH_TEXT_WEIGHT_SEMIBOLD, input->text_alignment_leading,
                  input->theme->settings_text, 1);
    }

    push_text(commands, input->layout->content_title,
              reach_settings_page_title(input->model->selected_page), scale_value(input, 28.0f),
              REACH_TEXT_WEIGHT_DEMIBOLD, input->text_alignment_leading,
              input->theme->settings_text, 1);
    if (input->model->selected_page == REACH_SETTINGS_PAGE_UPDATE)
        render_update_page(input, commands);
    else
        push_text(commands, input->layout->content_placeholder,
                  reach_settings_page_placeholder(input->model->selected_page),
                  scale_value(input, 15.0f), REACH_TEXT_WEIGHT_NORMAL,
                  input->text_alignment_leading, input->theme->settings_secondary_text, 1);
    return REACH_OK;
}
