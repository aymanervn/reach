#include "windows_adapters_internal.h"

#include <windows.h>

struct reach_input_language
{
    int32_t placeholder;
};

static reach_input_language reach_input_language_instance = {};

static HKL reach_input_language_layout(reach_window_id foreground_window)
{
    HWND window = reinterpret_cast<HWND>(foreground_window);
    DWORD thread = window != nullptr ? GetWindowThreadProcessId(window, nullptr) : 0;
    return GetKeyboardLayout(thread);
}

static reach_result reach_input_language_get_state(reach_input_language *language,
                                                   reach_window_id foreground_window,
                                                   reach_input_language_state *out_state)
{
    if (language == nullptr || out_state == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_state = {};

    HKL layout = reach_input_language_layout(foreground_window);
    if (layout == nullptr)
    {
        return REACH_ERROR;
    }

    LANGID language_id = LOWORD(reinterpret_cast<UINT_PTR>(layout));
    LCID locale = MAKELCID(language_id, SORT_DEFAULT);

    wchar_t code[9] = {};
    if (GetLocaleInfoW(locale, LOCALE_SISO639LANGNAME, code, 9) == 0)
    {
        return REACH_ERROR;
    }

    for (size_t index = 0; index < 8 && code[index] != 0; ++index)
    {
        wchar_t character = code[index];
        if (character >= L'a' && character <= L'z')
        {
            character = (wchar_t)(character - L'a' + L'A');
        }
        out_state->code[index] = (uint16_t)character;
    }
    out_state->layout_id = (uint32_t)(UINT_PTR)layout;
    return REACH_OK;
}

static reach_result reach_input_language_cycle_next(reach_input_language *language,
                                                    reach_window_id foreground_window)
{
    if (language == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    HWND window = reinterpret_cast<HWND>(foreground_window);
    if (window == nullptr)
    {
        return REACH_ERROR;
    }

    return PostMessageW(window, WM_INPUTLANGCHANGEREQUEST, INPUTLANGCHANGE_FORWARD, 0)
               ? REACH_OK
               : REACH_ERROR;
}

static void reach_input_language_destroy(reach_input_language *language)
{
    (void)language;
}

reach_result reach_windows_create_input_language(reach_input_language_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    out_port->language = &reach_input_language_instance;
    out_port->ops.get_state = reach_input_language_get_state;
    out_port->ops.cycle_next = reach_input_language_cycle_next;
    out_port->ops.destroy = reach_input_language_destroy;
    return REACH_OK;
}
