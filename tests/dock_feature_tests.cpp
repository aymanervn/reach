#include "reach/features/dock.h"

static int expect(int condition)
{
    return condition ? 0 : 1;
}

int main(void)
{
    int failed = 0;

    reach_dock_feature_model model = {};
    model.item_count = 3;

    reach_dock_layout layout = {};
    layout.app_slot_count = 3;
    layout.app_slots[0] = { 10.0f, 0.0f, 40.0f, 40.0f };
    layout.app_slots[1] = { 62.0f, 0.0f, 40.0f, 40.0f };
    layout.app_slots[2] = { 114.0f, 0.0f, 40.0f, 40.0f };

    failed += expect(reach_dock_reorder_target(&model, &layout, 0, 51.5f) == 0);
    failed += expect(reach_dock_reorder_target(&model, &layout, 0, 52.0f) == 1);
    failed += expect(reach_dock_reorder_target(&model, &layout, 1, 103.5f) == 1);
    failed += expect(reach_dock_reorder_target(&model, &layout, 1, 104.0f) == 2);

    failed += expect(reach_dock_reorder_target(&model, &layout, 2, 72.5f) == 2);
    failed += expect(reach_dock_reorder_target(&model, &layout, 2, 72.0f) == 1);
    failed += expect(reach_dock_reorder_target(&model, &layout, 1, 20.0f) == 0);

    failed += expect(reach_dock_reorder_target(&model, &layout, 3, 10.0f) == REACH_MAX_PINNED_APPS);

    return failed == 0 ? 0 : 1;
}
