#include "windows_adapters_internal.h"

#include <windows.h>
#include <commctrl.h>

#include <new>

static const size_t REACH_TEXTBOX_MAX_PENDING_EVENTS = 32;
static const UINT_PTR REACH_TEXTBOX_EDIT_SUBCLASS_ID = 1;
static const wchar_t *REACH_TEXTBOX_HOST_CLASS_NAME = L"ReachTextboxHostWindow";

struct reach_textbox
{
    HWND owner_hwnd;
    HWND host_hwnd;
    HWND edit_hwnd;

    HFONT font;
    HBRUSH background_brush;
    COLORREF text_color;
    COLORREF background_color;
    int edit_height;

    reach_textbox_event_callback callback;
    void *callback_user;

    reach_textbox_event pending_events[REACH_TEXTBOX_MAX_PENDING_EVENTS];
    size_t pending_event_count;
    int32_t suppress_change_event;
};

static BYTE reach_textbox_color_channel(float value)
{
    if (value < 0.0f)
        value = 0.0f;
    if (value > 1.0f)
        value = 1.0f;
    return static_cast<BYTE>(value * 255.0f + 0.5f);
}

static COLORREF reach_textbox_rgb(float r, float g, float b)
{
    return RGB(reach_textbox_color_channel(r), reach_textbox_color_channel(g),
               reach_textbox_color_channel(b));
}

static int reach_textbox_measure_edit_height(reach_textbox *textbox)
{
    if (textbox == nullptr || textbox->edit_hwnd == nullptr)
        return 1;

    HDC dc = GetDC(textbox->edit_hwnd);
    if (dc == nullptr)
        return 1;

    HGDIOBJ old_font = nullptr;
    if (textbox->font != nullptr)
        old_font = SelectObject(dc, textbox->font);

    TEXTMETRICW metrics = {};
    int height = 1;
    if (GetTextMetricsW(dc, &metrics))
    {
        int vertical_slack = GetSystemMetrics(SM_CYBORDER) * 4;
        if (vertical_slack < 4)
            vertical_slack = 4;
        height = metrics.tmHeight + metrics.tmExternalLeading + vertical_slack;
    }

    if (old_font != nullptr)
        SelectObject(dc, old_font);
    ReleaseDC(textbox->edit_hwnd, dc);

    return height > 1 ? height : 1;
}

