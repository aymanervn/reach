#include "settings_pages_internal.h"

const uint16_t *reach_settings_power_sleep_page_title(void)
{
    return (const uint16_t *)L"Power and Sleep";
}

const uint16_t *reach_settings_power_sleep_page_placeholder(void)
{
    return (const uint16_t *)L"Power and Sleep settings page";
}

static const int32_t
    reach_settings_power_presets[REACH_SETTINGS_POWER_TIMER_COUNT]
                                [REACH_SETTINGS_POWER_PRESET_COUNT] = {
                                    {0, 5, 15, 30, 60},
                                    {0, 1, 5, 10, 15},
                                    {0, 30, 60, 120, 180},
                                    {0, 60, 120, 240, 360},
};

const reach_settings_power_row_style *reach_settings_power_row_styles(void)
{
    static const reach_settings_power_row_style styles[REACH_SETTINGS_POWER_TIMER_COUNT] = {
        {REACH_VECTOR_ICON_SLEEP,
         (const uint16_t *)L"Sleep",
         (const uint16_t *)L"Put the device to sleep when idle for",
         {0.70f, 0.55f, 0.95f, 1.0f}},
        {REACH_VECTOR_ICON_LOCK,
         (const uint16_t *)L"Auto lock",
         (const uint16_t *)L"Lock Windows when idle for",
         {0.31f, 0.78f, 0.86f, 1.0f}},
        {REACH_VECTOR_ICON_SHUTDOWN,
         (const uint16_t *)L"Shutdown",
         (const uint16_t *)L"Shut down the device when idle for",
         {0.96f, 0.45f, 0.35f, 1.0f}},
        {REACH_VECTOR_ICON_RESTART,
         (const uint16_t *)L"Restart",
         (const uint16_t *)L"Restart the device when idle for",
         {0.20f, 0.72f, 0.96f, 1.0f}},
    };
    return styles;
}

int32_t reach_settings_power_option_minutes(size_t timer, size_t option)
{
    if (timer >= REACH_SETTINGS_POWER_TIMER_COUNT || option >= REACH_SETTINGS_POWER_PRESET_COUNT)
    {
        return 0;
    }
    return reach_settings_power_presets[timer][option];
}

const uint16_t *reach_settings_power_option_label(size_t timer, size_t option)
{
    static const struct
    {
        int32_t minutes;
        const uint16_t *label;
    } labels[] = {
        {0, (const uint16_t *)L"Never"},    {1, (const uint16_t *)L"1 min"},
        {5, (const uint16_t *)L"5 min"},    {10, (const uint16_t *)L"10 min"},
        {15, (const uint16_t *)L"15 min"},  {30, (const uint16_t *)L"30 min"},
        {60, (const uint16_t *)L"1 hr"},    {120, (const uint16_t *)L"2 hrs"},
        {180, (const uint16_t *)L"3 hrs"},  {240, (const uint16_t *)L"4 hrs"},
        {360, (const uint16_t *)L"6 hrs"},  {720, (const uint16_t *)L"12 hrs"},
    };

    int32_t minutes = reach_settings_power_option_minutes(timer, option);
    for (size_t index = 0; index < sizeof(labels) / sizeof(labels[0]); ++index)
    {
        if (labels[index].minutes == minutes)
        {
            return labels[index].label;
        }
    }
    return (const uint16_t *)L"Never";
}

static int32_t reach_settings_power_parse_field(const reach_text_edit *edit)
{
    int32_t value = 0;
    for (size_t index = 0; index < edit->length; ++index)
    {
        uint16_t ch = edit->text[index];
        if (ch < (uint16_t)'0' || ch > (uint16_t)'9')
        {
            continue;
        }
        value = value * 10 + (int32_t)(ch - (uint16_t)'0');
        if (value > 99)
        {
            value = 99;
        }
    }
    return value;
}

static int32_t reach_settings_power_parse_custom(const reach_settings_model *model, size_t timer)
{
    return reach_settings_power_parse_field(
               &model->power_custom_edits[timer][REACH_SETTINGS_POWER_FIELD_HOURS]) *
               60 +
           reach_settings_power_parse_field(
               &model->power_custom_edits[timer][REACH_SETTINGS_POWER_FIELD_MINUTES]);
}

