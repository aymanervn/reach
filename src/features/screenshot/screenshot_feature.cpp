#include "screenshot_common.h"

#include <math.h>
#include <new>

static const float REACH_SCREENSHOT_MIN_SELECTION_PX = 2.0f;

static float reach_screenshot_clamp(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

static int32_t reach_screenshot_rect_contains(reach_rect_f32 rect, reach_point_f32 point)
{
    return point.x >= rect.x && point.x < rect.x + rect.width && point.y >= rect.y &&
           point.y < rect.y + rect.height;
}

const reach_screen_image *reach_screenshot_active(const reach_screenshot *screenshot)
{
    return screenshot != nullptr && screenshot->active_image < screenshot->image_count
               ? &screenshot->images[screenshot->active_image]
               : nullptr;
}

int32_t reach_screenshot_selection_rect(const reach_screenshot *screenshot, reach_rect_f32 *out)
{
    const reach_screen_image *active = reach_screenshot_active(screenshot);
    if (active == nullptr || out == nullptr || !screenshot->selecting)
    {
        return 0;
    }
    reach_rect_f32 image = active->screen_bounds;
    float left = floorf(fminf(screenshot->anchor.x, screenshot->current.x));
    float top = floorf(fminf(screenshot->anchor.y, screenshot->current.y));
    float right = ceilf(fmaxf(screenshot->anchor.x, screenshot->current.x));
    float bottom = ceilf(fmaxf(screenshot->anchor.y, screenshot->current.y));
    left = reach_screenshot_clamp(left, image.x, image.x + image.width);
    right = reach_screenshot_clamp(right, image.x, image.x + image.width);
    top = reach_screenshot_clamp(top, image.y, image.y + image.height);
    bottom = reach_screenshot_clamp(bottom, image.y, image.y + image.height);
    *out = {left, top, right - left, bottom - top};
    return out->width > 0.0f && out->height > 0.0f;
}

static void reach_screenshot_retire_images(reach_screenshot *screenshot)
{
    for (size_t index = 0; index < screenshot->image_count; ++index)
    {
        uint64_t image_id = screenshot->images[index].image_id;
        try
        {
            screenshot->retired_images.push_back(image_id);
        }
        catch (...)
        {
            if (screenshot->capture.ops.release != nullptr)
            {
                screenshot->capture.ops.release(screenshot->capture.capture, image_id);
            }
        }
        screenshot->images[index] = {};
    }
    screenshot->image_count = 0;
    screenshot->active_image = 0;
}

static size_t reach_screenshot_image_at_point(const reach_screenshot *screenshot,
                                              reach_point_f32 point)
{
    for (size_t index = 0; index < screenshot->image_count; ++index)
    {
        if (reach_screenshot_rect_contains(screenshot->images[index].screen_bounds, point))
        {
            return index;
        }
    }
    return screenshot->image_count;
}

static int32_t reach_screenshot_capture_monitor(reach_screenshot *screenshot,
                                                reach_rect_f32 monitor)
{
    if (screenshot->image_count >= REACH_DISPLAY_MAX_MONITORS || monitor.width <= 0.0f ||
        monitor.height <= 0.0f)
    {
        return 0;
    }
    reach_screen_image image = {};
    if (screenshot->capture.ops.capture(screenshot->capture.capture, monitor, &image) != REACH_OK ||
        image.image_id == 0 || image.pixels.bgra_pixels == nullptr)
    {
        return 0;
    }
    screenshot->images[screenshot->image_count++] = image;
    return 1;
}

static int32_t reach_screenshot_capture(reach_screenshot *screenshot)
{
    if (screenshot->capture.ops.capture == nullptr)
    {
        return 0;
    }
    const reach_display_environment *display = &screenshot->display;
    for (size_t index = 0; index < display->monitor_count; ++index)
    {
        (void)reach_screenshot_capture_monitor(screenshot, display->monitors[index]);
    }
    if (screenshot->image_count == 0)
    {
        (void)reach_screenshot_capture_monitor(screenshot, display->primary_bounds);
    }
    if (screenshot->image_count == 0)
    {
        return 0;
    }
    reach_point_f32 primary_center = {
        display->primary_bounds.x + display->primary_bounds.width * 0.5f,
        display->primary_bounds.y + display->primary_bounds.height * 0.5f};
    size_t active = screenshot->has_open_point
                        ? reach_screenshot_image_at_point(screenshot, screenshot->open_point)
                        : screenshot->image_count;
    if (active >= screenshot->image_count)
    {
        active = reach_screenshot_image_at_point(screenshot, primary_center);
    }
    screenshot->active_image = active < screenshot->image_count ? active : 0;
    screenshot->has_open_point = 0;
    return 1;
}

static int32_t reach_screenshot_follow_point(reach_screenshot *screenshot, reach_point_f32 point)
{
    size_t index = reach_screenshot_image_at_point(screenshot, point);
    if (index >= screenshot->image_count || index == screenshot->active_image)
    {
        return 0;
    }
    screenshot->active_image = index;
    screenshot->bounds = screenshot->images[index].screen_bounds;
    return 1;
}

static int32_t reach_screenshot_copy_selection(reach_screenshot *screenshot)
{
    const reach_screen_image *active = reach_screenshot_active(screenshot);
    reach_rect_f32 selection = {};
    if (active == nullptr || !reach_screenshot_selection_rect(screenshot, &selection) ||
        selection.width < REACH_SCREENSHOT_MIN_SELECTION_PX ||
        selection.height < REACH_SCREENSHOT_MIN_SELECTION_PX ||
        screenshot->clipboard.ops.set_image == nullptr)
    {
        return 0;
    }
    reach_rect_f32 image_bounds = active->screen_bounds;
    reach_image_view cropped =
        reach_image_view_crop(active->pixels, (uint32_t)(selection.x - image_bounds.x),
                              (uint32_t)(selection.y - image_bounds.y), (uint32_t)selection.width,
                              (uint32_t)selection.height);
    return cropped.bgra_pixels != nullptr &&
           screenshot->clipboard.ops.set_image(screenshot->clipboard.provider, &cropped) ==
               REACH_OK;
}

void reach_screenshot_attach_ports(reach_screenshot *screenshot,
                                   const reach_screen_capture_port *capture,
                                   const reach_clipboard_port *clipboard)
{
    if (screenshot == nullptr)
    {
        return;
    }
    screenshot->capture = capture != nullptr ? *capture : reach_screen_capture_port{};
    screenshot->clipboard = clipboard != nullptr ? *clipboard : reach_clipboard_port{};
}

void reach_screenshot_set_routes(reach_screenshot *screenshot,
                                 const reach_screenshot_routes *routes)
{
    if (screenshot != nullptr)
    {
        screenshot->routes = routes != nullptr ? *routes : reach_screenshot_routes{};
    }
}

void reach_screenshot_set_display(reach_screenshot *screenshot,
                                  const reach_display_environment *display)
{
    if (screenshot != nullptr && display != nullptr)
    {
        screenshot->display = *display;
    }
}

int32_t reach_screenshot_is_open(const reach_screenshot *screenshot)
{
    return screenshot != nullptr && screenshot->open;
}

int32_t reach_screenshot_set_open(reach_screenshot *screenshot, int32_t open)
{
    if (screenshot == nullptr)
    {
        return 0;
    }
    int32_t next = open ? 1 : 0;
    if (screenshot->open == next)
    {
        return 0;
    }
    screenshot->selecting = 0;
    screenshot->pointer_captured = 0;
    reach_screenshot_retire_images(screenshot);
    if (next)
    {
        if (!reach_screenshot_capture(screenshot))
        {
            return 0;
        }
        screenshot->bounds = screenshot->images[screenshot->active_image].screen_bounds;
    }
    screenshot->open = next;
    return 1;
}

int32_t reach_screenshot_arrange(reach_screenshot *screenshot, reach_rect_f32 monitor_bounds)
{
    (void)monitor_bounds;
    const reach_screen_image *active = reach_screenshot_active(screenshot);
    return active != nullptr && (screenshot->bounds.x != active->screen_bounds.x ||
                                 screenshot->bounds.y != active->screen_bounds.y ||
                                 screenshot->bounds.width != active->screen_bounds.width ||
                                 screenshot->bounds.height != active->screen_bounds.height);
}

size_t reach_screenshot_take_retired_images(reach_screenshot *screenshot, uint64_t *out_ids,
                                            size_t capacity)
{
    if (screenshot == nullptr || out_ids == nullptr)
    {
        return 0;
    }
    size_t count = 0;
    while (count < capacity && !screenshot->retired_images.empty())
    {
        out_ids[count++] = screenshot->retired_images.back();
        screenshot->retired_images.pop_back();
    }
    return count;
}

size_t reach_screenshot_image_count(const reach_screenshot *screenshot)
{
    return screenshot != nullptr ? screenshot->image_count : 0;
}

uint64_t reach_screenshot_image_at(const reach_screenshot *screenshot, size_t index)
{
    return screenshot != nullptr && index < screenshot->image_count
               ? screenshot->images[index].image_id
               : 0;
}

void reach_screenshot_release_image(reach_screenshot *screenshot, uint64_t image_id)
{
    if (screenshot != nullptr && image_id != 0 && screenshot->capture.ops.release != nullptr)
    {
        screenshot->capture.ops.release(screenshot->capture.capture, image_id);
    }
}

void reach_screenshot_forget_images(reach_screenshot *screenshot)
{
    if (screenshot != nullptr)
    {
        for (size_t index = 0; index < screenshot->image_count; ++index)
        {
            screenshot->images[index] = {};
        }
        screenshot->image_count = 0;
        screenshot->active_image = 0;
        screenshot->open = 0;
        screenshot->selecting = 0;
        screenshot->pointer_captured = 0;
    }
}

static void reach_screenshot_capsule_reset(void *capsule)
{
    reach_screenshot *screenshot = static_cast<reach_screenshot *>(capsule);
    if (screenshot != nullptr)
    {
        (void)reach_screenshot_set_open(screenshot, 0);
    }
}

static int32_t reach_screenshot_capsule_is_open(const void *capsule)
{
    return reach_screenshot_is_open(static_cast<const reach_screenshot *>(capsule));
}

static void reach_screenshot_capsule_on_game_mode(void *capsule, int32_t enabled)
{
    reach_screenshot *screenshot = static_cast<reach_screenshot *>(capsule);
    if (enabled && screenshot != nullptr)
    {
        (void)reach_screenshot_set_open(screenshot, 0);
    }
}

static int32_t reach_screenshot_capsule_wants_pointer_move(const void *capsule)
{
    return reach_screenshot_is_open(static_cast<const reach_screenshot *>(capsule));
}

static int32_t reach_screenshot_capsule_pointer_sequence_active(const void *capsule)
{
    const reach_screenshot *screenshot = static_cast<const reach_screenshot *>(capsule);
    return screenshot != nullptr && screenshot->selecting;
}

static void reach_screenshot_hold_pointer(reach_screenshot *screenshot,
                                          reach_capsule_pointer_result *out)
{
    if (!screenshot->pointer_captured)
    {
        screenshot->pointer_captured = 1;
        out->capture = 1;
    }
}

static void reach_screenshot_follow(reach_screenshot *screenshot, reach_point_f32 point,
                                    reach_capsule_pointer_result *out)
{
    if (!screenshot->selecting && reach_screenshot_follow_point(screenshot, point))
    {
        out->relayout = 1;
        out->redraw = 1;
    }
}

static void reach_screenshot_capsule_handle_pointer(void *capsule, const reach_pointer_event *event,
                                                    reach_capsule_pointer_result *out)
{
    if (out == nullptr)
    {
        return;
    }
    *out = {};
    reach_screenshot *screenshot = static_cast<reach_screenshot *>(capsule);
    if (screenshot == nullptr || event == nullptr || !screenshot->open)
    {
        return;
    }
    reach_point_f32 point = {(float)event->x, (float)event->y};
    if (event->coordinate_space == REACH_POINTER_COORDINATE_SURFACE_LOCAL)
    {
        point.x += screenshot->bounds.x;
        point.y += screenshot->bounds.y;
    }

    switch (event->kind)
    {
    case REACH_POINTER_EVENT_DOWN:
        out->handled = 1;
        if (event->button == REACH_POINTER_BUTTON_SECONDARY)
        {
            out->action.kind = REACH_FEATURE_ACTION_CLOSE_SELF;
            out->capture = screenshot->pointer_captured ? -1 : 0;
            screenshot->pointer_captured = 0;
            out->redraw = 1;
            return;
        }
        if (event->button != REACH_POINTER_BUTTON_PRIMARY)
        {
            return;
        }
        reach_screenshot_follow(screenshot, point, out);
        reach_screenshot_hold_pointer(screenshot, out);
        screenshot->selecting = 1;
        screenshot->anchor = point;
        screenshot->current = point;
        out->redraw = 1;
        return;
    case REACH_POINTER_EVENT_MOVE:
        out->handled = 1;
        reach_screenshot_hold_pointer(screenshot, out);
        if (screenshot->selecting)
        {
            screenshot->current = point;
            out->redraw = 1;
        }
        else
        {
            reach_screenshot_follow(screenshot, point, out);
        }
        return;
    case REACH_POINTER_EVENT_UP:
        if (!screenshot->selecting || event->button != REACH_POINTER_BUTTON_PRIMARY)
        {
            return;
        }
        screenshot->current = point;
        out->handled = 1;
        out->redraw = 1;
        if (reach_screenshot_copy_selection(screenshot))
        {
            screenshot->selecting = 0;
            screenshot->pointer_captured = 0;
            out->capture = -1;
            out->action.kind = REACH_FEATURE_ACTION_CLOSE_SELF;
            if (screenshot->routes.copied != nullptr)
            {
                screenshot->routes.copied(screenshot->routes.user);
            }
            return;
        }
        screenshot->selecting = 0;
        return;
    case REACH_POINTER_EVENT_CANCEL:
        screenshot->pointer_captured = 0;
        if (screenshot->selecting)
        {
            screenshot->selecting = 0;
            out->redraw = 1;
        }
        return;
    default:
        return;
    }
}

static void reach_screenshot_capsule_handle_event(void *capsule, const reach_ui_event *event,
                                                  reach_capsule_event_result *out)
{
    if (out == nullptr)
    {
        return;
    }
    *out = {};
    reach_screenshot *screenshot = static_cast<reach_screenshot *>(capsule);
    if (screenshot == nullptr || event == nullptr)
    {
        return;
    }
    if (event->type == REACH_UI_EVENT_SCREENSHOT_TOGGLE)
    {
        screenshot->has_open_point = !screenshot->open;
        screenshot->open_point = {(float)event->x, (float)event->y};
        return;
    }
    if (!screenshot->open)
    {
        return;
    }
    if (event->type == REACH_UI_EVENT_ESCAPE)
    {
        out->handled = 1;
        out->redraw = 1;
        out->action.kind = REACH_FEATURE_ACTION_CLOSE_SELF;
    }
}

static void reach_screenshot_capsule_surface_geometry(const void *capsule,
                                                      reach_feature_surface_geometry *out)
{
    if (out == nullptr)
    {
        return;
    }
    *out = {};
    const reach_screenshot *screenshot = static_cast<const reach_screenshot *>(capsule);
    if (screenshot == nullptr)
    {
        return;
    }
    out->visible_bounds = screenshot->bounds;
    out->envelope_bounds = screenshot->bounds;
}

const reach_feature_capsule_ops *reach_screenshot_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        reach_screenshot_capsule_reset,
        nullptr,
        reach_screenshot_capsule_is_open,
        reach_screenshot_capsule_on_game_mode,
        nullptr,
        reach_screenshot_capsule_wants_pointer_move,
        reach_screenshot_capsule_handle_pointer,
        reach_screenshot_capsule_pointer_sequence_active,
        nullptr,
        reach_screenshot_capsule_surface_geometry,
        reach_screenshot_capsule_pointer_sequence_active,
        reach_screenshot_capsule_handle_event,
    };
    return &ops;
}

