#include "../src/features/widgets/widgets_layout.h"

#include <cstdio>

static int failed = 0;
static void expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failed;
    }
}

static void validate(const reach_rect_f32 *rects, size_t count, reach_rect_f32 bounds)
{
    for (size_t index = 0; index < count; ++index)
    {
        const auto &rect = rects[index];
        expect(rect.width > 0 && rect.height > 0, "every enabled item fits");
        expect(rect.x >= bounds.x && rect.y >= bounds.y &&
                   rect.x + rect.width <= bounds.x + bounds.width &&
                   rect.y + rect.height <= bounds.y + bounds.height,
               "packed rectangles stay on the desktop");
        for (size_t other = 0; other < index; ++other)
            expect(!reach_widgets_overlap(rect, rects[other], 14),
                   "packed rectangles do not overlap");
    }
}

int main()
{
    reach_rect_f32 desktop = {0, 0, 1280, 900};
    reach_rect_f32 area = {24, 84, 1232, 792};
    reach_widget_layout_item varied[3] = {{{0, 0, 440, 350}, {300, 228}, true, false},
                                          {{0, 0, 300, 350}, {280, 278}, true, false},
                                          {{0, 0, 420, 350}, {280, 180}, true, false}};
    reach_rect_f32 out[7] = {};
    expect(reach_widgets_pack(desktop, area, varied, 3, 14, out), "varied-width columns fit");
    validate(out, 3, desktop);
    expect(out[2].y == area.y && out[2].x + out[2].width + 14 <= out[0].x,
           "overflow starts a new column to the left of the widest card");

    reach_widget_layout_item items[7] = {{{100, 100, 244, 144}, {220, 136}, true, true},
                                         {{100, 100, 440, 300}, {300, 228}, true, true},
                                         {{0, 0, 300, 196}, {280, 180}, true, false},
                                         {{0, 0, 300, 300}, {280, 278}, true, false},
                                         {{0, 0, 280, 160}, {240, 144}, true, false},
                                         {{0, 0, 280, 160}, {240, 144}, true, false},
                                         {{0, 0, 280, 180}, {240, 160}, true, false}};
    desktop = {0, 0, 1920, 1080};
    area = {24, 84, 1872, 972};
    expect(reach_widgets_pack(desktop, area, items, 7, 14, out),
           "saved overlapping positions are repaired");
    validate(out, 7, desktop);
    expect(out[0].x == 100 && out[0].y == 100, "valid custom position is retained");

    for (auto &item : items)
        item.custom = false;
    expect(reach_widgets_pack(desktop, area, items, 7, 14, out),
           "reset positions pack from the right");
    validate(out, 7, desktop);
    expect(out[0].x + out[0].width == area.x + area.width && out[0].y == area.y,
           "first reset item anchors at the top right");

    expect(reach_widgets_overlap({0, 0, 300, 200}, {250, 50, 300, 200}, 14),
           "drag and resize candidates detect collisions");
    expect(!reach_widgets_overlap({0, 0, 300, 200}, {314, 0, 300, 200}, 14),
           "adjacent cards retain the configured gap");
    return failed ? 1 : 0;
}
