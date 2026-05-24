#include "render_d2d_internal.h"

static int reach_d2d_vector_icon_resource(uint64_t icon_id)
{
    switch (icon_id) {
    case REACH_VECTOR_ICON_LOCK: return 201;
    case REACH_VECTOR_ICON_SLEEP: return 202;
    case REACH_VECTOR_ICON_RESTART: return 203;
    case REACH_VECTOR_ICON_POWER:
    case REACH_VECTOR_ICON_SHUTDOWN: return 204;
    case REACH_VECTOR_ICON_SIGN_OUT: return 205;
    case REACH_VECTOR_ICON_ARROW_UP: return 206;
    case REACH_VECTOR_ICON_SETTINGS: return 207;
    case REACH_VECTOR_ICON_VOLUME_ZERO: return 208;
    case REACH_VECTOR_ICON_VOLUME_LOW: return 209;
    case REACH_VECTOR_ICON_VOLUME_HIGH: return 210;
    case REACH_VECTOR_ICON_ARROW_DOWN: return 211;
    case REACH_VECTOR_ICON_CHECK: return 212;
    case REACH_VECTOR_ICON_ETHERNET: return 213;
    case REACH_VECTOR_ICON_WIFI_LOW: return 214;
    case REACH_VECTOR_ICON_WIFI_MEDIUM: return 215;
    case REACH_VECTOR_ICON_WIFI_HIGH: return 216;
    case REACH_VECTOR_ICON_BATTERY_SAVER: return 218;
    case REACH_VECTOR_ICON_PROJECT: return 219;
    case REACH_VECTOR_ICON_BRIGHTNESS: return 220;
    case REACH_VECTOR_ICON_BLUETOOTH_ON: return 221;
    case REACH_VECTOR_ICON_BLUETOOTH_OFF: return 222;
    case REACH_VECTOR_ICON_NO_INTERNET: return 223;
    default: return 0;
    }
}

static char reach_d2d_hex_digit(int value)
{
    return static_cast<char>(value < 10 ? ('0' + value) : ('A' + value - 10));
}

static std::string reach_d2d_color_hex(reach_color color)
{
    int r = static_cast<int>(color.r * 255.0f + 0.5f);
    int g = static_cast<int>(color.g * 255.0f + 0.5f);
    int b = static_cast<int>(color.b * 255.0f + 0.5f);

    if (r < 0) {
        r = 0;
    } else if (r > 255) {
        r = 255;
    }

    if (g < 0) {
        g = 0;
    } else if (g > 255) {
        g = 255;
    }

    if (b < 0) {
        b = 0;
    } else if (b > 255) {
        b = 255;
    }

    std::string hex = "#000000";
    hex[1] = reach_d2d_hex_digit((r >> 4) & 0x0F);
    hex[2] = reach_d2d_hex_digit(r & 0x0F);
    hex[3] = reach_d2d_hex_digit((g >> 4) & 0x0F);
    hex[4] = reach_d2d_hex_digit(g & 0x0F);
    hex[5] = reach_d2d_hex_digit((b >> 4) & 0x0F);
    hex[6] = reach_d2d_hex_digit(b & 0x0F);

    return hex;
}

static void reach_d2d_replace_all(
    std::string *text,
    const char *needle,
    const std::string &replacement
)
{
    if (text == nullptr || needle == nullptr || needle[0] == 0) {
        return;
    }

    size_t offset = 0;
    size_t needle_length = strlen(needle);

    while ((offset = text->find(needle, offset)) != std::string::npos) {
        text->replace(offset, needle_length, replacement);
        offset += replacement.size();
    }
}