const reach_ui_event_type *reach_screenshot_activation_events(size_t *out_count)
{
    static const reach_ui_event_type events[] = {REACH_UI_EVENT_SCREENSHOT_TOGGLE};
    if (out_count != nullptr)
    {
        *out_count = sizeof(events) / sizeof(events[0]);
    }
    return events;
}

const reach_ui_event_type *reach_screenshot_routed_events(size_t *out_count)
{
    static const reach_ui_event_type events[] = {REACH_UI_EVENT_SCREENSHOT_TOGGLE,
                                                 REACH_UI_EVENT_ESCAPE};
    if (out_count != nullptr)
    {
        *out_count = sizeof(events) / sizeof(events[0]);
    }
    return events;
}

reach_result reach_screenshot_create(reach_screenshot **out_screenshot)
{
    if (out_screenshot == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_screenshot *screenshot = new (std::nothrow) reach_screenshot();
    if (screenshot == nullptr)
    {
        return REACH_ERROR;
    }
    *out_screenshot = screenshot;
    return REACH_OK;
}

void reach_screenshot_destroy(reach_screenshot *screenshot)
{
    if (screenshot == nullptr)
    {
        return;
    }
    reach_screenshot_retire_images(screenshot);
    for (uint64_t image_id : screenshot->retired_images)
    {
        reach_screenshot_release_image(screenshot, image_id);
    }
    delete screenshot;
}
