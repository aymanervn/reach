#include "reach/features/dock.h"

void reach_dock_feature_model_init(reach_dock_feature_model *model) {
  if (model == nullptr) {
    return;
  }

  *model = {};
}

int32_t reach_dock_key_equal(const reach_dock_order_key *a,
                             const reach_dock_order_key *b) {
  if (a == nullptr || b == nullptr) {
    return 0;
  }

  if (a->pinned && b->pinned) {
    return a->pin_id != 0 && a->pin_id == b->pin_id;
  }

  if (!a->pinned && !b->pinned) {
    return a->window != 0 && a->window == b->window;
  }

  return a->window != 0 && a->window == b->window;
}

uint32_t
reach_dock_feature_model_item_pin_id(const reach_dock_feature_model *model,
                                     size_t index) {
  if (model == nullptr || index >= model->item_count ||
      !model->items[index].pinned) {
    return 0;
  }
  return model->items[index].pin_id;
}

int32_t
reach_dock_feature_model_item_matches_key(const reach_dock_feature_model *model,
                                          size_t index,
                                          reach_dock_order_key key) {
  if (model == nullptr || index >= model->item_count) {
    return 0;
  }

  reach_dock_order_key item_key = {};
  item_key.pinned = model->items[index].pinned;
  item_key.pin_id = model->items[index].pin_id;
  item_key.window = model->items[index].window;
  return reach_dock_key_equal(&item_key, &key);
}

size_t
reach_dock_feature_model_find_item_key(const reach_dock_feature_model *model,
                                       reach_dock_order_key key) {
  if (model == nullptr) {
    return REACH_MAX_PINNED_APPS;
  }
  for (size_t index = 0; index < model->item_count; ++index) {
    if (reach_dock_feature_model_item_matches_key(model, index, key)) {
      return index;
    }
  }
  return REACH_MAX_PINNED_APPS;
}

size_t
reach_dock_feature_model_find_order_key(const reach_dock_feature_model *model,
                                        reach_dock_order_key key) {
  if (model == nullptr) {
    return REACH_MAX_PINNED_APPS;
  }
  for (size_t index = 0; index < model->order_count; ++index) {
    if (reach_dock_key_equal(&model->order[index], &key)) {
      return index;
    }
  }
  return REACH_MAX_PINNED_APPS;
}

void reach_dock_feature_model_move_order(reach_dock_feature_model *model,
                                         size_t source, size_t target) {
  if (model == nullptr || source >= model->order_count ||
      target >= model->order_count || source == target) {
    return;
  }

  reach_dock_order_key key = model->order[source];
  if (source < target) {
    for (size_t index = source; index < target; ++index) {
      model->order[index] = model->order[index + 1];
    }
  } else {
    for (size_t index = source; index > target; --index) {
      model->order[index] = model->order[index - 1];
    }
  }
  model->order[target] = key;
}

size_t reach_dock_feature_model_pinned_order_index(
    const reach_dock_feature_model *model, uint32_t pin_id) {
  if (model == nullptr || pin_id == 0) {
    return REACH_MAX_PINNED_APPS;
  }

  size_t pinned_index = 0;
  for (size_t index = 0; index < model->order_count; ++index) {
    if (model->order[index].pinned) {
      if (model->order[index].pin_id == pin_id) {
        return pinned_index;
      }
      ++pinned_index;
    }
  }
  return REACH_MAX_PINNED_APPS;
}