static void reach_textbox_move_edit(reach_textbox *textbox, int width, int host_height)
{
    if (textbox == nullptr || textbox->edit_hwnd == nullptr)
        return;

    int edit_height = textbox->edit_height > 0 ? textbox->edit_height : host_height;
    if (edit_height > host_height)
        edit_height = host_height;
    if (edit_height < 1)
        edit_height = 1;

    int edit_y = (host_height - edit_height) / 2;
    if (edit_y < 0)
        edit_y = 0;

    SetWindowPos(textbox->edit_hwnd, nullptr, 0, edit_y, width, edit_height,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

static size_t reach_textbox_read_text(const reach_textbox *textbox, uint16_t *out_text,
                                      size_t text_capacity)
{
    if (textbox == nullptr || textbox->edit_hwnd == nullptr || out_text == nullptr ||
        text_capacity == 0)
    {
        return 0;
    }

    int copied = GetWindowTextW(textbox->edit_hwnd, reinterpret_cast<wchar_t *>(out_text),
                                static_cast<int>(text_capacity));
    if (copied < 0)
        copied = 0;

    out_text[text_capacity - 1] = 0;
    return static_cast<size_t>(copied);
}

static void reach_textbox_queue_event(reach_textbox *textbox, reach_textbox_event_type type)
{
    if (textbox == nullptr || type == REACH_TEXTBOX_EVENT_NONE)
        return;

    reach_textbox_event event = {};
    event.type = type;
    event.text_length = reach_textbox_read_text(textbox, event.text, REACH_TEXTBOX_TEXT_CAPACITY);

    if (type == REACH_TEXTBOX_EVENT_TEXT_CHANGED && textbox->pending_event_count > 0 &&
        textbox->pending_events[textbox->pending_event_count - 1].type ==
            REACH_TEXTBOX_EVENT_TEXT_CHANGED)
    {
        textbox->pending_events[textbox->pending_event_count - 1] = event;
        return;
    }

    size_t index = textbox->pending_event_count < REACH_TEXTBOX_MAX_PENDING_EVENTS
                       ? textbox->pending_event_count++
                       : REACH_TEXTBOX_MAX_PENDING_EVENTS - 1;
    textbox->pending_events[index] = event;
}

static LRESULT CALLBACK reach_textbox_edit_subclass_proc(HWND hwnd, UINT message, WPARAM wparam,
                                                         LPARAM lparam, UINT_PTR subclass_id,
                                                         DWORD_PTR reference_data)
{
    (void)subclass_id;

    reach_textbox *textbox = reinterpret_cast<reach_textbox *>(reference_data);
    if (textbox == nullptr)
        return DefSubclassProc(hwnd, message, wparam, lparam);

    if (message == WM_KEYDOWN)
    {
        if (wparam == VK_RETURN)
        {
            reach_textbox_queue_event(textbox, REACH_TEXTBOX_EVENT_SUBMIT);
            return 0;
        }
        if (wparam == VK_ESCAPE)
        {
            reach_textbox_queue_event(textbox, REACH_TEXTBOX_EVENT_CANCEL);
            return 0;
        }
        if (wparam == VK_UP)
        {
            reach_textbox_queue_event(textbox, REACH_TEXTBOX_EVENT_NAVIGATE_UP);
            return 0;
        }
        if (wparam == VK_DOWN)
        {
            reach_textbox_queue_event(textbox, REACH_TEXTBOX_EVENT_NAVIGATE_DOWN);
            return 0;
        }
    }

    if (message == WM_CHAR)
    {
        if (wparam == VK_RETURN || wparam == VK_ESCAPE)
            return 0;
    }

    return DefSubclassProc(hwnd, message, wparam, lparam);
}

static LRESULT CALLBACK reach_textbox_host_proc(HWND hwnd, UINT message, WPARAM wparam,
                                                LPARAM lparam)
{
    reach_textbox *textbox =
        reinterpret_cast<reach_textbox *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE)
    {
        CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lparam);
        textbox = reinterpret_cast<reach_textbox *>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(textbox));
        return TRUE;
    }

    if (textbox != nullptr)
    {
        if ((message == WM_CTLCOLOREDIT || message == WM_CTLCOLORSTATIC) &&
            reinterpret_cast<HWND>(lparam) == textbox->edit_hwnd)
        {
            HDC dc = reinterpret_cast<HDC>(wparam);
            if (dc != nullptr)
            {
                SetTextColor(dc, textbox->text_color);
                SetBkColor(dc, textbox->background_color);
                SetBkMode(dc, OPAQUE);
            }
            return reinterpret_cast<LRESULT>(textbox->background_brush);
        }

        if (message == WM_ERASEBKGND)
        {
            HDC dc = reinterpret_cast<HDC>(wparam);
            RECT rect = {};
            if (dc != nullptr && GetClientRect(hwnd, &rect))
            {
                FillRect(dc, &rect, textbox->background_brush);
                return 1;
            }
        }

        if (message == WM_PAINT)
        {
            PAINTSTRUCT paint = {};
            HDC dc = BeginPaint(hwnd, &paint);
            if (dc != nullptr)
            {
                RECT rect = {};
                if (GetClientRect(hwnd, &rect))
                    FillRect(dc, &rect, textbox->background_brush);
            }
            EndPaint(hwnd, &paint);
            return 0;
        }

        if (message == WM_COMMAND && reinterpret_cast<HWND>(lparam) == textbox->edit_hwnd &&
            HIWORD(wparam) == EN_CHANGE)
        {
            if (!textbox->suppress_change_event)
                reach_textbox_queue_event(textbox, REACH_TEXTBOX_EVENT_TEXT_CHANGED);
            return 0;
        }

        if (message == WM_CLOSE)
        {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static reach_result reach_textbox_register_host_class()
{
    HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW existing = {};
    if (GetClassInfoExW(instance, REACH_TEXTBOX_HOST_CLASS_NAME, &existing))
        return REACH_OK;

    WNDCLASSEXW cls = {};
    cls.cbSize = sizeof(cls);
    cls.lpfnWndProc = reach_textbox_host_proc;
    cls.hInstance = instance;
    cls.hCursor = LoadCursorW(nullptr, IDC_IBEAM);
    cls.hbrBackground = nullptr;
    cls.lpszClassName = REACH_TEXTBOX_HOST_CLASS_NAME;

    return RegisterClassExW(&cls) != 0 ? REACH_OK : REACH_ERROR;
}

static reach_result reach_textbox_set_bounds(reach_textbox *textbox, reach_rect_f32 bounds)
{
    if (textbox == nullptr || textbox->owner_hwnd == nullptr || textbox->host_hwnd == nullptr ||
        textbox->edit_hwnd == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    POINT origin = {static_cast<LONG>(bounds.x), static_cast<LONG>(bounds.y)};
    ClientToScreen(textbox->owner_hwnd, &origin);

    int width = static_cast<int>(bounds.width);
    int height = static_cast<int>(bounds.height);
    if (width < 1)
        width = 1;
    if (height < 1)
        height = 1;

    BOOL host_ok = SetWindowPos(textbox->host_hwnd, HWND_TOPMOST, origin.x, origin.y, width, height,
                                SWP_NOACTIVATE);
    reach_textbox_move_edit(textbox, width, height);
    return host_ok ? REACH_OK : REACH_ERROR;
}

static reach_result reach_textbox_set_style(reach_textbox *textbox,
                                            const reach_textbox_style *style)
{
    if (textbox == nullptr || textbox->edit_hwnd == nullptr || style == nullptr)
        return REACH_INVALID_ARGUMENT;

    if (textbox->font != nullptr)
    {
        DeleteObject(textbox->font);
        textbox->font = nullptr;
    }

    int height = -static_cast<int>(style->font_size > 0.0f ? style->font_size + 0.5f : 16.0f);
    int weight = style->font_weight > 0 ? style->font_weight : FW_NORMAL;

    textbox->font = CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (textbox->font != nullptr)
        SendMessageW(textbox->edit_hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(textbox->font), TRUE);

    textbox->edit_height = reach_textbox_measure_edit_height(textbox);
    RECT host_rect = {};
    if (GetClientRect(textbox->host_hwnd, &host_rect))
    {
        reach_textbox_move_edit(textbox, host_rect.right - host_rect.left,
                                host_rect.bottom - host_rect.top);
    }

    textbox->text_color =
        reach_textbox_rgb(style->text_color.r, style->text_color.g, style->text_color.b);

    textbox->background_color = reach_textbox_rgb(
        style->background_color.r, style->background_color.g, style->background_color.b);

    if (textbox->background_brush != nullptr)
        DeleteObject(textbox->background_brush);

    textbox->background_brush = CreateSolidBrush(textbox->background_color);
    if (textbox->background_brush == nullptr)
        return REACH_ERROR;

    SendMessageW(textbox->edit_hwnd, EM_SETLIMITTEXT,
                 style->max_length > 0 ? static_cast<WPARAM>(style->max_length)
                                       : REACH_TEXTBOX_TEXT_CAPACITY - 1,
                 0);
    SendMessageW(textbox->edit_hwnd, EM_SETCUEBANNER, TRUE,
                 reinterpret_cast<LPARAM>(style->placeholder));

    InvalidateRect(textbox->host_hwnd, nullptr, TRUE);
    InvalidateRect(textbox->edit_hwnd, nullptr, TRUE);
    return REACH_OK;
}

static reach_result reach_textbox_set_text(reach_textbox *textbox, const uint16_t *text)
{
    if (textbox == nullptr || textbox->edit_hwnd == nullptr || text == nullptr)
        return REACH_INVALID_ARGUMENT;

    textbox->suppress_change_event = 1;
    BOOL ok = SetWindowTextW(textbox->edit_hwnd, reinterpret_cast<const wchar_t *>(text));
    SendMessageW(textbox->edit_hwnd, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
    textbox->suppress_change_event = 0;

    return ok ? REACH_OK : REACH_ERROR;
}

static reach_result reach_textbox_get_text(const reach_textbox *textbox, uint16_t *out_text,
                                           size_t text_capacity)
{
    if (textbox == nullptr || out_text == nullptr || text_capacity == 0)
        return REACH_INVALID_ARGUMENT;

    reach_textbox_read_text(textbox, out_text, text_capacity);
    return REACH_OK;
}

static reach_result reach_textbox_show(reach_textbox *textbox)
{
    if (textbox == nullptr || textbox->host_hwnd == nullptr || textbox->edit_hwnd == nullptr)
        return REACH_INVALID_ARGUMENT;

    ShowWindow(textbox->edit_hwnd, SW_SHOW);
    ShowWindow(textbox->host_hwnd, SW_SHOW);
    SetWindowPos(textbox->host_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    return REACH_OK;
}

static reach_result reach_textbox_hide(reach_textbox *textbox)
{
    if (textbox == nullptr || textbox->host_hwnd == nullptr)
        return REACH_INVALID_ARGUMENT;

    if (textbox->edit_hwnd != nullptr)
        ShowWindow(textbox->edit_hwnd, SW_HIDE);
    ShowWindow(textbox->host_hwnd, SW_HIDE);
    return REACH_OK;
}

static reach_result reach_textbox_set_focused(reach_textbox *textbox, int32_t focused)
{
    if (textbox == nullptr || textbox->edit_hwnd == nullptr)
        return REACH_INVALID_ARGUMENT;

    if (focused)
    {
        if (IsWindow(textbox->host_hwnd))
            SetActiveWindow(textbox->host_hwnd);
        if (GetFocus() != textbox->edit_hwnd)
            SetFocus(textbox->edit_hwnd);
    }
    else if (GetFocus() == textbox->edit_hwnd)
    {
        SetFocus(textbox->owner_hwnd);
    }

    return REACH_OK;
}

static reach_result reach_textbox_set_enabled(reach_textbox *textbox, int32_t enabled)
{
    if (textbox == nullptr || textbox->edit_hwnd == nullptr)
        return REACH_INVALID_ARGUMENT;

    EnableWindow(textbox->edit_hwnd, enabled ? TRUE : FALSE);
    return REACH_OK;
}

static reach_result reach_textbox_set_event_callback(reach_textbox *textbox,
                                                     reach_textbox_event_callback callback,
                                                     void *user)
{
    if (textbox == nullptr)
        return REACH_INVALID_ARGUMENT;

    textbox->callback = callback;
    textbox->callback_user = user;
    return REACH_OK;
}

static int32_t reach_textbox_has_pending_events(const reach_textbox *textbox)
{
    return textbox != nullptr && textbox->pending_event_count > 0;
}

static reach_result reach_textbox_dispatch_events(reach_textbox *textbox)
{
    if (textbox == nullptr)
        return REACH_INVALID_ARGUMENT;

    reach_textbox_event events[REACH_TEXTBOX_MAX_PENDING_EVENTS] = {};
    size_t count = textbox->pending_event_count;
    if (count > REACH_TEXTBOX_MAX_PENDING_EVENTS)
        count = REACH_TEXTBOX_MAX_PENDING_EVENTS;

    for (size_t index = 0; index < count; ++index)
        events[index] = textbox->pending_events[index];

    textbox->pending_event_count = 0;

    if (textbox->callback != nullptr)
    {
        for (size_t index = 0; index < count; ++index)
            textbox->callback(textbox->callback_user, &events[index]);
    }

    return REACH_OK;
}

static void reach_textbox_destroy(reach_textbox *textbox)
{
    if (textbox == nullptr)
        return;

    if (textbox->edit_hwnd != nullptr)
    {
        RemoveWindowSubclass(textbox->edit_hwnd, reach_textbox_edit_subclass_proc,
                             REACH_TEXTBOX_EDIT_SUBCLASS_ID);
        DestroyWindow(textbox->edit_hwnd);
    }

    if (textbox->host_hwnd != nullptr)
    {
        SetWindowLongPtrW(textbox->host_hwnd, GWLP_USERDATA, 0);
        DestroyWindow(textbox->host_hwnd);
    }

    if (textbox->font != nullptr)
        DeleteObject(textbox->font);

    if (textbox->background_brush != nullptr)
        DeleteObject(textbox->background_brush);

    delete textbox;
}

reach_result reach_windows_create_textbox(reach_platform_window *parent,
                                          reach_textbox_port *out_port)
{
    if (parent == nullptr || out_port == nullptr)
        return REACH_INVALID_ARGUMENT;

    *out_port = {};

    HWND owner_hwnd = reinterpret_cast<HWND>(reach_windows_platform_window_native_handle(parent));
    if (owner_hwnd == nullptr)
        return REACH_INVALID_ARGUMENT;

    if (reach_textbox_register_host_class() != REACH_OK)
        return REACH_ERROR;

    reach_textbox *textbox = new (std::nothrow) reach_textbox();
    if (textbox == nullptr)
        return REACH_ERROR;

    textbox->owner_hwnd = owner_hwnd;
    textbox->text_color = GetSysColor(COLOR_WINDOWTEXT);
    textbox->background_color = GetSysColor(COLOR_WINDOW);
    textbox->edit_height = 1;
    textbox->background_brush = CreateSolidBrush(textbox->background_color);

    textbox->host_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, REACH_TEXTBOX_HOST_CLASS_NAME, L"",
                                         WS_POPUP | WS_CLIPCHILDREN, 0, 0, 1, 1, owner_hwnd,
                                         nullptr, GetModuleHandleW(nullptr), textbox);

    textbox->edit_hwnd =
        CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL, 0, 0, 1,
                        1, textbox->host_hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);

    if (textbox->background_brush == nullptr || textbox->host_hwnd == nullptr ||
        textbox->edit_hwnd == nullptr ||
        !SetWindowSubclass(textbox->edit_hwnd, reach_textbox_edit_subclass_proc,
                           REACH_TEXTBOX_EDIT_SUBCLASS_ID, reinterpret_cast<DWORD_PTR>(textbox)))
    {
        reach_textbox_destroy(textbox);
        return REACH_ERROR;
    }

    SendMessageW(textbox->edit_hwnd, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                 MAKELPARAM(0, 0));

    out_port->textbox = textbox;
    out_port->ops.set_bounds = reach_textbox_set_bounds;
    out_port->ops.set_style = reach_textbox_set_style;
    out_port->ops.set_text = reach_textbox_set_text;
    out_port->ops.get_text = reach_textbox_get_text;
    out_port->ops.show = reach_textbox_show;
    out_port->ops.hide = reach_textbox_hide;
    out_port->ops.set_focused = reach_textbox_set_focused;
    out_port->ops.set_enabled = reach_textbox_set_enabled;
    out_port->ops.set_event_callback = reach_textbox_set_event_callback;
    out_port->ops.has_pending_events = reach_textbox_has_pending_events;
    out_port->ops.dispatch_events = reach_textbox_dispatch_events;
    out_port->ops.destroy = reach_textbox_destroy;

    return REACH_OK;
}