static reach_result reach_d2d_load_svg_resource_text(
    int resource_id,
    reach_color color,
    std::string *out_svg
)
{
    if (resource_id == 0 || out_svg == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(resource_id), RT_RCDATA);

    if (resource == nullptr) {
        return REACH_ERROR;
    }

    HGLOBAL loaded = LoadResource(module, resource);
    DWORD size = SizeofResource(module, resource);
    const char *data = loaded != nullptr
        ? static_cast<const char *>(LockResource(loaded))
        : nullptr;

    if (data == nullptr || size == 0) {
        return REACH_ERROR;
    }

    out_svg->assign(data, data + size);

    std::string hex = reach_d2d_color_hex(color);
    reach_d2d_replace_all(out_svg, "#000000", hex);
    reach_d2d_replace_all(out_svg, "#111918", hex);
    reach_d2d_replace_all(out_svg, "#292929", hex);
    reach_d2d_replace_all(out_svg, "#323232", hex);

    return REACH_OK;
}

static int reach_svg_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',';
}

static int reach_svg_is_command(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static const char *reach_svg_skip_separators(const char *cursor)
{
    while (cursor != nullptr && reach_svg_is_space(*cursor)) {
        ++cursor;
    }

    return cursor;
}

static int reach_svg_read_float(const char **cursor, float *out_value)
{
    if (cursor == nullptr || *cursor == nullptr || out_value == nullptr) {
        return 0;
    }

    const char *start = reach_svg_skip_separators(*cursor);
    if (start == nullptr || *start == 0 || reach_svg_is_command(*start)) {
        return 0;
    }

    char *end = nullptr;
    double value = strtod(start, &end);

    if (end == start) {
        return 0;
    }

    *out_value = static_cast<float>(value);
    *cursor = end;

    return 1;
}

static int reach_svg_read_point(const char **cursor, float *out_x, float *out_y)
{
    return reach_svg_read_float(cursor, out_x) &&
        reach_svg_read_float(cursor, out_y);
}

static void reach_svg_end_figure(
    ID2D1GeometrySink *sink,
    int *figure_open,
    D2D1_FIGURE_END end
)
{
    if (sink != nullptr && figure_open != nullptr && *figure_open) {
        sink->EndFigure(end);
        *figure_open = 0;
    }
}

static reach_result reach_svg_path_to_geometry(
    ID2D1Factory1 *factory,
    const std::string &path,
    ID2D1PathGeometry **out_geometry
)
{
    if (factory == nullptr || path.empty() || out_geometry == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_geometry = nullptr;

    ID2D1PathGeometry *geometry = nullptr;
    HRESULT hr = factory->CreatePathGeometry(&geometry);

    if (FAILED(hr) || geometry == nullptr) {
        return REACH_ERROR;
    }

    ID2D1GeometrySink *sink = nullptr;
    hr = geometry->Open(&sink);

    if (FAILED(hr) || sink == nullptr) {
        geometry->Release();
        return REACH_ERROR;
    }

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

    while (cursor != nullptr && *cursor != 0) {
        cursor = reach_svg_skip_separators(cursor);

        if (*cursor == 0) {
            break;
        }

        if (reach_svg_is_command(*cursor)) {
            command = *cursor;
            ++cursor;
        }

        if (command == 0) {
            break;
        }

        if (command == 'M' || command == 'm') {
            float x = 0.0f;
            float y = 0.0f;

            if (!reach_svg_read_point(&cursor, &x, &y)) {
                break;
            }

            if (command == 'm') {
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

        if (command == 'L' || command == 'l') {
            float x = 0.0f;
            float y = 0.0f;

            if (!reach_svg_read_point(&cursor, &x, &y)) {
                break;
            }

            if (command == 'l') {
                x += current_x;
                y += current_y;
            }

            if (figure_open) {
                sink->AddLine(D2D1::Point2F(x, y));
            }

            current_x = x;
            current_y = y;
            previous_was_cubic = 0;
            continue;
        }

        if (command == 'H' || command == 'h') {
            float x = 0.0f;

            if (!reach_svg_read_float(&cursor, &x)) {
                break;
            }

            if (command == 'h') {
                x += current_x;
            }

            if (figure_open) {
                sink->AddLine(D2D1::Point2F(x, current_y));
            }

            current_x = x;
            previous_was_cubic = 0;
            continue;
        }

        if (command == 'V' || command == 'v') {
            float y = 0.0f;

            if (!reach_svg_read_float(&cursor, &y)) {
                break;
            }

            if (command == 'v') {
                y += current_y;
            }

            if (figure_open) {
                sink->AddLine(D2D1::Point2F(current_x, y));
            }

            current_y = y;
            previous_was_cubic = 0;
            continue;
        }

        if (command == 'C' || command == 'c') {
            float x1 = 0.0f;
            float y1 = 0.0f;
            float x2 = 0.0f;
            float y2 = 0.0f;
            float x = 0.0f;
            float y = 0.0f;

            if (!reach_svg_read_point(&cursor, &x1, &y1) ||
                !reach_svg_read_point(&cursor, &x2, &y2) ||
                !reach_svg_read_point(&cursor, &x, &y)) {
                break;
            }

            if (command == 'c') {
                x1 += current_x;
                y1 += current_y;
                x2 += current_x;
                y2 += current_y;
                x += current_x;
                y += current_y;
            }

            if (figure_open) {
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

        if (command == 'S' || command == 's') {
            float x1 = previous_was_cubic
                ? current_x * 2.0f - previous_cubic_x
                : current_x;
            float y1 = previous_was_cubic
                ? current_y * 2.0f - previous_cubic_y
                : current_y;

            float x2 = 0.0f;
            float y2 = 0.0f;
            float x = 0.0f;
            float y = 0.0f;

            if (!reach_svg_read_point(&cursor, &x2, &y2) ||
                !reach_svg_read_point(&cursor, &x, &y)) {
                break;
            }

            if (command == 's') {
                x2 += current_x;
                y2 += current_y;
                x += current_x;
                y += current_y;
            }

            if (figure_open) {
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

        if (command == 'A' || command == 'a') {
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
                !reach_svg_read_float(&cursor, &sweep) ||
                !reach_svg_read_point(&cursor, &x, &y)) {
                break;
            }

            if (command == 'a') {
                x += current_x;
                y += current_y;
            }

            if (figure_open) {
                D2D1_ARC_SEGMENT segment = {};
                segment.point = D2D1::Point2F(x, y);
                segment.size = D2D1::SizeF(
                    radius_x < 0.0f ? -radius_x : radius_x,
                    radius_y < 0.0f ? -radius_y : radius_y);
                segment.rotationAngle = rotation;
                segment.sweepDirection = sweep != 0.0f
                    ? D2D1_SWEEP_DIRECTION_CLOCKWISE
                    : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
                segment.arcSize = large_arc != 0.0f
                    ? D2D1_ARC_SIZE_LARGE
                    : D2D1_ARC_SIZE_SMALL;
                sink->AddArc(segment);
            }

            current_x = x;
            current_y = y;
            previous_was_cubic = 0;
            continue;
        }

        if (command == 'Z' || command == 'z') {
            reach_svg_end_figure(sink, &figure_open, D2D1_FIGURE_END_CLOSED);
            current_x = start_x;
            current_y = start_y;
            previous_was_cubic = 0;
            command = 0;
            continue;
        }

        break;
    }

    reach_svg_end_figure(sink, &figure_open, D2D1_FIGURE_END_OPEN);

    hr = sink->Close();
    sink->Release();

    if (FAILED(hr)) {
        geometry->Release();
        return REACH_ERROR;
    }

    *out_geometry = geometry;
    return REACH_OK;
}

static int reach_svg_read_attribute(
    const std::string &tag,
    const char *name,
    std::string *out_value
)
{
    if (name == nullptr || out_value == nullptr) {
        return 0;
    }

    std::string key = std::string(name) + "=\"";
    size_t start = tag.find(key);

    if (start == std::string::npos) {
        return 0;
    }

    start += key.size();

    size_t end = tag.find('"', start);
    if (end == std::string::npos) {
        return 0;
    }

    *out_value = tag.substr(start, end - start);
    return 1;
}

static int reach_svg_view_box(
    const std::string &svg,
    float *out_x,
    float *out_y,
    float *out_width,
    float *out_height
)
{
    if (out_x == nullptr ||
        out_y == nullptr ||
        out_width == nullptr ||
        out_height == nullptr) {
        return 0;
    }

    std::string view_box;
    if (!reach_svg_read_attribute(svg, "viewBox", &view_box)) {
        return 0;
    }

    const char *cursor = view_box.c_str();

    return reach_svg_read_float(&cursor, out_x) &&
        reach_svg_read_float(&cursor, out_y) &&
        reach_svg_read_float(&cursor, out_width) &&
        reach_svg_read_float(&cursor, out_height);
}

static float reach_svg_attribute_float(
    const std::string &tag,
    const char *name,
    float fallback
)
{
    std::string value;
    if (!reach_svg_read_attribute(tag, name, &value)) {
        return fallback;
    }

    const char *cursor = value.c_str();
    float parsed = fallback;

    return reach_svg_read_float(&cursor, &parsed) ? parsed : fallback;
}

static int reach_svg_path_is_stroked(
    const std::string &svg,
    const std::string &tag
)
{
    if (tag.find("stroke=") != std::string::npos) {
        return 1;
    }

    return tag.find("fill=") == std::string::npos &&
        svg.find("stroke=") != std::string::npos;
}

static int reach_svg_path_in_ignored_section(
    const std::string &svg,
    size_t tag_start
)
{
    size_t defs_open = svg.rfind("<defs", tag_start);
    size_t defs_close = svg.rfind("</defs>", tag_start);

    if (defs_open != std::string::npos &&
        (defs_close == std::string::npos || defs_close < defs_open)) {
        return 1;
    }

    size_t clip_open = svg.rfind("<clipPath", tag_start);
    size_t clip_close = svg.rfind("</clipPath>", tag_start);

    return clip_open != std::string::npos &&
        (clip_close == std::string::npos || clip_close < clip_open);
}

static D2D1_MATRIX_3X2_F reach_svg_root_transform(
    const std::string &svg,
    float view_x,
    float view_y,
    float view_width,
    float view_height
)
{
    size_t svg_start = svg.find("<svg");
    size_t svg_end = svg_start != std::string::npos
        ? svg.find('>', svg_start)
        : std::string::npos;

    if (svg_start == std::string::npos || svg_end == std::string::npos) {
        return D2D1::Matrix3x2F::Identity();
    }

    std::string svg_tag = svg.substr(svg_start, svg_end - svg_start + 1);
    std::string transform;

    if (!reach_svg_read_attribute(svg_tag, "transform", &transform)) {
        return D2D1::Matrix3x2F::Identity();
    }

    const char *scale_start = strstr(transform.c_str(), "scale(");
    if (scale_start == nullptr) {
        return D2D1::Matrix3x2F::Identity();
    }

    const char *cursor = scale_start + 6;

    float scale_x = 1.0f;
    float scale_y = 1.0f;

    if (!reach_svg_read_float(&cursor, &scale_x)) {
        return D2D1::Matrix3x2F::Identity();
    }

    if (!reach_svg_read_float(&cursor, &scale_y)) {
        scale_y = scale_x;
    }

    float center_x = view_x + view_width * 0.5f;
    float center_y = view_y + view_height * 0.5f;

    return D2D1::Matrix3x2F::Translation(-center_x, -center_y) *
        D2D1::Matrix3x2F::Scale(scale_x, scale_y) *
        D2D1::Matrix3x2F::Translation(center_x, center_y);
}

static float reach_svg_inherited_stroke_width(
    const std::string &svg,
    size_t tag_start,
    float fallback
)
{
    size_t group_start = svg.rfind("<g", tag_start);
    size_t group_close = svg.rfind("</g>", tag_start);

    if (group_start == std::string::npos ||
        (group_close != std::string::npos && group_close > group_start)) {
        return fallback;
    }

    size_t group_end = svg.find('>', group_start);

    if (group_end == std::string::npos || group_end > tag_start) {
        return fallback;
    }

    std::string group_tag = svg.substr(group_start, group_end - group_start + 1);
    return reach_svg_attribute_float(group_tag, "stroke-width", fallback);
}

static reach_result reach_d2d_draw_svg_path_resource(
    reach_render_backend *backend,
    const reach_render_command *command,
    const std::string &svg
)
{
    ID2D1RenderTarget *target = reach_d2d_target(backend);

    if (backend == nullptr ||
        backend->factory == nullptr ||
        target == nullptr ||
        command == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    float view_x = 0.0f;
    float view_y = 0.0f;
    float view_width = 0.0f;
    float view_height = 0.0f;

    if (!reach_svg_view_box(svg, &view_x, &view_y, &view_width, &view_height) ||
        view_width <= 0.0f ||
        view_height <= 0.0f) {
        return REACH_ERROR;
    }

    ID2D1SolidColorBrush *brush = nullptr;
    HRESULT hr = target->CreateSolidColorBrush(
        reach_d2d_color(command->color),
        &brush);

    if (FAILED(hr) || brush == nullptr) {
        return REACH_ERROR;
    }

    float scale_x = command->rect.width / view_width;
    float scale_y = command->rect.height / view_height;

    D2D1_MATRIX_3X2_F transform =
        reach_svg_root_transform(svg, view_x, view_y, view_width, view_height) *
        D2D1::Matrix3x2F::Scale(scale_x, scale_y) *
        D2D1::Matrix3x2F::Translation(
            command->rect.x - view_x * scale_x,
            command->rect.y - view_y * scale_y);

    reach_result result = REACH_OK;
    size_t offset = 0;

    while (result == REACH_OK) {
        size_t tag_start = svg.find("<path", offset);

        if (tag_start == std::string::npos) {
            break;
        }

        if (reach_svg_path_in_ignored_section(svg, tag_start)) {
            offset = tag_start + 5;
            continue;
        }

        size_t tag_end = svg.find('>', tag_start);

        if (tag_end == std::string::npos) {
            break;
        }

        std::string tag = svg.substr(tag_start, tag_end - tag_start + 1);
        offset = tag_end + 1;

        std::string path_data;
        if (!reach_svg_read_attribute(tag, "d", &path_data)) {
            continue;
        }

        ID2D1PathGeometry *geometry = nullptr;
        result = reach_svg_path_to_geometry(backend->factory, path_data, &geometry);

        if (result != REACH_OK || geometry == nullptr) {
            continue;
        }

        ID2D1TransformedGeometry *transformed = nullptr;
        hr = backend->factory->CreateTransformedGeometry(
            geometry,
            transform,
            &transformed);
        geometry->Release();

        if (FAILED(hr) || transformed == nullptr) {
            result = REACH_ERROR;
            break;
        }

        if (reach_svg_path_is_stroked(svg, tag)) {
            float stroke_width = reach_svg_attribute_float(
                tag,
                "stroke-width",
                reach_svg_inherited_stroke_width(svg, tag_start, 1.0f));

            float stroke_scale = scale_x < scale_y ? scale_x : scale_y;
            target->DrawGeometry(transformed, brush, stroke_width * stroke_scale);
        } else {
            target->FillGeometry(transformed, brush);
        }

        transformed->Release();
    }

    brush->Release();

    return result;
}

reach_result reach_d2d_draw_vector_icon(
    reach_render_backend *backend,
    const reach_render_command *command
)
{
    if (backend == nullptr || command == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    int resource_id = reach_d2d_vector_icon_resource(command->icon_id);

    if (resource_id == 0) {
        return REACH_OK;
    }

    std::string svg_text;
    reach_result load_result = reach_d2d_load_svg_resource_text(
        resource_id,
        command->color,
        &svg_text);

    if (load_result != REACH_OK) {
        return load_result;
    }

    return reach_d2d_draw_svg_path_resource(backend, command, svg_text);
}
