#include "render_d2d_svg_document.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

static int reach_svg_is_space(char value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

static int reach_svg_is_separator(char value)
{
    return reach_svg_is_space(value) || value == ',';
}

static int reach_svg_attribute_name_boundary_before(const std::string &tag, size_t position)
{
    if (position == 0)
    {
        return 1;
    }

    char value = tag[position - 1];
    return reach_svg_is_space(value) || value == '<' || value == '/';
}

static int reach_svg_attribute_name_boundary_after(const std::string &tag, size_t position)
{
    if (position >= tag.size())
    {
        return 0;
    }

    char value = tag[position];
    return reach_svg_is_space(value) || value == '=';
}

int reach_svg_read_attribute(const std::string &tag, const char *name, std::string *out_value)
{
    if (name == nullptr || out_value == nullptr)
    {
        return 0;
    }

    size_t name_length = strlen(name);
    size_t position = 0;

    while ((position = tag.find(name, position)) != std::string::npos)
    {
        size_t after_name = position + name_length;

        if (!reach_svg_attribute_name_boundary_before(tag, position) ||
            !reach_svg_attribute_name_boundary_after(tag, after_name))
        {
            position = after_name;
            continue;
        }

        size_t cursor = after_name;
        while (cursor < tag.size() && reach_svg_is_space(tag[cursor]))
        {
            ++cursor;
        }

        if (cursor >= tag.size() || tag[cursor] != '=')
        {
            position = after_name;
            continue;
        }

        ++cursor;
        while (cursor < tag.size() && reach_svg_is_space(tag[cursor]))
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

int reach_svg_read_style_property(const std::string &tag, const char *name, std::string *out_value)
{
    if (name == nullptr || out_value == nullptr)
    {
        return 0;
    }

    std::string style;
    if (!reach_svg_read_attribute(tag, "style", &style))
    {
        return 0;
    }

    size_t cursor = 0;
    while (cursor < style.size())
    {
        while (cursor < style.size() && (reach_svg_is_space(style[cursor]) || style[cursor] == ';'))
        {
            ++cursor;
        }

        size_t property_start = cursor;
        size_t colon = style.find(':', property_start);
        if (colon == std::string::npos)
        {
            return 0;
        }

        size_t property_end = colon;
        while (property_end > property_start && reach_svg_is_space(style[property_end - 1]))
        {
            --property_end;
        }

        size_t value_start = colon + 1;
        while (value_start < style.size() && reach_svg_is_space(style[value_start]))
        {
            ++value_start;
        }

        size_t semicolon = style.find(';', value_start);
        size_t value_end = semicolon == std::string::npos ? style.size() : semicolon;
        while (value_end > value_start && reach_svg_is_space(style[value_end - 1]))
        {
            --value_end;
        }

        if (style.compare(property_start, property_end - property_start, name) == 0 &&
            strlen(name) == property_end - property_start)
        {
            *out_value = style.substr(value_start, value_end - value_start);
            return 1;
        }

        if (semicolon == std::string::npos)
        {
            break;
        }
        cursor = semicolon + 1;
    }

    return 0;
}

reach_svg_matrix reach_svg_matrix_identity(void)
{
    return {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
}

reach_svg_matrix reach_svg_matrix_multiply(reach_svg_matrix first, reach_svg_matrix second)
{
    reach_svg_matrix result = {};
    result.m11 = first.m11 * second.m11 + first.m12 * second.m21;
    result.m12 = first.m11 * second.m12 + first.m12 * second.m22;
    result.m21 = first.m21 * second.m11 + first.m22 * second.m21;
    result.m22 = first.m21 * second.m12 + first.m22 * second.m22;
    result.dx = first.dx * second.m11 + first.dy * second.m21 + second.dx;
    result.dy = first.dx * second.m12 + first.dy * second.m22 + second.dy;
    return result;
}

static const char *reach_svg_skip_separators(const char *cursor)
{
    while (cursor != nullptr && reach_svg_is_separator(*cursor))
    {
        ++cursor;
    }
    return cursor;
}

static int reach_svg_read_transform_number(const char **cursor, float *out_value)
{
    if (cursor == nullptr || *cursor == nullptr || out_value == nullptr)
    {
        return 0;
    }

    const char *start = reach_svg_skip_separators(*cursor);
    char *end = nullptr;
    double value = strtod(start, &end);
    if (end == start || !std::isfinite(value))
    {
        return 0;
    }

    *cursor = end;
    *out_value = static_cast<float>(value);
    return 1;
}

static reach_svg_matrix reach_svg_translation(float x, float y)
{
    return {1.0f, 0.0f, 0.0f, 1.0f, x, y};
}

static reach_svg_matrix reach_svg_scale(float x, float y)
{
    return {x, 0.0f, 0.0f, y, 0.0f, 0.0f};
}

static reach_svg_matrix reach_svg_rotation(float degrees)
{
    constexpr float radians_per_degree = 0.017453292519943295769f;
    float radians = degrees * radians_per_degree;
    float cosine = std::cos(radians);
    float sine = std::sin(radians);
    return {cosine, sine, -sine, cosine, 0.0f, 0.0f};
}

static int reach_svg_transform_for_function(const std::string &name, const float *arguments,
                                            size_t argument_count, reach_svg_matrix *out_transform)
{
    if (out_transform == nullptr)
    {
        return 0;
    }

    if (name == "matrix" && argument_count == 6)
    {
        *out_transform = {arguments[0], arguments[1], arguments[2],
                          arguments[3], arguments[4], arguments[5]};
        return 1;
    }
    if (name == "translate" && (argument_count == 1 || argument_count == 2))
    {
        *out_transform =
            reach_svg_translation(arguments[0], argument_count == 2 ? arguments[1] : 0.0f);
        return 1;
    }
    if (name == "scale" && (argument_count == 1 || argument_count == 2))
    {
        *out_transform =
            reach_svg_scale(arguments[0], argument_count == 2 ? arguments[1] : arguments[0]);
        return 1;
    }
    if (name == "rotate" && (argument_count == 1 || argument_count == 3))
    {
        reach_svg_matrix rotation = reach_svg_rotation(arguments[0]);
        if (argument_count == 3)
        {
            rotation = reach_svg_matrix_multiply(
                reach_svg_translation(-arguments[1], -arguments[2]), rotation);
            rotation = reach_svg_matrix_multiply(rotation,
                                                 reach_svg_translation(arguments[1], arguments[2]));
        }
        *out_transform = rotation;
        return 1;
    }
    if ((name == "skewX" || name == "skewY") && argument_count == 1)
    {
        constexpr float radians_per_degree = 0.017453292519943295769f;
        float tangent = std::tan(arguments[0] * radians_per_degree);
        if (!std::isfinite(tangent))
        {
            return 0;
        }
        *out_transform = name == "skewX" ? reach_svg_matrix{1.0f, 0.0f, tangent, 1.0f, 0.0f, 0.0f}
                                         : reach_svg_matrix{1.0f, tangent, 0.0f, 1.0f, 0.0f, 0.0f};
        return 1;
    }
    return 0;
}

int reach_svg_parse_transform(const std::string &value, reach_svg_matrix *out_transform)
{
    if (out_transform == nullptr)
    {
        return 0;
    }

    reach_svg_matrix transform = reach_svg_matrix_identity();
    const char *cursor = value.c_str();

    while (1)
    {
        cursor = reach_svg_skip_separators(cursor);
        if (*cursor == 0)
        {
            *out_transform = transform;
            return 1;
        }

        const char *name_start = cursor;
        while ((*cursor >= 'A' && *cursor <= 'Z') || (*cursor >= 'a' && *cursor <= 'z'))
        {
            ++cursor;
        }
        if (cursor == name_start)
        {
            return 0;
        }

        std::string name(name_start, cursor);
        while (reach_svg_is_space(*cursor))
        {
            ++cursor;
        }
        if (*cursor != '(')
        {
            return 0;
        }
        ++cursor;

        float arguments[6] = {};
        size_t argument_count = 0;
        while (1)
        {
            cursor = reach_svg_skip_separators(cursor);
            if (*cursor == ')')
            {
                ++cursor;
                break;
            }
            if (*cursor == 0 || argument_count == 6 ||
                !reach_svg_read_transform_number(&cursor, &arguments[argument_count]))
            {
                return 0;
            }
            ++argument_count;
        }

        reach_svg_matrix function_transform = {};
        if (!reach_svg_transform_for_function(name, arguments, argument_count, &function_transform))
        {
            return 0;
        }
        transform = reach_svg_matrix_multiply(function_transform, transform);
    }
}

static int reach_svg_ascii_equal(const std::string &left, const char *right)
{
    if (right == nullptr || left.size() != strlen(right))
    {
        return 0;
    }

    for (size_t index = 0; index < left.size(); ++index)
    {
        char a = left[index];
        char b = right[index];
        if (a >= 'A' && a <= 'Z')
        {
            a = static_cast<char>(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z')
        {
            b = static_cast<char>(b - 'A' + 'a');
        }
        if (a != b)
        {
            return 0;
        }
    }
    return 1;
}

static size_t reach_svg_tag_end(const std::string &svg, size_t tag_start)
{
    char quote = 0;
    for (size_t index = tag_start + 1; index < svg.size(); ++index)
    {
        char value = svg[index];
        if (quote != 0)
        {
            if (value == quote)
            {
                quote = 0;
            }
        }
        else if (value == '"' || value == '\'')
        {
            quote = value;
        }
        else if (value == '>')
        {
            return index;
        }
    }
    return std::string::npos;
}

static int reach_svg_tag_name(const std::string &tag, std::string *out_name, int *out_closing,
                              int *out_self_closing)
{
    if (out_name == nullptr || out_closing == nullptr || out_self_closing == nullptr ||
        tag.size() < 3 || tag.front() != '<' || tag.back() != '>')
    {
        return 0;
    }

    size_t cursor = 1;
    while (cursor < tag.size() && reach_svg_is_space(tag[cursor]))
    {
        ++cursor;
    }

    *out_closing = cursor < tag.size() && tag[cursor] == '/';
    if (*out_closing)
    {
        ++cursor;
    }
    while (cursor < tag.size() && reach_svg_is_space(tag[cursor]))
    {
        ++cursor;
    }

    size_t name_start = cursor;
    while (cursor < tag.size())
    {
        char value = tag[cursor];
        if (!(value == ':' || value == '_' || value == '-' || (value >= '0' && value <= '9') ||
              (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z')))
        {
            break;
        }
        ++cursor;
    }
    if (cursor == name_start)
    {
        return 0;
    }

    *out_name = tag.substr(name_start, cursor - name_start);
    size_t end = tag.size() - 1;
    while (end > 0 && reach_svg_is_space(tag[end - 1]))
    {
        --end;
    }
    *out_self_closing = end > 0 && tag[end - 1] == '/';
    return 1;
}

typedef struct reach_svg_document_frame
{
    reach_svg_matrix transform;
    int ignored;
} reach_svg_document_frame;

int reach_svg_collect_drawables(const std::string &svg,
                                std::vector<reach_svg_drawable> *out_drawables)
{
    if (out_drawables == nullptr)
    {
        return 0;
    }

    out_drawables->clear();
    std::vector<reach_svg_document_frame> stack;
    size_t offset = 0;

    while ((offset = svg.find('<', offset)) != std::string::npos)
    {
        if (svg.compare(offset, 4, "<!--") == 0)
        {
            size_t comment_end = svg.find("-->", offset + 4);
            if (comment_end == std::string::npos)
            {
                return 0;
            }
            offset = comment_end + 3;
            continue;
        }

        size_t tag_end = reach_svg_tag_end(svg, offset);
        if (tag_end == std::string::npos)
        {
            return 0;
        }

        std::string tag = svg.substr(offset, tag_end - offset + 1);
        std::string name;
        int closing = 0;
        int self_closing = 0;
        if (!reach_svg_tag_name(tag, &name, &closing, &self_closing))
        {
            offset = tag_end + 1;
            continue;
        }

        if (closing)
        {
            if (stack.empty())
            {
                return 0;
            }
            stack.pop_back();
            offset = tag_end + 1;
            continue;
        }

        reach_svg_document_frame frame = {};
        frame.transform = stack.empty() ? reach_svg_matrix_identity() : stack.back().transform;
        frame.ignored = (!stack.empty() && stack.back().ignored) ||
                        reach_svg_ascii_equal(name, "defs") ||
                        reach_svg_ascii_equal(name, "clipPath");

        std::string transform_value;
        if (reach_svg_read_attribute(tag, "transform", &transform_value))
        {
            reach_svg_matrix local_transform = {};
            if (!reach_svg_parse_transform(transform_value, &local_transform))
            {
                return 0;
            }
            frame.transform = reach_svg_matrix_multiply(local_transform, frame.transform);
        }

        if (!frame.ignored &&
            (reach_svg_ascii_equal(name, "path") || reach_svg_ascii_equal(name, "circle")))
        {
            reach_svg_drawable drawable = {};
            drawable.kind = reach_svg_ascii_equal(name, "path") ? REACH_SVG_DRAWABLE_PATH
                                                                : REACH_SVG_DRAWABLE_CIRCLE;
            drawable.tag_start = offset;
            drawable.tag_end = tag_end;
            drawable.transform = frame.transform;
            out_drawables->push_back(drawable);
        }

        if (!self_closing)
        {
            stack.push_back(frame);
        }
        offset = tag_end + 1;
    }

    return stack.empty();
}