static void reach_settings_power_seed_field(reach_text_edit *edit, int32_t value)
{
    if (value < 0)
    {
        value = 0;
    }
    if (value > 99)
    {
        value = 99;
    }
    uint16_t digits[REACH_SETTINGS_POWER_CUSTOM_DIGITS + 1] = {};
    size_t count = 0;
    if (value >= 10)
    {
        digits[count++] = (uint16_t)('0' + value / 10);
    }
    digits[count++] = (uint16_t)('0' + value % 10);
    digits[count] = 0;
    reach_text_edit_set_text(edit, digits);
}

static void reach_settings_power_seed_custom(reach_settings_model *model, size_t timer,
                                             int32_t minutes)
{
    reach_settings_power_seed_field(
        &model->power_custom_edits[timer][REACH_SETTINGS_POWER_FIELD_HOURS], minutes / 60);
    reach_settings_power_seed_field(
        &model->power_custom_edits[timer][REACH_SETTINGS_POWER_FIELD_MINUTES], minutes % 60);
}

static int32_t reach_settings_power_custom_empty(const reach_settings_model *model, size_t timer)
{
    return model->power_custom_edits[timer][REACH_SETTINGS_POWER_FIELD_HOURS].length == 0 &&
           model->power_custom_edits[timer][REACH_SETTINGS_POWER_FIELD_MINUTES].length == 0;
}

void reach_settings_model_set_power_minutes(reach_settings_model *model, size_t timer,
                                            int32_t minutes)
{
    REACH_ASSERT(model != nullptr);
    REACH_ASSERT(timer < REACH_SETTINGS_POWER_TIMER_COUNT);
    if (model == nullptr || timer >= REACH_SETTINGS_POWER_TIMER_COUNT)
    {
        return;
    }
    if (minutes < 0)
    {
        minutes = 0;
    }
    model->power_minutes[timer] = minutes;
    size_t selected = REACH_SETTINGS_POWER_CUSTOM_OPTION;
    for (size_t option = 0; option < REACH_SETTINGS_POWER_PRESET_COUNT; ++option)
    {
        if (reach_settings_power_presets[timer][option] == minutes)
        {
            selected = option;
            break;
        }
    }
    if (selected == REACH_SETTINGS_POWER_CUSTOM_OPTION)
    {
        reach_settings_power_seed_custom(model, timer, minutes);
    }
    model->power_selected[timer] = selected;
    model->power_previous[timer] = selected;
    reach_animation_manager_set(&model->power_animations, timer, 1.0f);
}

int32_t reach_settings_model_power_minutes(const reach_settings_model *model, size_t timer)
{
    REACH_ASSERT(model != nullptr);
    REACH_ASSERT(timer < REACH_SETTINGS_POWER_TIMER_COUNT);
    if (model == nullptr || timer >= REACH_SETTINGS_POWER_TIMER_COUNT)
    {
        return 0;
    }
    return model->power_minutes[timer];
}

void reach_settings_model_select_power_option(reach_settings_model *model, size_t timer,
                                              size_t option)
{
    REACH_ASSERT(model != nullptr);
    REACH_ASSERT(timer < REACH_SETTINGS_POWER_TIMER_COUNT);
    REACH_ASSERT(option < REACH_SETTINGS_POWER_OPTION_COUNT);
    if (model == nullptr || timer >= REACH_SETTINGS_POWER_TIMER_COUNT ||
        option >= REACH_SETTINGS_POWER_OPTION_COUNT || option == model->power_selected[timer])
    {
        return;
    }
    model->power_previous[timer] = model->power_selected[timer];
    model->power_selected[timer] = option;
    if (option == REACH_SETTINGS_POWER_CUSTOM_OPTION)
    {
        if (reach_settings_power_custom_empty(model, timer))
        {
            reach_settings_power_seed_custom(model, timer, model->power_minutes[timer]);
        }
        model->power_minutes[timer] = reach_settings_power_parse_custom(model, timer);
    }
    else
    {
        model->power_minutes[timer] = reach_settings_power_presets[timer][option];
    }
    reach_animation_manager_start(&model->power_animations, timer, 0.0f, 1.0f, 0.22,
                                  REACH_EASING_EASE_OUT);
}

