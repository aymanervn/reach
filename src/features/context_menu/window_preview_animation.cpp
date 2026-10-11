#include "context_menu_common.h"

#include <math.h>

static constexpr double reach_preview_reflow_seconds = 0.18;
static constexpr float reach_preview_entry_offset_dp = 12.0f;

static const reach_context_menu_preview_card *
reach_preview_find_card(const reach_context_menu_preview_layout *layout, reach_window_id window)
{
    for (size_t index = 0; index < layout->count; ++index)
    {
        if (layout->cards[index].window == window)
        {
            return &layout->cards[index];
        }
    }
    return nullptr;
}

void reach_context_menu_stop_preview_reflow(reach_context_menu *menu)
{
    reach_animation_manager_set(&menu->animations, REACH_CONTEXT_MENU_ANIM_REFLOW, 1.0f);
    menu->preview_origin_count = 0;
}

void reach_context_menu_reflow_previews(reach_context_menu *menu,
                                        reach_context_menu_preview_layout *layout,
                                        reach_rect_f32 previous_bounds, int32_t constraints_changed)
{
    int32_t sources_changed = layout->count != menu->previews.count;
    size_t survivors = 0;
    for (size_t index = 0; index < layout->count; ++index)
    {
        sources_changed |= layout->cards[index].window != menu->previews.cards[index].window;
        survivors +=
            reach_preview_find_card(&menu->previews, layout->cards[index].window) != nullptr;
    }
    if (constraints_changed || !menu->state.open || (sources_changed && survivors == 0))
    {
        reach_context_menu_stop_preview_reflow(menu);
    }
    else if (sources_changed)
    {
        menu->preview_origin_count = layout->count;
        for (size_t index = 0; index < layout->count; ++index)
        {
            const reach_context_menu_preview_card *card = &layout->cards[index];
            const reach_context_menu_preview_card *previous =
                reach_preview_find_card(&menu->previews, card->window);
            menu->preview_origins[index].window = card->window;
            menu->preview_origins[index].screen_x =
                previous != nullptr ? previous_bounds.x + previous->preview.x
                                    : menu->state.bounds.x + card->preview.x +
                                          reach_preview_entry_offset_dp * menu->state.dpi_scale;
        }
        reach_animation_manager_start(&menu->animations, REACH_CONTEXT_MENU_ANIM_REFLOW, 0.0f, 1.0f,
                                      reach_preview_reflow_seconds, REACH_EASING_EASE_IN_OUT);
    }
    float progress =
        reach_animation_manager_value(&menu->animations, REACH_CONTEXT_MENU_ANIM_REFLOW);
    if (progress >= 1.0f)
    {
        menu->preview_origin_count = 0;
        return;
    }
    for (size_t index = 0; index < layout->count; ++index)
    {
        reach_context_menu_preview_card *card = &layout->cards[index];
        float x = card->preview.x;
        for (size_t origin = 0; origin < menu->preview_origin_count; ++origin)
        {
            if (menu->preview_origins[origin].window == card->window)
            {
                float from = menu->preview_origins[origin].screen_x - menu->state.bounds.x;
                x = from + (x - from) * progress;
                break;
            }
        }
        float left = menu->state.item_viewport.x;
        float right = left + menu->state.item_viewport.width - card->preview.width;
        x = fmaxf(left, fminf(x, right));
        float offset = x - card->preview.x;
        card->preview.x = x;
        card->header.x += offset;
        card->close.x += offset;
        menu->state.item_slots[card->index].x = x;
    }
}
