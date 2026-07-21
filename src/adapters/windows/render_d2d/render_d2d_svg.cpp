#include "reach/core/geometry.h"
#include "render_d2d_internal.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

static int reach_d2d_vector_icon_resource(uint64_t icon_id)
{
    switch (icon_id)
    {
    case REACH_VECTOR_ICON_LOCK:
        return 201;
    case REACH_VECTOR_ICON_SLEEP:
        return 202;
    case REACH_VECTOR_ICON_RESTART:
        return 203;
    case REACH_VECTOR_ICON_POWER:
    case REACH_VECTOR_ICON_SHUTDOWN:
        return 204;
    case REACH_VECTOR_ICON_SIGN_OUT:
        return 205;
    case REACH_VECTOR_ICON_ARROW_UP:
        return 206;
    case REACH_VECTOR_ICON_QUICK_SETTINGS:
        return 207;
    case REACH_VECTOR_ICON_VOLUME_ZERO:
        return 208;
    case REACH_VECTOR_ICON_VOLUME_LOW:
        return 209;
    case REACH_VECTOR_ICON_VOLUME_HIGH:
        return 210;
    case REACH_VECTOR_ICON_ARROW_DOWN:
        return 211;
    case REACH_VECTOR_ICON_CHECK:
        return 212;
    case REACH_VECTOR_ICON_ETHERNET:
        return 213;
    case REACH_VECTOR_ICON_WIFI_LOW:
        return 214;
    case REACH_VECTOR_ICON_WIFI_MEDIUM:
        return 215;
    case REACH_VECTOR_ICON_WIFI_HIGH:
        return 216;
    case REACH_VECTOR_ICON_BATTERY_SAVER:
        return 218;
    case REACH_VECTOR_ICON_PROJECT:
        return 219;
    case REACH_VECTOR_ICON_BRIGHTNESS:
        return 220;
    case REACH_VECTOR_ICON_BLUETOOTH_ON:
        return 221;
    case REACH_VECTOR_ICON_BLUETOOTH_OFF:
        return 222;
    case REACH_VECTOR_ICON_NO_INTERNET:
        return 223;
    case REACH_VECTOR_ICON_FOLDER:
        return 224;
    case REACH_VECTOR_ICON_PHOTO:
        return 225;
    case REACH_VECTOR_ICON_VIDEO:
        return 226;
    case REACH_VECTOR_ICON_MUSIC:
        return 227;
    case REACH_VECTOR_ICON_DOCUMENT:
        return 228;
    case REACH_VECTOR_ICON_FILE:
        return 229;
    case REACH_VECTOR_ICON_PLAY:
        return 230;
    case REACH_VECTOR_ICON_PAUSE:
        return 231;
    case REACH_VECTOR_ICON_PREVIOUS:
        return 232;
    case REACH_VECTOR_ICON_NEXT:
        return 233;
    case REACH_VECTOR_ICON_MINIMIZE:
        return 234;
    case REACH_VECTOR_ICON_CLOSE:
        return 235;
    case REACH_VECTOR_ICON_RESIZE:
        return 236;
    case REACH_VECTOR_ICON_SETTINGS:
        return 237;
    case REACH_VECTOR_ICON_SEARCH:
        return 238;
    case REACH_VECTOR_ICON_EXECUTABLE:
        return 239;
    default:
        return 0;
    }
}

static reach_result reach_d2d_load_svg_resource_text(int resource_id, std::string *out_svg)
{
    if (resource_id == 0 || out_svg == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resource_id), RT_RCDATA);

    if (resource == nullptr)
    {
        return REACH_ERROR;
    }

    HGLOBAL loaded = LoadResource(module, resource);
    DWORD size = SizeofResource(module, resource);
    const char *data =
        loaded != nullptr ? static_cast<const char *>(LockResource(loaded)) : nullptr;

    if (data == nullptr || size == 0)
    {
        return REACH_ERROR;
    }

    out_svg->assign(data, data + size);
    return REACH_OK;
}