int32_t reach_settings_model_tick_power_animations(reach_settings_model *model,
                                                   double delta_seconds)
{
    if (model == nullptr)
    {
        return 0;
    }
    int32_t active = 0;
    if (reach_animation_manager_any_active(&model->power_animations))
    {
        reach_animation_manager_tick(&model->power_animations, delta_seconds);
        active = 1;
    }
    if (reach_animation_manager_any_active(&model->power_wait_animations))
    {
        reach_animation_manager_tick(&model->power_wait_animations, delta_seconds);
        active = 1;
    }
    return active;
}

int32_t reach_settings_model_power_animations_active(const reach_settings_model *model)
{
    return model != nullptr &&
           (reach_animation_manager_any_active(&model->power_animations) ||
            reach_animation_manager_any_active(&model->power_wait_animations));
}

void reach_settings_model_power_focus_custom(reach_settings_model *model, size_t timer,
                                             size_t field)
{
    REACH_ASSERT(model != nullptr);
    REACH_ASSERT(timer < REACH_SETTINGS_POWER_TIMER_COUNT);
    REACH_ASSERT(field < REACH_SETTINGS_POWER_FIELD_COUNT);
    if (model == nullptr || timer >= REACH_SETTINGS_POWER_TIMER_COUNT ||
        field >= REACH_SETTINGS_POWER_FIELD_COUNT)
    {
        return;
    }
    reach_settings_model_select_power_option(model, timer, REACH_SETTINGS_POWER_CUSTOM_OPTION);
    if (reach_settings_power_custom_empty(model, timer))
    {
        reach_settings_power_seed_custom(model, timer, model->power_minutes[timer]);
    }
    reach_text_edit_select_all(&model->power_custom_edits[timer][field]);
    model->power_focused_timer = (int32_t)timer;
    model->power_focused_field = (int32_t)field;
    model->power_caret_visible = 1;
    model->power_caret_phase = 0.0;
}

void reach_settings_model_power_blur(reach_settings_model *model)
{
    if (model == nullptr || model->power_focused_timer < 0)
    {
        return;
    }
    size_t timer = (size_t)model->power_focused_timer;
    if (model->power_selected[timer] == REACH_SETTINGS_POWER_CUSTOM_OPTION)
    {
        reach_settings_power_seed_custom(model, timer, model->power_minutes[timer]);
    }
    for (size_t field = 0; field < REACH_SETTINGS_POWER_FIELD_COUNT; ++field)
    {
        model->power_custom_edits[timer][field].selection_anchor = -1;
    }
    model->power_focused_timer = -1;
}

static reach_text_edit *reach_settings_power_focused_edit(reach_settings_model *model)
{
    if (model == nullptr || model->power_focused_timer < 0 ||
        model->power_focused_timer >= REACH_SETTINGS_POWER_TIMER_COUNT ||
        model->power_focused_field < 0 ||
        model->power_focused_field >= REACH_SETTINGS_POWER_FIELD_COUNT)
    {
        return nullptr;
    }
    return &model->power_custom_edits[model->power_focused_timer][model->power_focused_field];
}

static void reach_settings_power_apply_edit_minutes(reach_settings_model *model)
{
    size_t timer = (size_t)model->power_focused_timer;
    model->power_minutes[timer] = reach_settings_power_parse_custom(model, timer);
    if (model->power_selected[timer] != REACH_SETTINGS_POWER_CUSTOM_OPTION)
    {
        model->power_previous[timer] = model->power_selected[timer];
        model->power_selected[timer] = REACH_SETTINGS_POWER_CUSTOM_OPTION;
        reach_animation_manager_start(&model->power_animations, timer, 0.0f, 1.0f, 0.22,
                                      REACH_EASING_EASE_OUT);
    }
}

