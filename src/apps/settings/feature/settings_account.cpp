#include "settings_pages_internal.h"

const uint16_t *reach_settings_account_page_title(void)
{
    return (const uint16_t *)L"Account";
}

const uint16_t *reach_settings_account_page_placeholder(void)
{
    return (const uint16_t *)L"Account settings page";
}

void reach_settings_model_set_account(reach_settings_model *model, const uint16_t *display_name,
                                      const uint16_t *user_name, int32_t is_admin,
                                      uint64_t picture)
{
    REACH_ASSERT(model != nullptr);
    if (model == nullptr)
    {
        return;
    }
    reach_copy_utf16(model->account_display_name, REACH_SETTINGS_ACCOUNT_NAME_CAPACITY,
                     display_name);
    reach_copy_utf16(model->account_user_name, REACH_SETTINGS_ACCOUNT_NAME_CAPACITY, user_name);
    model->account_is_admin = is_admin ? 1 : 0;
    model->account_picture = picture;
}

const uint16_t *reach_settings_account_type_label(int32_t is_admin)
{
    return is_admin ? (const uint16_t *)L"Administrator" : (const uint16_t *)L"Standard user";
}

uint16_t reach_settings_account_initial(const reach_settings_model *model)
{
    if (model == nullptr || model->account_display_name[0] == 0)
    {
        return (uint16_t)'?';
    }
    uint16_t initial = model->account_display_name[0];
    if (initial >= (uint16_t)'a' && initial <= (uint16_t)'z')
    {
        initial = (uint16_t)(initial - (uint16_t)'a' + (uint16_t)'A');
    }
    return initial;
}

static reach_text_edit *reach_settings_account_focused_edit(reach_settings_model *model)
{
    if (model == nullptr || model->account_focused_field < 0 ||
        model->account_focused_field >= REACH_SETTINGS_ACCOUNT_FIELD_COUNT)
    {
        return nullptr;
    }
    return &model->account_password_edits[model->account_focused_field];
}

void reach_settings_model_account_focus_password(reach_settings_model *model, size_t field)
{
    REACH_ASSERT(model != nullptr);
    REACH_ASSERT(field < REACH_SETTINGS_ACCOUNT_FIELD_COUNT);
    if (model == nullptr || field >= REACH_SETTINGS_ACCOUNT_FIELD_COUNT)
    {
        return;
    }
    reach_text_edit_select_all(&model->account_password_edits[field]);
    model->account_focused_field = (int32_t)field;
    model->account_caret_visible = 1;
    model->account_caret_phase = 0.0;
    if (model->account_status != REACH_SETTINGS_ACCOUNT_STATUS_SUCCESS)
    {
        model->account_status = REACH_SETTINGS_ACCOUNT_STATUS_NONE;
    }
}

void reach_settings_model_account_blur(reach_settings_model *model)
{
    if (model == nullptr || model->account_focused_field < 0)
    {
        return;
    }
    for (size_t field = 0; field < REACH_SETTINGS_ACCOUNT_FIELD_COUNT; ++field)
    {
        model->account_password_edits[field].selection_anchor = -1;
    }
    model->account_focused_field = -1;
}

int32_t reach_settings_model_account_insert_char(reach_settings_model *model, uint16_t ch)
{
    reach_text_edit *edit = reach_settings_account_focused_edit(model);
    if (edit == nullptr || ch < 32)
    {
        return 0;
    }
    if (reach_text_edit_insert_char(edit, ch) != REACH_TEXT_EDIT_EVENT_TEXT_CHANGED)
    {
        return 0;
    }
    model->account_status = REACH_SETTINGS_ACCOUNT_STATUS_NONE;
    model->account_caret_visible = 1;
    model->account_caret_phase = 0.0;
    return 1;
}

