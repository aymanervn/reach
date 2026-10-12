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
    double now = reach_motion_now_seconds();
    for (size_t index = 0; index < menu->previews.count; ++index)
        reach_motion_set(&menu->previews.cards[index].x,
                         reach_motion_value_at(&menu->previews.cards[index].x, now));
    reach_motion_set(&menu->reflow, 1.0f);
    menu->preview_origin_count = 0;
}

void reach_context_menu_reflow_previews(reach_context_menu *menu,
                                        reach_context_menu_preview_layout *layout,
                                        reach_rect_f32 previous_bounds, int32_t constraints_changed)
{
    double now = reach_motion_now_seconds();
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
                previous != nullptr ? previous_bounds.x + previous->preview.x +
                                          reach_motion_value_at(&previous->x, now)
                                    : menu->state.bounds.x + card->preview.x +
                                          reach_preview_entry_offset_dp * menu->state.dpi_scale;
        }
        reach_motion_start_at(&menu->reflow, 0, 1, reach_preview_reflow_seconds,
                              REACH_EASING_EASE_IN_OUT, now);
    }
    if (!reach_motion_active_at(&menu->reflow, now))
        menu->preview_origin_count = 0;
    for (size_t index = 0; index < layout->count; ++index)
    {
        reach_context_menu_preview_card *card = &layout->cards[index];
        reach_motion_set(&card->x, 0);
        for (size_t origin = 0; origin < menu->preview_origin_count; ++origin)
        {
            if (menu->preview_origins[origin].window == card->window)
            {
                float from =
                    menu->preview_origins[origin].screen_x - menu->state.bounds.x - card->preview.x;
                card->x = reach_motion_affine(&menu->reflow, -from, from);
                break;
            }
        }
        float left = menu->state.item_viewport.x - card->preview.x;
        float right = left + menu->state.item_viewport.width - card->preview.width;
        card->x = reach_motion_clamp(&card->x, left, fmaxf(left, right));
    }
}

reach_context_menu_preview_card reach_context_menu_preview_card_at(const reach_context_menu *menu,
                                                                   size_t index, double now_seconds)
{
    reach_context_menu_preview_card card = menu->previews.cards[index];
    float x = reach_motion_value_at(&card.x, now_seconds);
    card.preview.x += x;
    card.header.x += x;
    card.close.x += x;
    return card;
}