static int reach_svg_is_separator(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',';
}

static int reach_svg_is_space_only(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int reach_svg_is_command(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static const char *reach_svg_skip_separators(const char *cursor)
{
    while (cursor != nullptr && reach_svg_is_separator(*cursor))
    {
        ++cursor;
    }

    return cursor;
}

static int reach_svg_read_float(const char **cursor, float *out_value)
{
    if (cursor == nullptr || *cursor == nullptr || out_value == nullptr)
    {
        return 0;
    }

    const char *start = reach_svg_skip_separators(*cursor);
    if (start == nullptr || *start == 0 || reach_svg_is_command(*start))
    {
        return 0;
    }

    char *end = nullptr;
    double value = strtod(start, &end);

    if (end == start)
    {
        return 0;
    }

    *out_value = static_cast<float>(value);
    *cursor = end;
    return 1;
}

static int reach_svg_read_point(const char **cursor, float *out_x, float *out_y)
{
    return reach_svg_read_float(cursor, out_x) && reach_svg_read_float(cursor, out_y);
}

static int reach_svg_attribute_name_boundary_before(const std::string &tag, size_t pos)
{
    if (pos == 0)
    {
        return 1;
    }

    char c = tag[pos - 1];
    return reach_svg_is_space_only(c) || c == '<' || c == '/';
}

static int reach_svg_attribute_name_boundary_after(const std::string &tag, size_t pos)
{
    if (pos >= tag.size())
    {
        return 0;
    }

    char c = tag[pos];
    return reach_svg_is_space_only(c) || c == '=';
}

static int reach_svg_read_attribute(const std::string &tag, const char *name,
                                    std::string *out_value)
{
    if (name == nullptr || out_value == nullptr)
    {
        return 0;
    }

    size_t name_length = strlen(name);
    size_t pos = 0;

    while ((pos = tag.find(name, pos)) != std::string::npos)
    {
        size_t after_name = pos + name_length;

        if (!reach_svg_attribute_name_boundary_before(tag, pos) ||
            !reach_svg_attribute_name_boundary_after(tag, after_name))
        {
            pos = after_name;
            continue;
        }

        size_t cursor = after_name;
        while (cursor < tag.size() && reach_svg_is_space_only(tag[cursor]))
        {
            ++cursor;
        }

        if (cursor >= tag.size() || tag[cursor] != '=')
        {
            pos = after_name;
            continue;
        }

        ++cursor;
        while (cursor < tag.size() && reach_svg_is_space_only(tag[cursor]))
        {
            ++cursor;
        }

        if (cursor >= tag.size() || (tag[cursor] != '"' && tag[cursor] != '\''))
        {
            return 0;
        }

        char quote = tag[cursor++];
        size_t end = tag.find(quote, cursor);

        if (end == std::string::npos)
        {
            return 0;
        }

        *out_value = tag.substr(cursor, end - cursor);
        return 1;
    }

    return 0;
}

static int reach_svg_read_root_tag(const std::string &svg, std::string *out_tag)
{
    if (out_tag == nullptr)
    {
        return 0;
    }

    size_t start = svg.find("<svg");
    if (start == std::string::npos)
    {
        return 0;
    }

    size_t end = svg.find('>', start);
    if (end == std::string::npos)
    {
        return 0;
    }

    *out_tag = svg.substr(start, end - start + 1);
    return 1;
}

static int reach_svg_view_box(const std::string &svg, float *out_x, float *out_y, float *out_width,
                              float *out_height)
{
    if (out_x == nullptr || out_y == nullptr || out_width == nullptr || out_height == nullptr)
    {
        return 0;
    }

    std::string root_tag;
    std::string view_box;

    if (!reach_svg_read_root_tag(svg, &root_tag) ||
        !reach_svg_read_attribute(root_tag, "viewBox", &view_box))
    {
        return 0;
    }

    const char *cursor = view_box.c_str();

    return reach_svg_read_float(&cursor, out_x) && reach_svg_read_float(&cursor, out_y) &&
           reach_svg_read_float(&cursor, out_width) && reach_svg_read_float(&cursor, out_height);
}

static float reach_svg_attribute_float(const std::string &tag, const char *name, float fallback)
{
    std::string value;
    if (!reach_svg_read_attribute(tag, name, &value))
    {
        return fallback;
    }

    const char *cursor = value.c_str();
    float parsed = fallback;
    return reach_svg_read_float(&cursor, &parsed) ? parsed : fallback;
}

static int reach_svg_ascii_equal(const std::string &a, const char *b)
{
    if (b == nullptr || a.size() != strlen(b))
    {
        return 0;
    }

    for (size_t i = 0; i < a.size(); ++i)
    {
        char ca = a[i];
        char cb = b[i];

        if (ca >= 'A' && ca <= 'Z')
        {
            ca = static_cast<char>(ca - 'A' + 'a');
        }

        if (cb >= 'A' && cb <= 'Z')
        {
            cb = static_cast<char>(cb - 'A' + 'a');
        }

        if (ca != cb)
        {
            return 0;
        }
    }

    return 1;
}

struct reach_svg_style
{
    int fill;
    int stroke;
    float stroke_width;
    D2D1_CAP_STYLE line_cap;
    D2D1_LINE_JOIN line_join;
    float miter_limit;
    D2D1_FILL_MODE fill_mode;
};

static D2D1_CAP_STYLE reach_svg_parse_line_cap(const std::string &value, D2D1_CAP_STYLE fallback)
{
    if (reach_svg_ascii_equal(value, "round"))
    {
        return D2D1_CAP_STYLE_ROUND;
    }

    if (reach_svg_ascii_equal(value, "square"))
    {
        return D2D1_CAP_STYLE_SQUARE;
    }

    if (reach_svg_ascii_equal(value, "butt"))
    {
        return D2D1_CAP_STYLE_FLAT;
    }

    return fallback;
}

static D2D1_LINE_JOIN reach_svg_parse_line_join(const std::string &value, D2D1_LINE_JOIN fallback)
{
    if (reach_svg_ascii_equal(value, "round"))
    {
        return D2D1_LINE_JOIN_ROUND;
    }

    if (reach_svg_ascii_equal(value, "bevel"))
    {
        return D2D1_LINE_JOIN_BEVEL;
    }

    if (reach_svg_ascii_equal(value, "miter") || reach_svg_ascii_equal(value, "miter-clip"))
    {
        return D2D1_LINE_JOIN_MITER;
    }

    return fallback;
}

static D2D1_FILL_MODE reach_svg_parse_fill_rule(const std::string &value, D2D1_FILL_MODE fallback)
{
    if (reach_svg_ascii_equal(value, "evenodd"))
    {
        return D2D1_FILL_MODE_ALTERNATE;
    }

    if (reach_svg_ascii_equal(value, "nonzero"))
    {
        return D2D1_FILL_MODE_WINDING;
    }

    return fallback;
}

static int reach_svg_parse_paint_active(const std::string &value, int fallback)
{
    if (reach_svg_ascii_equal(value, "none"))
    {
        return 0;
    }

    if (value.empty())
    {
        return fallback;
    }

    return 1;
}

static reach_svg_style reach_svg_default_style(void)
{
    reach_svg_style style = {};
    style.fill = 1;
    style.stroke = 0;
    style.stroke_width = 1.0f;
    style.line_cap = D2D1_CAP_STYLE_FLAT;
    style.line_join = D2D1_LINE_JOIN_MITER;
    style.miter_limit = 4.0f;
    style.fill_mode = D2D1_FILL_MODE_WINDING;
    return style;
}

static void reach_svg_apply_style_attributes(const std::string &tag, reach_svg_style *style)
{
    if (style == nullptr)
    {
        return;
    }

    std::string value;

    if (reach_svg_read_attribute(tag, "fill", &value))
    {
        style->fill = reach_svg_parse_paint_active(value, style->fill);
    }

    if (reach_svg_read_attribute(tag, "stroke", &value))
    {
        style->stroke = reach_svg_parse_paint_active(value, style->stroke);
    }

    if (reach_svg_read_attribute(tag, "stroke-width", &value))
    {
        const char *cursor = value.c_str();
        float parsed = style->stroke_width;
        if (reach_svg_read_float(&cursor, &parsed))
        {
            style->stroke_width = parsed;
        }
    }

    if (reach_svg_read_attribute(tag, "stroke-linecap", &value))
    {
        style->line_cap = reach_svg_parse_line_cap(value, style->line_cap);
    }

    if (reach_svg_read_attribute(tag, "stroke-linejoin", &value))
    {
        style->line_join = reach_svg_parse_line_join(value, style->line_join);
    }

    if (reach_svg_read_attribute(tag, "stroke-miterlimit", &value))
    {
        const char *cursor = value.c_str();
        float parsed = style->miter_limit;
        if (reach_svg_read_float(&cursor, &parsed))
        {
            style->miter_limit = parsed;
        }
    }

    if (reach_svg_read_attribute(tag, "fill-rule", &value))
    {
        style->fill_mode = reach_svg_parse_fill_rule(value, style->fill_mode);
    }
}

static reach_svg_style reach_svg_root_style(const std::string &svg)
{
    reach_svg_style style = reach_svg_default_style();

    std::string root_tag;
    if (reach_svg_read_root_tag(svg, &root_tag))
    {
        reach_svg_apply_style_attributes(root_tag, &style);
    }

    return style;
}

static void reach_svg_end_figure(ID2D1GeometrySink *sink, int *figure_open, D2D1_FIGURE_END end)
{
    if (sink != nullptr && figure_open != nullptr && *figure_open)
    {
        sink->EndFigure(end);
        *figure_open = 0;
    }
}

static reach_result reach_svg_path_to_geometry(ID2D1Factory1 *factory, const std::string &path,
                                               D2D1_FILL_MODE fill_mode,
                                               ID2D1PathGeometry **out_geometry)
{
    if (factory == nullptr || path.empty() || out_geometry == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_geometry = nullptr;

    ID2D1PathGeometry *geometry = nullptr;
    HRESULT hr = factory->CreatePathGeometry(&geometry);

    if (FAILED(hr) || geometry == nullptr)
    {
        return REACH_ERROR;
    }

    ID2D1GeometrySink *sink = nullptr;
    hr = geometry->Open(&sink);

    if (FAILED(hr) || sink == nullptr)
    {
        geometry->Release();
        return REACH_ERROR;
    }

    sink->SetFillMode(fill_mode);

    const char *cursor = path.c_str();
    char command = 0;

    float current_x = 0.0f;
    float current_y = 0.0f;
    float start_x = 0.0f;
    float start_y = 0.0f;
    float previous_cubic_x = 0.0f;
    float previous_cubic_y = 0.0f;

    int previous_was_cubic = 0;
    int figure_open = 0;

    while (cursor != nullptr && *cursor != 0)
    {
        cursor = reach_svg_skip_separators(cursor);

        if (*cursor == 0)
        {
            break;
        }

        if (reach_svg_is_command(*cursor))
        {
            command = *cursor;
            ++cursor;
        }

        if (command == 0)
        {
            break;
        }

        if (command == 'M' || command == 'm')
        {
            float x = 0.0f;
            float y = 0.0f;

            if (!reach_svg_read_point(&cursor, &x, &y))
            {
                break;
            }

            if (command == 'm')
            {
                x += current_x;
                y += current_y;
            }

            reach_svg_end_figure(sink, &figure_open, D2D1_FIGURE_END_OPEN);
            sink->BeginFigure(D2D1::Point2F(x, y), D2D1_FIGURE_BEGIN_FILLED);

            figure_open = 1;
            current_x = start_x = x;
            current_y = start_y = y;
            previous_was_cubic = 0;
            command = command == 'm' ? 'l' : 'L';
            continue;
        }

        if (command == 'L' || command == 'l')
        {
            float x = 0.0f;
            float y = 0.0f;

            if (!reach_svg_read_point(&cursor, &x, &y))
            {
                break;
            }

            if (command == 'l')
            {
                x += current_x;
                y += current_y;
            }

            if (figure_open)
            {
                sink->AddLine(D2D1::Point2F(x, y));
            }

            current_x = x;
            current_y = y;
            previous_was_cubic = 0;
            continue;
        }

        if (command == 'H' || command == 'h')
        {
            float x = 0.0f;

            if (!reach_svg_read_float(&cursor, &x))
            {
                break;
            }

            if (command == 'h')
            {
                x += current_x;
            }

            if (figure_open)
            {
                sink->AddLine(D2D1::Point2F(x, current_y));
            }

            current_x = x;
            previous_was_cubic = 0;
            continue;
        }

        if (command == 'V' || command == 'v')
        {
            float y = 0.0f;

            if (!reach_svg_read_float(&cursor, &y))
            {
                break;
            }

            if (command == 'v')
            {
                y += current_y;
            }

            if (figure_open)
            {
                sink->AddLine(D2D1::Point2F(current_x, y));
            }

            current_y = y;
            previous_was_cubic = 0;
            continue;
        }

        if (command == 'C' || command == 'c')
        {
            float x1 = 0.0f;
            float y1 = 0.0f;
            float x2 = 0.0f;
            float y2 = 0.0f;
            float x = 0.0f;
            float y = 0.0f;

            if (!reach_svg_read_point(&cursor, &x1, &y1) ||
                !reach_svg_read_point(&cursor, &x2, &y2) || !reach_svg_read_point(&cursor, &x, &y))
            {
                break;
            }

            if (command == 'c')
            {
                x1 += current_x;
                y1 += current_y;
                x2 += current_x;
                y2 += current_y;
                x += current_x;
                y += current_y;
            }

            if (figure_open)
            {
                D2D1_BEZIER_SEGMENT segment = {};
                segment.point1 = D2D1::Point2F(x1, y1);
                segment.point2 = D2D1::Point2F(x2, y2);
                segment.point3 = D2D1::Point2F(x, y);
                sink->AddBezier(segment);
            }

            current_x = x;
            current_y = y;
            previous_cubic_x = x2;
            previous_cubic_y = y2;
            previous_was_cubic = 1;
            continue;
        }

        if (command == 'S' || command == 's')
        {
            float x1 = previous_was_cubic ? current_x * 2.0f - previous_cubic_x : current_x;
            float y1 = previous_was_cubic ? current_y * 2.0f - previous_cubic_y : current_y;

            float x2 = 0.0f;
            float y2 = 0.0f;
            float x = 0.0f;
            float y = 0.0f;

            if (!reach_svg_read_point(&cursor, &x2, &y2) || !reach_svg_read_point(&cursor, &x, &y))
            {
                break;
            }

            if (command == 's')
            {
                x2 += current_x;
                y2 += current_y;
                x += current_x;
                y += current_y;
            }

            if (figure_open)
            {
                D2D1_BEZIER_SEGMENT segment = {};
                segment.point1 = D2D1::Point2F(x1, y1);
                segment.point2 = D2D1::Point2F(x2, y2);
                segment.point3 = D2D1::Point2F(x, y);
                sink->AddBezier(segment);
            }

            current_x = x;
            current_y = y;
            previous_cubic_x = x2;
            previous_cubic_y = y2;
            previous_was_cubic = 1;
            continue;
        }

        if (command == 'A' || command == 'a')
        {
            float radius_x = 0.0f;
            float radius_y = 0.0f;
            float rotation = 0.0f;
            float large_arc = 0.0f;
            float sweep = 0.0f;
            float x = 0.0f;
            float y = 0.0f;

            if (!reach_svg_read_float(&cursor, &radius_x) ||
                !reach_svg_read_float(&cursor, &radius_y) ||
                !reach_svg_read_float(&cursor, &rotation) ||
                !reach_svg_read_float(&cursor, &large_arc) ||
                !reach_svg_read_float(&cursor, &sweep) || !reach_svg_read_point(&cursor, &x, &y))
            {
                break;
            }

            if (command == 'a')
            {
                x += current_x;
                y += current_y;
            }

            if (figure_open)
            {
                D2D1_ARC_SEGMENT segment = {};
                segment.point = D2D1::Point2F(x, y);
                segment.size = D2D1::SizeF(std::fabs(radius_x), std::fabs(radius_y));
                segment.rotationAngle = rotation;
                segment.sweepDirection = sweep != 0.0f ? D2D1_SWEEP_DIRECTION_CLOCKWISE
                                                       : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
                segment.arcSize = large_arc != 0.0f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL;
                sink->AddArc(segment);
            }

            current_x = x;
            current_y = y;
            previous_was_cubic = 0;
            continue;
        }

        if (command == 'Z' || command == 'z')
        {
            reach_svg_end_figure(sink, &figure_open, D2D1_FIGURE_END_CLOSED);
            current_x = start_x;
            current_y = start_y;
            previous_was_cubic = 0;
            command = 0;
            continue;
        }

        reach_svg_end_figure(sink, &figure_open, D2D1_FIGURE_END_OPEN);
        sink->Close();
        sink->Release();
        geometry->Release();
        return REACH_ERROR;
    }

    reach_svg_end_figure(sink, &figure_open, D2D1_FIGURE_END_OPEN);

    hr = sink->Close();
    sink->Release();

    if (FAILED(hr))
    {
        geometry->Release();
        return REACH_ERROR;
    }

    *out_geometry = geometry;
    return REACH_OK;
}

static int reach_svg_element_in_ignored_section(const std::string &svg, size_t tag_start)
{
    size_t defs_open = svg.rfind("<defs", tag_start);
    size_t defs_close = svg.rfind("</defs>", tag_start);

    if (defs_open != std::string::npos &&
        (defs_close == std::string::npos || defs_close < defs_open))
    {
        return 1;
    }

    size_t clip_open = svg.rfind("<clipPath", tag_start);
    size_t clip_close = svg.rfind("</clipPath>", tag_start);

    return clip_open != std::string::npos &&
           (clip_close == std::string::npos || clip_close < clip_open);
}

static D2D1_MATRIX_3X2_F reach_svg_view_box_to_rect_transform(float view_x, float view_y,
                                                              float view_width, float view_height,
                                                              const reach_rect_f32 &rect)
{
    float scale_x = rect.width / view_width;
    float scale_y = rect.height / view_height;
    float scale = scale_x < scale_y ? scale_x : scale_y;

    float width = view_width * scale;
    float height = view_height * scale;
    float offset_x = rect.x + (rect.width - width) * 0.5f;
    float offset_y = rect.y + (rect.height - height) * 0.5f;

    return D2D1::Matrix3x2F::Scale(scale, scale) *
           D2D1::Matrix3x2F::Translation(offset_x - view_x * scale, offset_y - view_y * scale);
}

static reach_result reach_d2d_create_stroke_style(ID2D1Factory1 *factory,
                                                  const reach_svg_style &style,
                                                  ID2D1StrokeStyle **out_stroke_style)
{
    if (factory == nullptr || out_stroke_style == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_stroke_style = nullptr;

    D2D1_STROKE_STYLE_PROPERTIES properties = {};
    properties.startCap = style.line_cap;
    properties.endCap = style.line_cap;
    properties.dashCap = style.line_cap;
    properties.lineJoin = style.line_join;
    properties.miterLimit = style.miter_limit;
    properties.dashStyle = D2D1_DASH_STYLE_SOLID;
    properties.dashOffset = 0.0f;

    HRESULT hr = factory->CreateStrokeStyle(&properties, nullptr, 0, out_stroke_style);

    return SUCCEEDED(hr) && *out_stroke_style != nullptr ? REACH_OK : REACH_ERROR;
}

static void reach_d2d_draw_svg_geometry(ID2D1RenderTarget *target, ID2D1Geometry *geometry,
                                        ID2D1SolidColorBrush *brush, ID2D1StrokeStyle *stroke_style,
                                        const reach_svg_style &style, float stroke_scale)
{
    if (target == nullptr || geometry == nullptr || brush == nullptr)
    {
        return;
    }

    if (style.fill)
    {
        target->FillGeometry(geometry, brush);
    }

    if (style.stroke && style.stroke_width > 0.0f)
    {
        target->DrawGeometry(geometry, brush, style.stroke_width * stroke_scale, stroke_style);
    }
}

static reach_result reach_d2d_draw_svg_path_tag(reach_render_backend *backend,
                                                ID2D1RenderTarget *target,
                                                ID2D1SolidColorBrush *brush, const std::string &tag,
                                                const reach_svg_style &root_style,
                                                const D2D1_MATRIX_3X2_F &transform,
                                                float stroke_scale)
{
    std::string path_data;
    if (!reach_svg_read_attribute(tag, "d", &path_data))
    {
        return REACH_OK;
    }

    reach_svg_style style = root_style;
    reach_svg_apply_style_attributes(tag, &style);

    if (!style.fill && !style.stroke)
    {
        return REACH_OK;
    }

    ID2D1PathGeometry *geometry = nullptr;
    reach_result result =
        reach_svg_path_to_geometry(backend->factory, path_data, style.fill_mode, &geometry);

    if (result != REACH_OK || geometry == nullptr)
    {
        return result;
    }

    ID2D1TransformedGeometry *transformed = nullptr;
    HRESULT hr = backend->factory->CreateTransformedGeometry(geometry, transform, &transformed);
    geometry->Release();

    if (FAILED(hr) || transformed == nullptr)
    {
        return REACH_ERROR;
    }

    ID2D1StrokeStyle *stroke_style = nullptr;
    if (style.stroke)
    {
        result = reach_d2d_create_stroke_style(backend->factory, style, &stroke_style);
        if (result != REACH_OK)
        {
            transformed->Release();
            return result;
        }
    }

    reach_d2d_draw_svg_geometry(target, transformed, brush, stroke_style, style, stroke_scale);

    if (stroke_style != nullptr)
    {
        stroke_style->Release();
    }

    transformed->Release();
    return REACH_OK;
}

static reach_result
reach_d2d_draw_svg_circle_tag(reach_render_backend *backend, ID2D1RenderTarget *target,
                              ID2D1SolidColorBrush *brush, const std::string &tag,
                              const reach_svg_style &root_style, const D2D1_MATRIX_3X2_F &transform,
                              float stroke_scale)
{
    reach_svg_style style = root_style;
    reach_svg_apply_style_attributes(tag, &style);

    if (!style.fill && !style.stroke)
    {
        return REACH_OK;
    }

    float cx = reach_svg_attribute_float(tag, "cx", 0.0f);
    float cy = reach_svg_attribute_float(tag, "cy", 0.0f);
    float r = reach_svg_attribute_float(tag, "r", 0.0f);

    if (r <= 0.0f)
    {
        return REACH_OK;
    }

    ID2D1EllipseGeometry *geometry = nullptr;
    HRESULT hr = backend->factory->CreateEllipseGeometry(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r),
                                                         &geometry);

    if (FAILED(hr) || geometry == nullptr)
    {
        return REACH_ERROR;
    }

    ID2D1TransformedGeometry *transformed = nullptr;
    hr = backend->factory->CreateTransformedGeometry(geometry, transform, &transformed);
    geometry->Release();

    if (FAILED(hr) || transformed == nullptr)
    {
        return REACH_ERROR;
    }

    ID2D1StrokeStyle *stroke_style = nullptr;
    reach_result result = REACH_OK;

    if (style.stroke)
    {
        result = reach_d2d_create_stroke_style(backend->factory, style, &stroke_style);
        if (result != REACH_OK)
        {
            transformed->Release();
            return result;
        }
    }

    reach_d2d_draw_svg_geometry(target, transformed, brush, stroke_style, style, stroke_scale);

    if (stroke_style != nullptr)
    {
        stroke_style->Release();
    }

    transformed->Release();
    return REACH_OK;
}

static reach_result reach_d2d_draw_svg_resource(reach_render_backend *backend,
                                                const reach_render_command *command,
                                                const std::string &svg)
{
    if (backend == nullptr || backend->factory == nullptr || command == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (target == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    float view_x = 0.0f;
    float view_y = 0.0f;
    float view_width = 0.0f;
    float view_height = 0.0f;

    if (!reach_svg_view_box(svg, &view_x, &view_y, &view_width, &view_height) ||
        view_width <= 0.0f || view_height <= 0.0f || command->rect.width <= 0.0f ||
        command->rect.height <= 0.0f)
    {
        return REACH_ERROR;
    }

    D2D1_MATRIX_3X2_F transform = reach_svg_view_box_to_rect_transform(view_x, view_y, view_width,
                                                                       view_height, command->rect);

    float stroke_scale = command->rect.width / view_width;
    float stroke_scale_y = command->rect.height / view_height;
    if (stroke_scale_y < stroke_scale)
    {
        stroke_scale = stroke_scale_y;
    }

    ID2D1SolidColorBrush *brush = nullptr;
    HRESULT hr = target->CreateSolidColorBrush(reach_d2d_color(command->color), &brush);

    if (FAILED(hr) || brush == nullptr)
    {
        return REACH_ERROR;
    }

    reach_svg_style root_style = reach_svg_root_style(svg);

    reach_result result = REACH_OK;
    size_t offset = 0;

    while (result == REACH_OK)
    {
        size_t path_start = svg.find("<path", offset);
        size_t circle_start = svg.find("<circle", offset);

        size_t tag_start = std::string::npos;
        const char *kind = nullptr;

        if (path_start != std::string::npos &&
            (circle_start == std::string::npos || path_start < circle_start))
        {
            tag_start = path_start;
            kind = "path";
        }
        else if (circle_start != std::string::npos)
        {
            tag_start = circle_start;
            kind = "circle";
        }
        else
        {
            break;
        }

        if (reach_svg_element_in_ignored_section(svg, tag_start))
        {
            offset = tag_start + 1;
            continue;
        }

        size_t tag_end = svg.find('>', tag_start);
        if (tag_end == std::string::npos)
        {
            result = REACH_ERROR;
            break;
        }

        std::string tag = svg.substr(tag_start, tag_end - tag_start + 1);
        offset = tag_end + 1;

        if (strcmp(kind, "path") == 0)
        {
            result = reach_d2d_draw_svg_path_tag(backend, target, brush, tag, root_style, transform,
                                                 stroke_scale);
        }
        else
        {
            result = reach_d2d_draw_svg_circle_tag(backend, target, brush, tag, root_style,
                                                   transform, stroke_scale);
        }
    }

    brush->Release();
    return result;
}

reach_result reach_d2d_draw_vector_icon(reach_render_backend *backend,
                                        const reach_render_command *command)
{
    if (backend == nullptr || command == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    int resource_id = reach_d2d_vector_icon_resource(command->icon_id);

    if (resource_id == 0)
    {
        return REACH_OK;
    }

    std::string svg_text;
    reach_result load_result = reach_d2d_load_svg_resource_text(resource_id, &svg_text);

    if (load_result != REACH_OK)
    {
        return load_result;
    }

    return reach_d2d_draw_svg_resource(backend, command, svg_text);
}
