#include "context_menu_common.h"

#include <math.h>

static const reach_context_menu_preview_card *
reach_preview_find_card(const reach_context_menu_preview_layout *layout, reach_window_id window)
{
    for (size_t index = 0; index < layout->count; ++index)
        if (layout->cards[index].window == window)
            return &layout->cards[index];
    return nullptr;
}

void reach_context_menu_reset_preview_layout(reach_context_menu *menu)
{
    reach_motion_set(&menu->width, 0);
    reach_motion_set(&menu->reflow, 1);
    menu->layout_initialized = menu->layout_cleanup_pending = 0;
    menu->preview_origin_count = 0;
}

void reach_context_menu_stop_preview_reflow(reach_context_menu *menu)
{
    double now = reach_motion_now_seconds();
    auto freeze = [now](reach_motion &motion)
    {
        if (motion.generation)
            reach_motion_set(&motion, reach_motion_value_at(&motion, now));
    };
    auto freeze_rect = [&](reach_rect_motion &rect)
    {
        freeze(rect.x);
        freeze(rect.y);
        freeze(rect.width);
        freeze(rect.height);
    };
    freeze(menu->width);
    freeze_rect(menu->previews.body);
    freeze_rect(menu->previews.viewport);
    freeze_rect(menu->previews.previous);
    freeze_rect(menu->previews.next);
    freeze_rect(menu->previews.footer);
    freeze(menu->previews.notch);
    for (size_t index = 0; index < menu->previews.count; ++index)
        freeze_rect(menu->previews.cards[index].motion);
    reach_motion_set(&menu->reflow, 1);
    menu->preview_origin_count = 0;
    menu->layout_cleanup_pending = 0;
}

reach_result reach_context_menu_reflow_previews(reach_context_menu *menu,
                                                reach_context_menu_preview_layout *layout,
                                                double now, int32_t restart, int32_t cut)
{
    if (cut)
        menu->preview_origin_count = 0;
    else if (restart)
    {
        menu->preview_origin_count = layout->count;
        auto body = reach_rect_motion_value_at(&menu->previews.body, now);
        auto next_body = reach_rect_motion_value_at(&layout->body, now);
        for (size_t index = 0; index < layout->count; ++index)
        {
            const auto &card = layout->cards[index];
            const auto *previous = reach_preview_find_card(&menu->previews, card.window);
            menu->preview_origins[index].window = card.window;
            menu->preview_origins[index].screen_x =
                previous
                    ? menu->previews.envelope.x + body.x +
                          reach_motion_value_at(&previous->motion.x, now)
                    : layout->envelope.x + next_body.x +
                          reach_motion_value_at(&card.motion.x, now) + 12 * menu->state.dpi_scale;
        }
    }
    for (size_t index = 0; index < layout->count; ++index)
    {
        auto &card = layout->cards[index];
        for (size_t origin = 0; origin < menu->preview_origin_count; ++origin)
            if (menu->preview_origins[origin].window == card.window)
            {
                double start = menu->reflow.started_seconds;
                float base = layout->envelope.x + reach_motion_value_at(&layout->body.x, start) +
                             reach_motion_value_at(&card.motion.x, start);
                float from = menu->preview_origins[origin].screen_x - base;
                auto correction = reach_motion_affine(&menu->reflow, -from, from);
                if (reach_motion_combine(&card.motion.x, 1, &correction, 1, 0, &card.motion.x) !=
                    REACH_OK)
                    return REACH_INVALID_ARGUMENT;
                break;
            }
        reach_motion far = {};
        if (reach_motion_combine(&layout->viewport.x, 1, &layout->viewport.width, 1, 0, &far) !=
                REACH_OK ||
            reach_motion_combine(&far, 1, &card.motion.width, -1, 0, &far) != REACH_OK ||
            reach_motion_max(&far, &layout->viewport.x, &far) != REACH_OK ||
            reach_motion_max(&card.motion.x, &layout->viewport.x, &card.motion.x) != REACH_OK ||
            reach_motion_min(&card.motion.x, &far, &card.motion.x) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
    }
    return REACH_OK;
}

reach_rect_f32 reach_context_menu_preview_bounds_at(const reach_context_menu *menu,
                                                    const reach_rect_motion *bounds, double now)
{
    auto result = reach_rect_motion_value_at(bounds, now);
    auto body = reach_rect_motion_value_at(&menu->previews.body, now);
    result.x += body.x;
    result.y += body.y;
    return result;
}

reach_context_menu_preview_placement
reach_context_menu_preview_card_at(const reach_context_menu *menu, size_t index, double now)
{
    const auto &source = menu->previews.cards[index];
    reach_context_menu_preview_placement card = {};
    card.window = source.window;
    card.index = source.index;
    card.live = source.live;
    card.preview = reach_context_menu_preview_bounds_at(menu, &source.motion, now);
    auto body = reach_rect_motion_value_at(&menu->previews.body, now);
    card.header = {card.preview.x, body.y + menu->previews.header_y, card.preview.width,
                   source.header_height};
    float close = fminf(source.header_height, card.header.width);
    card.close = {card.header.x + card.header.width - close, card.header.y, close,
                  source.header_height};
    return card;
}
