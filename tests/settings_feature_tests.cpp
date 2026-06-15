#include "reach/features/settings.h"

#include <assert.h>

static int utf16_equal_ascii(const uint16_t *text, const char *ascii)
{
    size_t index = 0;
    while (ascii[index] != 0)
    {
        if (text[index] != (uint16_t)(unsigned char)ascii[index])
        {
            return 0;
        }
        ++index;
    }
    return text[index] == 0;
}

int main()
{
    reach_settings_model model = {};
    reach_settings_model_init(&model);
    assert(model.selected_page == REACH_SETTINGS_PAGE_WIFI);
    assert(utf16_equal_ascii(reach_settings_page_title(REACH_SETTINGS_PAGE_STARTUP_APPS),
                             "Startup Apps"));

    size_t nav_count = 0;
    const reach_settings_nav_item *items = reach_settings_nav_items(&nav_count);
    assert(items != nullptr);
    assert(nav_count == REACH_SETTINGS_NAV_ITEM_COUNT);
    assert(items[3].page == REACH_SETTINGS_PAGE_STARTUP_APPS);
    assert(items[3].icon_id == REACH_VECTOR_ICON_SETTINGS);

    reach_rect_f32 bounds = {0.0f, 0.0f, 800.0f, 520.0f};
    reach_settings_layout layout =
        reach_settings_layout_for_bounds(bounds, reach_theme_default(), 1.0f);
    assert(layout.nav_item_count == REACH_SETTINGS_NAV_ITEM_COUNT);
    assert(layout.nav.width >= 176.0f);
    assert(layout.content.width > layout.nav.width);

    reach_settings_hit_result nav_hit = reach_settings_hit_test(
        &layout, layout.nav_items[4].bounds.x + 4.0f, layout.nav_items[4].bounds.y + 4.0f);
    assert(nav_hit.type == REACH_SETTINGS_HIT_NAV_ITEM);
    assert(nav_hit.page == REACH_SETTINGS_PAGE_POWER_SLEEP);

    reach_settings_model_select_page(&model, nav_hit.page);
    assert(model.selected_page == REACH_SETTINGS_PAGE_POWER_SLEEP);

    reach_settings_hit_result close_hit = reach_settings_hit_test(
        &layout, layout.close_button.x + 1.0f, layout.close_button.y + 1.0f);
    assert(close_hit.type == REACH_SETTINGS_HIT_CLOSE);

    reach_render_command_buffer commands = {};
    reach_settings_render_input input = {};
    input.theme = reach_theme_default();
    input.model = &model;
    input.layout = &layout;
    input.dpi_scale = 1.0f;
    input.text_alignment_leading = REACH_TEXT_ALIGNMENT_LEADING;
    input.text_alignment_center = REACH_TEXT_ALIGNMENT_CENTER;
    assert(reach_settings_build_render_commands(&input, &commands) == REACH_OK);
    assert(commands.count > 0);

    return 0;
}
