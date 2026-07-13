#ifndef REACH_FEATURES_CONTEXT_MENU_COMMON_H
#define REACH_FEATURES_CONTEXT_MENU_COMMON_H

#include "reach/features/context_menu.h"

typedef struct reach_context_menu_hit_result
{
    int32_t hit;
    size_t index;
} reach_context_menu_hit_result;

typedef struct reach_context_menu_action
{
    uint32_t command;
} reach_context_menu_action;

reach_context_menu_hit_result reach_context_menu_hit_test_items(const reach_rect_f32 *item_slots,
                                                                size_t item_count, int32_t x,
                                                                int32_t y);
reach_context_menu_action reach_context_menu_action_for_hit(const uint32_t *item_commands,
                                                            size_t item_count,
                                                            reach_context_menu_hit_result hit);
int32_t reach_context_menu_set_hovered(reach_context_menu *menu, size_t index);

#endif
