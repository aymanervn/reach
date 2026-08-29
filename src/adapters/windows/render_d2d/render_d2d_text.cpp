#include "render_d2d_internal.h"

#include "reach/core/typography.h"

static const wchar_t reach_d2d_font_fallback[] = L"Segoe UI";
static const wchar_t reach_d2d_bundled_font_family[] = L"JetBrains Mono";
static const wchar_t *reach_d2d_ui_font_family(reach_render_backend *backend);
static const UINT reach_d2d_bundled_font_resources[] = {300, 301, 302, 303};

static int32_t reach_d2d_load_font_resource(UINT resource_id, const void **out_data,
                                            UINT32 *out_size)
{
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resource_id), RT_RCDATA);
    if (resource == nullptr)
    {
        return 0;
    }
    HGLOBAL loaded = LoadResource(module, resource);
    DWORD size = SizeofResource(module, resource);
    const void *data = loaded != nullptr ? LockResource(loaded) : nullptr;
    if (data == nullptr || size == 0)
    {
        return 0;
    }
    *out_data = data;
    *out_size = (UINT32)size;
    return 1;
}

static IDWriteFontCollection *reach_d2d_build_bundled_collection(reach_render_backend *backend)
{
    IDWriteFactory5 *factory5 = nullptr;
    if (backend->text_factory == nullptr ||
        FAILED(backend->text_factory->QueryInterface(__uuidof(IDWriteFactory5),
                                                     reinterpret_cast<void **>(&factory5))) ||
        factory5 == nullptr)
    {
        return nullptr;
    }

    IDWriteInMemoryFontFileLoader *loader = nullptr;
    IDWriteFontSetBuilder1 *builder = nullptr;
    IDWriteFontSet *font_set = nullptr;
    IDWriteFontCollection1 *collection = nullptr;

    HRESULT hr = factory5->CreateInMemoryFontFileLoader(&loader);
    if (SUCCEEDED(hr))
    {
        hr = factory5->RegisterFontFileLoader(loader);
    }
    if (SUCCEEDED(hr))
    {
        hr = factory5->CreateFontSetBuilder(&builder);
    }

    size_t added = 0;
    if (SUCCEEDED(hr))
    {
        for (size_t index = 0; index < sizeof(reach_d2d_bundled_font_resources) / sizeof(UINT);
             ++index)
        {
            const void *data = nullptr;
            UINT32 size = 0;
            if (!reach_d2d_load_font_resource(reach_d2d_bundled_font_resources[index], &data,
                                              &size))
            {
                continue;
            }
            IDWriteFontFile *file = nullptr;
            if (FAILED(loader->CreateInMemoryFontFileReference(factory5, data, size, nullptr,
                                                               &file)) ||
                file == nullptr)
            {
                continue;
            }
            if (SUCCEEDED(builder->AddFontFile(file)))
            {
                added += 1;
            }
            file->Release();
        }
    }

    if (SUCCEEDED(hr) && added == 0)
    {
        hr = E_FAIL;
    }
    if (SUCCEEDED(hr))
    {
        hr = builder->CreateFontSet(&font_set);
    }
    if (SUCCEEDED(hr))
    {
        hr = factory5->CreateFontCollectionFromFontSet(font_set, &collection);
    }

    if (font_set != nullptr)
    {
        font_set->Release();
    }
    if (builder != nullptr)
    {
        builder->Release();
    }
    if (FAILED(hr) || collection == nullptr)
    {
        if (loader != nullptr)
        {
            (void)factory5->UnregisterFontFileLoader(loader);
            loader->Release();
        }
        factory5->Release();
        reach_d2d_log_hresult(L"bundled font collection", hr);
        return nullptr;
    }

    backend->bundled_font_loader = loader;
    factory5->Release();
    return collection;
}

static IDWriteFontCollection *reach_d2d_ui_font_collection(reach_render_backend *backend)
{
    if (backend == nullptr || !backend->use_bundled_font)
    {
        return nullptr;
    }
    if (!backend->bundled_font_attempted)
    {
        backend->bundled_font_attempted = 1;
        backend->bundled_font_collection = reach_d2d_build_bundled_collection(backend);
    }
    return backend->bundled_font_collection;
}

void reach_d2d_set_ui_font(reach_render_backend *backend, int32_t use_bundled_font)
{
    if (backend == nullptr)
    {
        return;
    }
    backend->use_bundled_font = use_bundled_font ? 1 : 0;
    backend->ui_font_family[0] = 0;
}

