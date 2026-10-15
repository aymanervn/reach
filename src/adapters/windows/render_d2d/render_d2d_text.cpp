#include "render_d2d_internal.h"

#include "reach/core/typography.h"

#include <algorithm>

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
    if (backend->use_bundled_font == (use_bundled_font ? 1 : 0))
        return;
    backend->text_layouts.clear();
    backend->text_width_layouts.clear();
    backend->text_formats.clear();
    reach_d2d_clear_retained_layers(backend);
    backend->use_bundled_font = use_bundled_font ? 1 : 0;
    backend->ui_font_family[0] = 0;
}

static HRESULT reach_d2d_cached_layout(reach_render_backend *backend, const wchar_t *text,
                                       float size, int32_t weight, float width, float height,
                                       int32_t alignment, int32_t paragraph, int32_t ellipsis,
                                       IDWriteTextLayout **out)
{
    for (size_t index = 0; index < backend->text_layouts.size(); ++index)
    {
        auto &entry = backend->text_layouts[index];
        if (entry.text == text && entry.size == size && entry.weight == weight &&
            entry.width == width && entry.height == height && entry.alignment == alignment &&
            entry.paragraph == paragraph && entry.ellipsis == ellipsis)
        {
            auto hit = std::move(entry);
            backend->text_layouts.erase(backend->text_layouts.begin() + index);
            backend->text_layouts.push_back(std::move(hit));
            return backend->text_layouts.back().layout.CopyTo(out);
        }
    }
    ComPtr<IDWriteTextFormat> format;
    for (const auto &entry : backend->text_formats)
        if (entry.size == size && entry.weight == weight && entry.alignment == alignment &&
            entry.paragraph == paragraph && entry.ellipsis == ellipsis)
            format = entry.format;
    HRESULT hr = S_OK;
    if (format == nullptr)
    {
        hr = backend->text_factory->CreateTextFormat(
            reach_d2d_ui_font_family(backend), reach_d2d_ui_font_collection(backend),
            (DWRITE_FONT_WEIGHT)weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size,
            L"", &format);
        if (SUCCEEDED(hr))
            hr = format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        if (SUCCEEDED(hr))
            hr = format->SetTextAlignment((DWRITE_TEXT_ALIGNMENT)alignment);
        if (SUCCEEDED(hr))
            hr = format->SetParagraphAlignment((DWRITE_PARAGRAPH_ALIGNMENT)paragraph);
        if (SUCCEEDED(hr) && ellipsis)
        {
            ComPtr<IDWriteInlineObject> sign;
            hr = backend->text_factory->CreateEllipsisTrimmingSign(format.Get(), &sign);
            DWRITE_TRIMMING trimming = {DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
            if (SUCCEEDED(hr))
                hr = format->SetTrimming(&trimming, sign.Get());
        }
        if (SUCCEEDED(hr))
        {
            if (backend->text_formats.size() >= 32)
                backend->text_formats.erase(backend->text_formats.begin());
            backend->text_formats.push_back({size, weight, alignment, paragraph, ellipsis, format});
        }
    }
    ComPtr<IDWriteTextLayout> layout;
    if (SUCCEEDED(hr))
        hr = backend->text_factory->CreateTextLayout(text, (UINT32)wcslen(text), format.Get(),
                                                     width, height, &layout);
    if (SUCCEEDED(hr))
    {
        if (wcslen(text) <= 1024)
        {
            if (backend->text_layouts.size() >= 128)
                backend->text_layouts.erase(backend->text_layouts.begin());
            backend->text_layouts.push_back(
                {text, size, width, height, weight, alignment, paragraph, ellipsis, layout});
        }
        hr = layout.CopyTo(out);
    }
    return hr;
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
    IDWriteTextLayout *layout = nullptr;
    HRESULT hr = reach_d2d_cached_layout(backend, value, text_size, weight, 1000000.0f,
                                         text_size * 4.0f, DWRITE_TEXT_ALIGNMENT_LEADING,
                                         DWRITE_PARAGRAPH_ALIGNMENT_NEAR, 0, &layout);
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
    if (backend != nullptr)
    {
        backend->text_layouts.clear();
        backend->text_width_layouts.clear();
        backend->text_formats.clear();
    }
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

static HRESULT reach_d2d_command_text_layout(reach_render_backend *backend,
                                             const reach_render_command *command,
                                             IDWriteTextLayout **out_layout)
{
    int32_t alignment =
        command->text_alignment == REACH_TEXT_ALIGNMENT_LEADING    ? DWRITE_TEXT_ALIGNMENT_LEADING
        : command->text_alignment == REACH_TEXT_ALIGNMENT_TRAILING ? DWRITE_TEXT_ALIGNMENT_TRAILING
                                                                   : DWRITE_TEXT_ALIGNMENT_CENTER;
    return reach_d2d_cached_layout(
        backend, (const wchar_t *)command->text,
        command->text_size > 0 ? command->text_size : REACH_TEXT_SIZE_MEDIUM,
        command->text_weight > 0 ? command->text_weight : DWRITE_FONT_WEIGHT_NORMAL,
        command->rect.width, command->rect.height, alignment, DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
        command->text_ellipsis, out_layout);
}

reach_result reach_d2d_text_width_layout_for(reach_render_backend *backend,
                                             const reach_render_command *command,
                                             const reach_d2d_text_width_layout **out)
{
    for (size_t index = 0; index < backend->text_width_layouts.size(); ++index)
    {
        const auto &entry = backend->text_width_layouts[index];
        if (entry.text == (const wchar_t *)command->text && entry.size == command->text_size &&
            entry.weight == command->text_weight && entry.width == command->rect.width &&
            entry.height == command->rect.height && entry.alignment == command->text_alignment &&
            entry.ellipsis == command->text_ellipsis)
        {
            auto hit = std::move(backend->text_width_layouts[index]);
            backend->text_width_layouts.erase(backend->text_width_layouts.begin() + index);
            backend->text_width_layouts.push_back(std::move(hit));
            *out = &backend->text_width_layouts.back().layout;
            return REACH_OK;
        }
    }
    ComPtr<IDWriteTextLayout> layout;
    HRESULT hr = reach_d2d_command_text_layout(backend, command, &layout);
    if (FAILED(hr))
        return REACH_ERROR;
    reach_d2d_text_width_entry entry = {};
    entry.text = (const wchar_t *)command->text;
    entry.size = command->text_size;
    entry.weight = command->text_weight;
    entry.width = command->rect.width;
    entry.height = command->rect.height;
    entry.alignment = command->text_alignment;
    entry.ellipsis = command->text_ellipsis;
    if (command->text_ellipsis)
    {
        UINT32 count = 0;
        (void)layout->GetClusterMetrics(nullptr, 0, &count);
        if (count > 260)
            return REACH_ERROR;
        std::vector<DWRITE_CLUSTER_METRICS> clusters(count);
        hr = layout->GetClusterMetrics(clusters.data(), count, &count);
        DWRITE_TRIMMING trimming = {};
        ComPtr<IDWriteInlineObject> sign;
        if (SUCCEEDED(hr))
            hr = layout->GetTrimming(&trimming, &sign);
        DWRITE_INLINE_OBJECT_METRICS metrics = {};
        if (SUCCEEDED(hr) && sign != nullptr)
            hr = sign->GetMetrics(&metrics);
        DWRITE_TEXT_METRICS text = {};
        if (SUCCEEDED(hr))
            hr = layout->GetMetrics(&text);
        if (FAILED(hr))
            return REACH_ERROR;
        float advance = 0;
        auto add = [&](float value)
        {
            if (value > 0 && value < command->rect.width)
                entry.layout.thresholds.push_back(value);
        };
        for (const auto &cluster : clusters)
        {
            advance += cluster.width;
            add(advance + metrics.width);
        }
        add(text.width);
        auto &points = entry.layout.thresholds;
        std::sort(points.begin(), points.end());
        points.erase(std::unique(points.begin(), points.end()), points.end());
    }
    for (size_t index = 0; index <= entry.layout.thresholds.size(); ++index)
    {
        float left = index > 0 ? entry.layout.thresholds[index - 1] : 0;
        float right = index < entry.layout.thresholds.size() ? entry.layout.thresholds[index]
                                                             : command->rect.width;
        entry.layout.widths.push_back(command->text_ellipsis ? (left + right) * .5f : right);
    }
    if (backend->text_width_layouts.size() >= 32)
        backend->text_width_layouts.erase(backend->text_width_layouts.begin());
    backend->text_width_layouts.push_back(std::move(entry));
    *out = &backend->text_width_layouts.back().layout;
    return REACH_OK;
}

reach_result reach_d2d_draw_text_to_target(reach_render_backend *backend, ID2D1RenderTarget *target,
                                           const reach_render_command *command)
{
    if (backend == nullptr || command == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (target == nullptr || backend->text_factory == nullptr)
    {
        return REACH_ERROR;
    }

    IDWriteTextLayout *layout = nullptr;
    ID2D1SolidColorBrush *brush = nullptr;
    HRESULT hr = reach_d2d_command_text_layout(backend, command, &layout);
    if (SUCCEEDED(hr))
    {
        hr = target->CreateSolidColorBrush(reach_d2d_color(command->color), &brush);
    }

    if (FAILED(hr))
    {
        if (layout != nullptr)
            layout->Release();
        return REACH_ERROR;
    }
    target->DrawTextLayout(D2D1::Point2F(command->rect.x, command->rect.y), layout, brush);
    brush->Release();
    layout->Release();

    return REACH_OK;
}

reach_result reach_d2d_draw_text(reach_render_backend *backend, const reach_render_command *command)
{
    return reach_d2d_draw_text_to_target(backend, reach_d2d_target(backend), command);
}

struct reach_d2d_textbox_layout
{
    ComPtr<IDWriteTextLayout> layout;
    float left = 0;
    float top = 0;
    UINT32 text_length = 0;
    bool showing_placeholder = false;
};

static HRESULT reach_d2d_make_textbox_layout(reach_render_backend *backend,
                                             const reach_render_command *command,
                                             reach_d2d_textbox_layout *out)
{
    const wchar_t *text = reinterpret_cast<const wchar_t *>(command->text);
    const wchar_t *placeholder = reinterpret_cast<const wchar_t *>(command->placeholder);
    const UINT32 text_length = static_cast<UINT32>(wcslen(text));
    const int32_t showing_placeholder = (text_length == 0);
    const wchar_t *display = showing_placeholder ? placeholder : text;

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

    DWRITE_TEXT_ALIGNMENT alignment = DWRITE_TEXT_ALIGNMENT_LEADING;
    if (command->text_alignment == REACH_TEXT_ALIGNMENT_CENTER)
    {
        alignment = DWRITE_TEXT_ALIGNMENT_CENTER;
    }
    else if (command->text_alignment == REACH_TEXT_ALIGNMENT_TRAILING)
    {
        alignment = DWRITE_TEXT_ALIGNMENT_TRAILING;
    }
    HRESULT hr = reach_d2d_cached_layout(backend, display, font_size, (int32_t)weight,
                                         content_width, content_height, (int32_t)alignment,
                                         DWRITE_PARAGRAPH_ALIGNMENT_CENTER, 0,
                                         out->layout.ReleaseAndGetAddressOf());
    if (FAILED(hr) || out->layout == nullptr)
        return FAILED(hr) ? hr : E_FAIL;
    out->left = content_left;
    out->top = content_top;
    out->text_length = text_length;
    out->showing_placeholder = showing_placeholder != 0;
    return S_OK;
}

static bool reach_d2d_textbox_caret(const reach_d2d_textbox_layout &text,
                                    const reach_render_command *command, reach_render_command *out)
{
    UINT32 position = text.showing_placeholder
                          ? 0
                          : std::min(text.text_length, (UINT32)std::max(0, command->caret_index));
    FLOAT x = 0, y = 0;
    DWRITE_HIT_TEST_METRICS metrics = {};
    if (FAILED(text.layout->HitTestTextPosition(position, FALSE, &x, &y, &metrics)))
        return false;
    *out = {};
    out->type = REACH_RENDER_COMMAND_LINE;
    out->rect = {text.left + x, text.top + y, 0, metrics.height};
    out->stroke_width = 1.5f;
    out->color = command->text_color;
    return true;
}

reach_result reach_d2d_textbox_caret_line(reach_render_backend *backend,
                                          const reach_render_command *command,
                                          reach_render_command *out)
{
    if (backend == nullptr || backend->text_factory == nullptr || command == nullptr ||
        out == nullptr)
        return REACH_INVALID_ARGUMENT;
    *out = {};
    reach_d2d_textbox_layout text;
    if (FAILED(reach_d2d_make_textbox_layout(backend, command, &text)))
        return REACH_ERROR;
    (void)reach_d2d_textbox_caret(text, command, out);
    return REACH_OK;
}

reach_result reach_d2d_draw_textbox(reach_render_backend *backend,
                                    const reach_render_command *command, float caret_opacity)
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

    reach_d2d_textbox_layout text;
    if (FAILED(reach_d2d_make_textbox_layout(backend, command, &text)))
        return REACH_ERROR;
    IDWriteTextLayout *layout = text.layout.Get();
    bool showing_placeholder = text.showing_placeholder;
    float content_left = text.left;
    float content_top = text.top;

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

    if (command->caret_visible && caret_opacity > 0)
    {
        reach_render_command caret = {};
        if (reach_d2d_textbox_caret(text, command, &caret))
        {
            caret.color.a *= caret_opacity;
            (void)reach_d2d_draw_line(target, &caret);
        }
    }

    return REACH_OK;
}

int32_t reach_d2d_text_raster_phase(reach_render_backend *backend,
                                    const reach_render_command *command, float *out_y)
{
    if (backend == nullptr || command == nullptr || out_y == nullptr || command->text[0] == 0)
        return 0;
    ComPtr<IDWriteTextLayout> layout;
    HRESULT hr = reach_d2d_command_text_layout(backend, command, &layout);
    if (FAILED(hr))
        return 0;
    DWRITE_TEXT_METRICS metrics = {};
    DWRITE_LINE_METRICS line = {};
    UINT32 count = 0;
    if (FAILED(layout->GetMetrics(&metrics)) || FAILED(layout->GetLineMetrics(&line, 1, &count)) ||
        count != 1)
        return 0;
    float baseline = command->rect.y + metrics.top + line.baseline;
    *out_y = baseline - roundf(baseline);
    return 1;
}
