#include "render_d2d_internal.h"

reach_result reach_d2d_draw_text(reach_render_backend *backend, const reach_render_command *command)
{
    if (backend == nullptr || command == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (target == nullptr || backend->text_factory == nullptr)
    {
        return REACH_ERROR;
    }

    const wchar_t *text = reinterpret_cast<const wchar_t *>(command->text);
    if (text == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    IDWriteTextFormat *format = nullptr;
    ID2D1SolidColorBrush *brush = nullptr;

    DWRITE_FONT_WEIGHT weight = command->text_weight > 0
                                    ? static_cast<DWRITE_FONT_WEIGHT>(command->text_weight)
                                    : DWRITE_FONT_WEIGHT_NORMAL;

    float text_size = command->text_size > 0.0f ? command->text_size : 16.0f;

    HRESULT hr = backend->text_factory->CreateTextFormat(
        L"Segoe UI", nullptr, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        text_size, L"", &format);

    if (SUCCEEDED(hr))
    {
        DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        if (command->text_alignment == REACH_TEXT_ALIGNMENT_LEADING)
        {
            alignment = DWRITE_TEXT_ALIGNMENT_LEADING;
        }
        else if (command->text_alignment == REACH_TEXT_ALIGNMENT_CENTER)
        {
            alignment = DWRITE_TEXT_ALIGNMENT_CENTER;
        }

        (void)format->SetTextAlignment(alignment);
        (void)format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        if (command->text_ellipsis)
        {
            (void)format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

            IDWriteInlineObject *ellipsis = nullptr;
            HRESULT trim_hr = backend->text_factory->CreateEllipsisTrimmingSign(format, &ellipsis);

            if (SUCCEEDED(trim_hr))
            {
                DWRITE_TRIMMING trimming = {};
                trimming.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;

                (void)format->SetTrimming(&trimming, ellipsis);
                ellipsis->Release();
            }
        }
    }

    if (SUCCEEDED(hr))
    {
        hr = target->CreateSolidColorBrush(reach_d2d_color(command->color), &brush);
    }

    if (FAILED(hr))
    {
        if (format != nullptr)
        {
            format->Release();
        }
        return REACH_ERROR;
    }

    D2D1_RECT_F rect =
        D2D1::RectF(command->rect.x, command->rect.y, command->rect.x + command->rect.width,
                    command->rect.y + command->rect.height);

    target->DrawText(text, static_cast<UINT32>(wcslen(text)), format, rect, brush);

    brush->Release();
    format->Release();

    return REACH_OK;
}

reach_result reach_d2d_draw_textbox(reach_render_backend *backend,
                                    const reach_render_command *command)
{
    if (backend == nullptr || command == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (target == nullptr || backend->text_factory == nullptr)
    {
        return REACH_ERROR;
    }

    const D2D1_RECT_F bounds =
        D2D1::RectF(command->rect.x, command->rect.y, command->rect.x + command->rect.width,
                    command->rect.y + command->rect.height);
    const float radius = command->radius;

    /* Background (alpha honored — the whole point of leaving the native EDIT). */
    if (command->color.a > 0.0f)
    {
        ID2D1SolidColorBrush *bg = nullptr;
        if (SUCCEEDED(target->CreateSolidColorBrush(reach_d2d_color(command->color), &bg)))
        {
            if (radius > 0.0f)
            {
                target->FillRoundedRectangle(D2D1::RoundedRect(bounds, radius, radius), bg);
            }
            else
            {
                target->FillRectangle(bounds, bg);
            }
            bg->Release();
        }
    }

    /* Border. */
    if (command->stroke_width > 0.0f && command->border_color.a > 0.0f)
    {
        ID2D1SolidColorBrush *border = nullptr;
        if (SUCCEEDED(target->CreateSolidColorBrush(reach_d2d_color(command->border_color), &border)))
        {
            if (radius > 0.0f)
            {
                target->DrawRoundedRectangle(D2D1::RoundedRect(bounds, radius, radius), border,
                                             command->stroke_width);
            }
            else
            {
                target->DrawRectangle(bounds, border, command->stroke_width);
            }
            border->Release();
        }
    }

    const wchar_t *text = reinterpret_cast<const wchar_t *>(command->text);
    const wchar_t *placeholder = reinterpret_cast<const wchar_t *>(command->placeholder);
    const UINT32 text_length = static_cast<UINT32>(wcslen(text));
    const int32_t showing_placeholder = (text_length == 0);
    const wchar_t *display = showing_placeholder ? placeholder : text;
    const UINT32 display_length = static_cast<UINT32>(wcslen(display));

    const float font_size = command->text_size > 0.0f ? command->text_size : 16.0f;
    const DWRITE_FONT_WEIGHT weight = command->text_weight > 0
                                          ? static_cast<DWRITE_FONT_WEIGHT>(command->text_weight)
                                          : DWRITE_FONT_WEIGHT_NORMAL;
    /* Half-em horizontal inset; scales with the (already DPI-scaled) font size. */
    const float inset = font_size * 0.5f;
    const float content_left = command->rect.x + inset;
    const float content_top = command->rect.y;
    const float content_width = command->rect.width - inset * 2.0f > 0.0f
                                    ? command->rect.width - inset * 2.0f
                                    : 0.0f;
    const float content_height = command->rect.height;

    IDWriteTextFormat *format = nullptr;
    HRESULT hr = backend->text_factory->CreateTextFormat(L"Segoe UI", nullptr, weight,
                                                         DWRITE_FONT_STYLE_NORMAL,
                                                         DWRITE_FONT_STRETCH_NORMAL, font_size, L"",
                                                         &format);
    if (FAILED(hr))
    {
        return REACH_ERROR;
    }
    (void)format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    (void)format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    (void)format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    IDWriteTextLayout *layout = nullptr;
    hr = backend->text_factory->CreateTextLayout(display, display_length, format, content_width,
                                                 content_height, &layout);
    format->Release();
    if (FAILED(hr) || layout == nullptr)
    {
        return REACH_ERROR;
    }

    /* Selection highlight (only meaningful on real text, not the placeholder). */
    if (!showing_placeholder && command->selection_color.a > 0.0f &&
        command->selection_start != command->selection_end)
    {
        UINT32 sel_begin = static_cast<UINT32>(command->selection_start < command->selection_end
                                                   ? command->selection_start
                                                   : command->selection_end);
        UINT32 sel_count = static_cast<UINT32>(command->selection_start < command->selection_end
                                                   ? command->selection_end - command->selection_start
                                                   : command->selection_start - command->selection_end);
        /* Single-line, no-wrap layout: a contiguous selection is at most a few
         * runs (bidi can split it). A small fixed buffer is plenty. */
        DWRITE_HIT_TEST_METRICS metrics[8] = {};
        UINT32 hit_count = 0;
        if (SUCCEEDED(layout->HitTestTextRange(sel_begin, sel_count, content_left, content_top,
                                               metrics, 8, &hit_count)))
        {
            ID2D1SolidColorBrush *sel = nullptr;
            if (SUCCEEDED(
                    target->CreateSolidColorBrush(reach_d2d_color(command->selection_color), &sel)))
            {
                for (UINT32 i = 0; i < hit_count && i < 8; ++i)
                {
                    target->FillRectangle(
                        D2D1::RectF(metrics[i].left, metrics[i].top,
                                    metrics[i].left + metrics[i].width,
                                    metrics[i].top + metrics[i].height),
                        sel);
                }
                sel->Release();
            }
        }
    }

    /* Text / placeholder. */
    reach_color text_color = showing_placeholder ? command->placeholder_color : command->text_color;
    ID2D1SolidColorBrush *text_brush = nullptr;
    if (SUCCEEDED(target->CreateSolidColorBrush(reach_d2d_color(text_color), &text_brush)))
    {
        target->DrawTextLayout(D2D1::Point2F(content_left, content_top), layout, text_brush,
                               D2D1_DRAW_TEXT_OPTIONS_CLIP);
        text_brush->Release();
    }

    /* Caret (a thin vertical bar at the caret index, while the blink is "on"). */
    if (command->caret_visible)
    {
        UINT32 caret_pos = static_cast<UINT32>(command->caret_index < 0 ? 0 : command->caret_index);
        if (showing_placeholder)
        {
            caret_pos = 0;
        }
        else if (caret_pos > text_length)
        {
            caret_pos = text_length;
        }

        FLOAT caret_x = 0.0f;
        FLOAT caret_y = 0.0f;
        DWRITE_HIT_TEST_METRICS caret_metrics = {};
        if (SUCCEEDED(layout->HitTestTextPosition(showing_placeholder ? 0 : caret_pos, FALSE,
                                                   &caret_x, &caret_y, &caret_metrics)))
        {
            ID2D1SolidColorBrush *caret_brush = nullptr;
            reach_color caret_color = command->text_color;
            if (SUCCEEDED(target->CreateSolidColorBrush(reach_d2d_color(caret_color), &caret_brush)))
            {
                float x = content_left + caret_x;
                float top = content_top + caret_y;
                float bottom = top + caret_metrics.height;
                target->DrawLine(D2D1::Point2F(x, top), D2D1::Point2F(x, bottom), caret_brush,
                                 1.5f);
                caret_brush->Release();
            }
        }
    }

    layout->Release();
    return REACH_OK;
}