reach_result reach_d2d_measure_text(void *context, const uint16_t *text, float text_size,
                                    int32_t text_weight, float *out_width)
{
    reach_render_backend *backend = static_cast<reach_render_backend *>(context);
    if (backend == nullptr || text == nullptr || out_width == nullptr || text_size <= 0.0f ||
        backend->text_factory == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_width = 0.0f;
    const wchar_t *value = reinterpret_cast<const wchar_t *>(text);
    const UINT32 length = static_cast<UINT32>(wcslen(value));
    if (length == 0)
    {
        return REACH_OK;
    }

    const DWRITE_FONT_WEIGHT weight =
        text_weight > 0 ? static_cast<DWRITE_FONT_WEIGHT>(text_weight) : DWRITE_FONT_WEIGHT_NORMAL;
    IDWriteTextFormat *format = nullptr;
    HRESULT hr = backend->text_factory->CreateTextFormat(
        reach_d2d_ui_font_family(backend), reach_d2d_ui_font_collection(backend), weight,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, text_size, L"", &format);
    if (FAILED(hr) || format == nullptr)
    {
        return REACH_ERROR;
    }

    (void)format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    IDWriteTextLayout *layout = nullptr;
    hr = backend->text_factory->CreateTextLayout(value, length, format, 1000000.0f,
                                                 text_size * 4.0f, &layout);
    format->Release();
    if (FAILED(hr) || layout == nullptr)
    {
        return REACH_ERROR;
    }

    DWRITE_TEXT_METRICS metrics = {};
    hr = layout->GetMetrics(&metrics);
    layout->Release();
    if (FAILED(hr))
    {
        return REACH_ERROR;
    }

    *out_width = metrics.widthIncludingTrailingWhitespace;
    return REACH_OK;
}

void reach_d2d_release_fonts(reach_render_backend *backend)
{
    if (backend == nullptr)
    {
        return;
    }
    if (backend->bundled_font_collection != nullptr)
    {
        backend->bundled_font_collection->Release();
        backend->bundled_font_collection = nullptr;
    }
    if (backend->bundled_font_loader != nullptr)
    {
        IDWriteFactory5 *factory5 = nullptr;
        if (backend->text_factory != nullptr &&
            SUCCEEDED(backend->text_factory->QueryInterface(
                __uuidof(IDWriteFactory5), reinterpret_cast<void **>(&factory5))) &&
            factory5 != nullptr)
        {
            (void)factory5->UnregisterFontFileLoader(backend->bundled_font_loader);
            factory5->Release();
        }
        backend->bundled_font_loader->Release();
        backend->bundled_font_loader = nullptr;
    }
}

static int32_t reach_d2d_font_family_exists(IDWriteFactory *factory, const wchar_t *family)
{
    if (factory == nullptr || family == nullptr || family[0] == 0)
    {
        return 0;
    }
    IDWriteFontCollection *collection = nullptr;
    if (FAILED(factory->GetSystemFontCollection(&collection, FALSE)) || collection == nullptr)
    {
        return 0;
    }
    UINT32 index = 0;
    BOOL exists = FALSE;
    HRESULT hr = collection->FindFamilyName(family, &index, &exists);
    collection->Release();
    return SUCCEEDED(hr) && exists;
}

static void reach_d2d_query_font_substitute(const wchar_t *family, wchar_t *out, size_t out_count)
{
    out[0] = 0;
    DWORD bytes = (DWORD)(out_count * sizeof(wchar_t));
    if (RegGetValueW(HKEY_LOCAL_MACHINE,
                     L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\FontSubstitutes", family,
                     RRF_RT_REG_SZ, nullptr, out, &bytes) != ERROR_SUCCESS)
    {
        out[0] = 0;
        return;
    }
    wchar_t *charset = wcschr(out, L',');
    if (charset != nullptr)
    {
        *charset = 0;
    }
}

static const wchar_t *reach_d2d_ui_font_family(reach_render_backend *backend)
{
    if (backend == nullptr)
    {
        return reach_d2d_font_fallback;
    }
    if (backend->ui_font_family[0] != 0)
    {
        return backend->ui_font_family;
    }
    if (reach_d2d_ui_font_collection(backend) != nullptr)
    {
        wcsncpy_s(backend->ui_font_family, reach_d2d_bundled_font_family, _TRUNCATE);
        return backend->ui_font_family;
    }

    wchar_t family[LF_FACESIZE] = {};
    NONCLIENTMETRICSW metrics = {};
    metrics.cbSize = sizeof(metrics);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0) &&
        metrics.lfMessageFont.lfFaceName[0] != 0)
    {
        wcsncpy_s(family, metrics.lfMessageFont.lfFaceName, _TRUNCATE);
    }
    else
    {
        wcsncpy_s(family, reach_d2d_font_fallback, _TRUNCATE);
    }

    wchar_t substitute[LF_FACESIZE] = {};
    reach_d2d_query_font_substitute(family, substitute, LF_FACESIZE);
    if (reach_d2d_font_family_exists(backend->text_factory, substitute))
    {
        wcsncpy_s(family, substitute, _TRUNCATE);
    }
    else if (!reach_d2d_font_family_exists(backend->text_factory, family))
    {
        wcsncpy_s(family, reach_d2d_font_fallback, _TRUNCATE);
    }

    wcsncpy_s(backend->ui_font_family, family, _TRUNCATE);
    return backend->ui_font_family;
}

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

    float text_size = command->text_size > 0.0f ? command->text_size : REACH_TEXT_SIZE_MEDIUM;

    HRESULT hr = backend->text_factory->CreateTextFormat(
        reach_d2d_ui_font_family(backend), reach_d2d_ui_font_collection(backend), weight,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, text_size, L"", &format);

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
        else if (command->text_alignment == REACH_TEXT_ALIGNMENT_TRAILING)
        {
            alignment = DWRITE_TEXT_ALIGNMENT_TRAILING;
        }

        (void)format->SetTextAlignment(alignment);
        (void)format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        (void)format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

        if (command->text_ellipsis)
        {
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

    const wchar_t *text = reinterpret_cast<const wchar_t *>(command->text);
    const wchar_t *placeholder = reinterpret_cast<const wchar_t *>(command->placeholder);
    const UINT32 text_length = static_cast<UINT32>(wcslen(text));
    const int32_t showing_placeholder = (text_length == 0);
    const wchar_t *display = showing_placeholder ? placeholder : text;
    const UINT32 display_length = static_cast<UINT32>(wcslen(display));

    const float font_size = command->text_size > 0.0f ? command->text_size : REACH_TEXT_SIZE_MEDIUM;
    const DWRITE_FONT_WEIGHT weight = command->text_weight > 0
                                          ? static_cast<DWRITE_FONT_WEIGHT>(command->text_weight)
                                          : DWRITE_FONT_WEIGHT_NORMAL;

    const float inset = font_size * 0.5f;
    const float content_left = command->rect.x + inset;
    const float content_top = command->rect.y;
    const float content_width =
        command->rect.width - inset * 2.0f > 0.0f ? command->rect.width - inset * 2.0f : 0.0f;
    const float content_height = command->rect.height;

    IDWriteTextFormat *format = nullptr;
    HRESULT hr = backend->text_factory->CreateTextFormat(
        reach_d2d_ui_font_family(backend), reach_d2d_ui_font_collection(backend), weight,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, font_size, L"", &format);
    if (FAILED(hr))
    {
        return REACH_ERROR;
    }
    DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_LEADING;
    if (command->text_alignment == REACH_TEXT_ALIGNMENT_CENTER)
    {
        alignment = DWRITE_TEXT_ALIGNMENT_CENTER;
    }
    else if (command->text_alignment == REACH_TEXT_ALIGNMENT_TRAILING)
    {
        alignment = DWRITE_TEXT_ALIGNMENT_TRAILING;
    }
    (void)format->SetTextAlignment(alignment);
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

    if (!showing_placeholder && command->selection_color.a > 0.0f &&
        command->selection_start != command->selection_end)
    {
        UINT32 sel_begin = static_cast<UINT32>(command->selection_start < command->selection_end
                                                   ? command->selection_start
                                                   : command->selection_end);
        UINT32 sel_count =
            static_cast<UINT32>(command->selection_start < command->selection_end
                                    ? command->selection_end - command->selection_start
                                    : command->selection_start - command->selection_end);

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
                    target->FillRectangle(D2D1::RectF(metrics[i].left, metrics[i].top,
                                                      metrics[i].left + metrics[i].width,
                                                      metrics[i].top + metrics[i].height),
                                          sel);
                }
                sel->Release();
            }
        }
    }

    reach_color text_color = showing_placeholder ? command->placeholder_color : command->text_color;
    ID2D1SolidColorBrush *text_brush = nullptr;
    if (SUCCEEDED(target->CreateSolidColorBrush(reach_d2d_color(text_color), &text_brush)))
    {
        target->DrawTextLayout(D2D1::Point2F(content_left, content_top), layout, text_brush,
                               D2D1_DRAW_TEXT_OPTIONS_CLIP);
        text_brush->Release();
    }

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
            if (SUCCEEDED(
                    target->CreateSolidColorBrush(reach_d2d_color(caret_color), &caret_brush)))
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
