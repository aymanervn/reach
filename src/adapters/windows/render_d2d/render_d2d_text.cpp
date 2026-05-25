#include "render_d2d_internal.h"

reach_result reach_d2d_draw_text(
    reach_render_backend *backend,
    const reach_render_command *command
)
{
    if (backend == nullptr || command == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (target == nullptr || backend->text_factory == nullptr) {
        return REACH_ERROR;
    }

    const wchar_t *text = reinterpret_cast<const wchar_t *>(command->text);
    if (text == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    IDWriteTextFormat *format = nullptr;
    ID2D1SolidColorBrush *brush = nullptr;

    DWRITE_FONT_WEIGHT weight = command->text_weight > 0
        ? static_cast<DWRITE_FONT_WEIGHT>(command->text_weight)
        : DWRITE_FONT_WEIGHT_NORMAL;

    float text_size = command->text_size > 0.0f
        ? command->text_size
        : 16.0f;

    HRESULT hr = backend->text_factory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        weight,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        text_size,
        L"",
        &format);

    if (SUCCEEDED(hr)) {
        DWRITE_TEXT_ALIGNMENT alignment = command->text_alignment >= 0
            ? static_cast<DWRITE_TEXT_ALIGNMENT>(command->text_alignment)
            : DWRITE_TEXT_ALIGNMENT_CENTER;

        (void)format->SetTextAlignment(alignment);
        (void)format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

        if (command->text_ellipsis) {
            (void)format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

            IDWriteInlineObject *ellipsis = nullptr;
            HRESULT trim_hr = backend->text_factory->CreateEllipsisTrimmingSign(
                format,
                &ellipsis);

            if (SUCCEEDED(trim_hr)) {
                DWRITE_TRIMMING trimming = {};
                trimming.granularity = DWRITE_TRIMMING_GRANULARITY_CHARACTER;

                (void)format->SetTrimming(&trimming, ellipsis);
                ellipsis->Release();
            }
        }
    }

    if (SUCCEEDED(hr)) {
        hr = target->CreateSolidColorBrush(
            reach_d2d_color(command->color),
            &brush);
    }

    if (FAILED(hr)) {
        if (format != nullptr) {
            format->Release();
        }
        return REACH_ERROR;
    }

    D2D1_RECT_F rect = D2D1::RectF(
        command->rect.x,
        command->rect.y,
        command->rect.x + command->rect.width,
        command->rect.y + command->rect.height);

    target->DrawText(
        text,
        static_cast<UINT32>(wcslen(text)),
        format,
        rect,
        brush);

    brush->Release();
    format->Release();

    return REACH_OK;
}

reach_result reach_d2d_draw_text_caret(
    reach_render_backend *backend,
    const reach_render_command *command
)
{
    if (backend == nullptr || command == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (target == nullptr || backend->text_factory == nullptr) {
        return REACH_ERROR;
    }

    DWRITE_FONT_WEIGHT weight = command->text_weight > 0
        ? static_cast<DWRITE_FONT_WEIGHT>(command->text_weight)
        : DWRITE_FONT_WEIGHT_NORMAL;
    float text_size = command->text_size > 0.0f
        ? command->text_size
        : 16.0f;

    IDWriteTextFormat *format = nullptr;
    HRESULT hr = backend->text_factory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        weight,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        text_size,
        L"",
        &format);

    float measured_width = 0.0f;
    const wchar_t *text = reinterpret_cast<const wchar_t *>(command->text);
    UINT32 text_length = text != nullptr ? static_cast<UINT32>(wcslen(text)) : 0;

    if (SUCCEEDED(hr) && text_length > 0) {
        (void)format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        IDWriteTextLayout *layout = nullptr;
        hr = backend->text_factory->CreateTextLayout(
            text,
            text_length,
            format,
            command->rect.width,
            command->rect.height,
            &layout);
        if (SUCCEEDED(hr) && layout != nullptr) {
            DWRITE_TEXT_METRICS metrics = {};
            if (SUCCEEDED(layout->GetMetrics(&metrics))) {
                measured_width = metrics.widthIncludingTrailingWhitespace;
            }
            layout->Release();
        }
    }

    ID2D1SolidColorBrush *brush = nullptr;
    if (SUCCEEDED(hr)) {
        hr = target->CreateSolidColorBrush(reach_d2d_color(command->color), &brush);
    }
    if (FAILED(hr) || brush == nullptr) {
        if (format != nullptr) {
            format->Release();
        }
        return REACH_ERROR;
    }

    float caret_x = command->rect.x + measured_width;
    float max_x = command->rect.x + command->rect.width - 2.0f;
    if (caret_x > max_x) {
        caret_x = max_x;
    }
    if (caret_x < command->rect.x) {
        caret_x = command->rect.x;
    }

    D2D1_ROUNDED_RECT caret = {};
    caret.rect = D2D1::RectF(
        caret_x,
        command->rect.y,
        caret_x + 2.0f,
        command->rect.y + command->rect.height);
    caret.radiusX = command->radius;
    caret.radiusY = command->radius;
    target->FillRoundedRectangle(caret, brush);

    brush->Release();
    if (format != nullptr) {
        format->Release();
    }
    return REACH_OK;
}