int32_t reach_settings_model_account_handle_edit_key(reach_settings_model *model,
                                                     reach_text_edit_key key,
                                                     reach_text_edit_modifiers modifiers)
{
    reach_text_edit *edit = reach_settings_account_focused_edit(model);
    if (edit == nullptr)
    {
        return 0;
    }
    if (reach_text_edit_handle_key(edit, key, modifiers) == REACH_TEXT_EDIT_EVENT_TEXT_CHANGED)
    {
        model->account_status = REACH_SETTINGS_ACCOUNT_STATUS_NONE;
    }
    model->account_caret_visible = 1;
    model->account_caret_phase = 0.0;
    return 1;
}

static int32_t reach_settings_account_edits_equal(const reach_text_edit *left,
                                                  const reach_text_edit *right)
{
    if (left->length != right->length)
    {
        return 0;
    }
    for (size_t index = 0; index < left->length; ++index)
    {
        if (left->text[index] != right->text[index])
        {
            return 0;
        }
    }
    return 1;
}

int32_t reach_settings_model_account_submit_ready(reach_settings_model *model)
{
    REACH_ASSERT(model != nullptr);
    if (model == nullptr)
    {
        return 0;
    }
    if (!reach_settings_account_edits_equal(
            &model->account_password_edits[REACH_SETTINGS_ACCOUNT_FIELD_NEW],
            &model->account_password_edits[REACH_SETTINGS_ACCOUNT_FIELD_CONFIRM]))
    {
        model->account_status = REACH_SETTINGS_ACCOUNT_STATUS_MISMATCH;
        return 0;
    }
    if (model->account_password_edits[REACH_SETTINGS_ACCOUNT_FIELD_NEW].length == 0)
    {
        model->account_status = REACH_SETTINGS_ACCOUNT_STATUS_EMPTY;
        return 0;
    }
    return 1;
}

void reach_settings_model_account_apply_status(reach_settings_model *model, int32_t status)
{
    REACH_ASSERT(model != nullptr);
    if (model == nullptr)
    {
        return;
    }
    model->account_status = status;
    if (status == REACH_SETTINGS_ACCOUNT_STATUS_SUCCESS)
    {
        for (size_t field = 0; field < REACH_SETTINGS_ACCOUNT_FIELD_COUNT; ++field)
        {
            reach_text_edit_clear(&model->account_password_edits[field]);
        }
        reach_settings_model_account_blur(model);
    }
}

int32_t reach_settings_model_tick_account_caret(reach_settings_model *model, double delta_seconds)
{
    if (model == nullptr || model->account_focused_field < 0)
    {
        return 0;
    }
    model->account_caret_phase += delta_seconds;
    while (model->account_caret_phase >= 1.0)
    {
        model->account_caret_phase -= 1.0;
    }
    int32_t visible = model->account_caret_phase < 0.55 ? 1 : 0;
    if (visible == model->account_caret_visible)
    {
        return 0;
    }
    model->account_caret_visible = visible;
    return 1;
}

const uint16_t *reach_settings_account_status_message(int32_t status)
{
    switch (status)
    {
    case REACH_SETTINGS_ACCOUNT_STATUS_EMPTY:
        return (const uint16_t *)L"Enter a new password";
    case REACH_SETTINGS_ACCOUNT_STATUS_MISMATCH:
        return (const uint16_t *)L"New passwords do not match";
    case REACH_SETTINGS_ACCOUNT_STATUS_WRONG_CURRENT:
        return (const uint16_t *)L"Current password is incorrect";
    case REACH_SETTINGS_ACCOUNT_STATUS_POLICY:
        return (const uint16_t *)L"New password does not meet the policy requirements";
    case REACH_SETTINGS_ACCOUNT_STATUS_ERROR:
        return (const uint16_t *)L"Could not change the password";
    case REACH_SETTINGS_ACCOUNT_STATUS_SUCCESS:
        return (const uint16_t *)L"Password changed";
    default:
        return (const uint16_t *)L"";
    }
}
