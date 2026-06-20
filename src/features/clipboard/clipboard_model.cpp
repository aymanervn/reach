#include "reach/features/clipboard.h"

static size_t reach_clipboard_find_duplicate(const reach_clipboard_model *model,
                                             const reach_clipboard_item *item)
{
    if (model == nullptr || item == nullptr || item->content_hash == 0)
    {
        return REACH_CLIPBOARD_MAX_ITEMS;
    }
    for (size_t index = 0; index < model->count; ++index)
    {
        if (model->items[index].kind == item->kind &&
            model->items[index].content_hash == item->content_hash)
        {
            return index;
        }
    }
    return REACH_CLIPBOARD_MAX_ITEMS;
}

void reach_clipboard_model_clear_press(reach_clipboard_model *model)
{
    if (model == nullptr)
    {
        return;
    }

    model->pressed_index = REACH_CLIPBOARD_MAX_ITEMS;
    model->pressed_hit_type = REACH_CLIPBOARD_HIT_NONE;
    model->pressed_item_id = 0;
}

void reach_clipboard_model_init(reach_clipboard_model *model)
{
    if (model == nullptr)
    {
        return;
    }
    *model = {};
    model->hovered_index = REACH_CLIPBOARD_MAX_ITEMS;
    reach_clipboard_model_clear_press(model);
    reach_scrollbar_model_init(&model->scrollbar, REACH_SCROLLBAR_DRAG_FREE, 0.0f);
}

int32_t reach_clipboard_model_promote(reach_clipboard_model *model, size_t index)
{
    if (model == nullptr || index >= model->count)
    {
        return 0;
    }
    if (index == 0)
    {
        return 1;
    }
    reach_clipboard_item item = model->items[index];
    for (size_t cursor = index; cursor > 0; --cursor)
    {
        model->items[cursor] = model->items[cursor - 1];
    }
    model->items[0] = item;
    return 1;
}

reach_clipboard_insert_result reach_clipboard_model_insert(reach_clipboard_model *model,
                                                           reach_clipboard_item item)
{
    reach_clipboard_insert_result result = {};
    if (model == nullptr || item.id == 0 ||
        (item.kind != REACH_CLIPBOARD_ITEM_TEXT && item.kind != REACH_CLIPBOARD_ITEM_IMAGE))
    {
        result.rejected_id = item.id;
        return result;
    }

    size_t duplicate = reach_clipboard_find_duplicate(model, &item);
    if (duplicate < model->count)
    {
        result.rejected_id = item.id;
        result.promoted_existing = reach_clipboard_model_promote(model, duplicate);
        return result;
    }

    if (model->count == REACH_CLIPBOARD_MAX_ITEMS)
    {
        result.evicted_id = model->items[model->count - 1].id;
    }
    else
    {
        ++model->count;
    }
    for (size_t index = model->count - 1; index > 0; --index)
    {
        model->items[index] = model->items[index - 1];
    }
    model->items[0] = item;
    model->hovered_index = REACH_CLIPBOARD_MAX_ITEMS;
    reach_clipboard_model_clear_press(model);
    result.inserted = 1;
    return result;
}

void reach_clipboard_model_remove(reach_clipboard_model *model, size_t index)
{
    if (model == nullptr || index >= model->count)
    {
        return;
    }
    for (size_t cursor = index; cursor + 1 < model->count; ++cursor)
    {
        model->items[cursor] = model->items[cursor + 1];
    }
    --model->count;
    model->items[model->count] = {};
    model->hovered_index = REACH_CLIPBOARD_MAX_ITEMS;
    reach_clipboard_model_clear_press(model);
}