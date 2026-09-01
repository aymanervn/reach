#ifndef REACH_ADAPTERS_WINDOWS_RENDER_D2D_SVG_DOCUMENT_H
#define REACH_ADAPTERS_WINDOWS_RENDER_D2D_SVG_DOCUMENT_H

#include <stddef.h>

#include <string>
#include <vector>

typedef struct reach_svg_matrix
{
    float m11;
    float m12;
    float m21;
    float m22;
    float dx;
    float dy;
} reach_svg_matrix;

typedef enum reach_svg_drawable_kind
{
    REACH_SVG_DRAWABLE_PATH = 0,
    REACH_SVG_DRAWABLE_CIRCLE = 1
} reach_svg_drawable_kind;

typedef struct reach_svg_drawable
{
    reach_svg_drawable_kind kind;
    size_t tag_start;
    size_t tag_end;
    reach_svg_matrix transform;
} reach_svg_drawable;

int reach_svg_read_attribute(const std::string &tag, const char *name, std::string *out_value);
int reach_svg_read_style_property(const std::string &tag, const char *name, std::string *out_value);
reach_svg_matrix reach_svg_matrix_identity(void);
reach_svg_matrix reach_svg_matrix_multiply(reach_svg_matrix first, reach_svg_matrix second);
int reach_svg_parse_transform(const std::string &value, reach_svg_matrix *out_transform);
int reach_svg_collect_drawables(const std::string &svg,
                                std::vector<reach_svg_drawable> *out_drawables);

#endif