int32_t reach_settings_model_power_insert_char(reach_settings_model *model, uint16_t ch)
{
    reach_text_edit *edit = reach_settings_power_focused_edit(model);
    if (edit == nullptr || ch < (uint16_t)'0' || ch > (uint16_t)'9')
    {
        return 0;
    }
    if (reach_text_edit_insert_char(edit, ch) != REACH_TEXT_EDIT_EVENT_TEXT_CHANGED)
    {
        return 0;
    }
    reach_settings_power_apply_edit_minutes(model);
    model->power_caret_visible = 1;
    model->power_caret_phase = 0.0;
    return 1;
}

int32_t reach_settings_model_power_handle_edit_key(reach_settings_model *model,
                                                   reach_text_edit_key key,
                                                   reach_text_edit_modifiers modifiers)
{
    reach_text_edit *edit = reach_settings_power_focused_edit(model);
    if (edit == nullptr)
    {
        return 0;
    }
    if (reach_text_edit_handle_key(edit, key, modifiers) == REACH_TEXT_EDIT_EVENT_TEXT_CHANGED)
    {
        reach_settings_power_apply_edit_minutes(model);
    }
    model->power_caret_visible = 1;
    model->power_caret_phase = 0.0;
    return 1;
}

int32_t reach_settings_power_timer_supports_wait(size_t timer)
{
    return timer < REACH_SETTINGS_POWER_TIMER_COUNT && timer != REACH_SETTINGS_POWER_TIMER_LOCK;
}

void reach_settings_model_set_power_wait_apps(reach_settings_model *model, size_t timer,
                                              int32_t enabled)
{
    REACH_ASSERT(model != nullptr);
    if (model == nullptr || !reach_settings_power_timer_supports_wait(timer))
    {
        return;
    }
    model->power_wait_apps[timer] = enabled ? 1 : 0;
    reach_animation_manager_set(&model->power_wait_animations, timer, enabled ? 1.0f : 0.0f);
}

int32_t reach_settings_model_power_wait_apps(const reach_settings_model *model, size_t timer)
{
    REACH_ASSERT(model != nullptr);
    if (model == nullptr || timer >= REACH_SETTINGS_POWER_TIMER_COUNT)
    {
        return 0;
    }
    return model->power_wait_apps[timer];
}

int32_t reach_settings_model_toggle_power_wait_apps(reach_settings_model *model, size_t timer)
{
    REACH_ASSERT(model != nullptr);
    if (model == nullptr || !reach_settings_power_timer_supports_wait(timer))
    {
        return 0;
    }
    model->power_wait_apps[timer] = model->power_wait_apps[timer] ? 0 : 1;
    float current = reach_animation_manager_value(&model->power_wait_animations, timer);
    reach_animation_manager_start(&model->power_wait_animations, timer, current,
                                  model->power_wait_apps[timer] ? 1.0f : 0.0f, 0.18,
                                  REACH_EASING_EASE_OUT);
    return 1;
}

int32_t reach_settings_model_power_dirty(const reach_settings_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    for (size_t timer = 0; timer < REACH_SETTINGS_POWER_TIMER_COUNT; ++timer)
    {
        if (model->power_minutes[timer] != model->power_applied_minutes[timer] ||
            model->power_wait_apps[timer] != model->power_applied_wait_apps[timer])
        {
            return 1;
        }
    }
    return 0;
}

void reach_settings_model_power_mark_applied(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return;
    }
    for (size_t timer = 0; timer < REACH_SETTINGS_POWER_TIMER_COUNT; ++timer)
    {
        model->power_applied_minutes[timer] = model->power_minutes[timer];
        model->power_applied_wait_apps[timer] = model->power_wait_apps[timer];
    }
}

int32_t reach_settings_model_tick_power_caret(reach_settings_model *model, double delta_seconds)
{
    if (model == nullptr || model->power_focused_timer < 0)
    {
        return 0;
    }
    model->power_caret_phase += delta_seconds;
    while (model->power_caret_phase >= 1.0)
    {
        model->power_caret_phase -= 1.0;
    }
    int32_t visible = model->power_caret_phase < 0.55 ? 1 : 0;
    if (visible == model->power_caret_visible)
    {
        return 0;
    }
    model->power_caret_visible = visible;
    return 1;
}
