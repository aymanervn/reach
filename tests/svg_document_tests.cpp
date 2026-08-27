#include "render_d2d_svg_document.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdio.h>

static int failed = 0;

static void expect(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        failed += 1;
    }
}

static int near(float actual, float expected)
{
    return std::fabs(actual - expected) < 0.0001f;
}

static void transform_point(reach_svg_matrix transform, float x, float y, float *out_x,
                            float *out_y)
{
    *out_x = x * transform.m11 + y * transform.m21 + transform.dx;
    *out_y = x * transform.m12 + y * transform.m22 + transform.dy;
}

static void expect_point(reach_svg_matrix transform, float x, float y, float expected_x,
                         float expected_y, const char *message)
{
    float actual_x = 0.0f;
    float actual_y = 0.0f;
    transform_point(transform, x, y, &actual_x, &actual_y);
    expect(near(actual_x, expected_x) && near(actual_y, expected_y), message);
}

static void expect_all_svg_resources_parse(void)
{
    size_t resource_count = 0;
    std::filesystem::path resources = std::filesystem::path(REACH_SOURCE_DIR) / "resources";
    for (const std::filesystem::directory_entry &entry :
         std::filesystem::recursive_directory_iterator(resources))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".svg")
        {
            continue;
        }

        std::ifstream stream(entry.path(), std::ios::binary);
        std::string svg((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        std::vector<reach_svg_drawable> drawables;
        if (!stream.good() && !stream.eof())
        {
            fprintf(stderr, "FAIL: could not read %s\n", entry.path().string().c_str());
            failed += 1;
        }
        else if (!reach_svg_collect_drawables(svg, &drawables) || drawables.empty())
        {
            fprintf(stderr, "FAIL: could not parse drawables from %s\n",
                    entry.path().string().c_str());
            failed += 1;
        }
        resource_count += 1;
    }
    expect(resource_count > 0, "SVG resources are discovered");
}

int main(void)
{
    std::string style_value;
    expect(reach_svg_read_style_property("<path style='stroke: red; fill: none'/>", "fill",
                                         &style_value) &&
               style_value == "none",
           "inline style properties parse");

    reach_svg_matrix transform = {};
    expect(reach_svg_parse_transform("matrix(1 2 3 4 5 6)", &transform), "matrix parses");
    expect(near(transform.m11, 1.0f) && near(transform.m12, 2.0f) && near(transform.m21, 3.0f) &&
               near(transform.m22, 4.0f) && near(transform.dx, 5.0f) && near(transform.dy, 6.0f),
           "matrix values map to SVG coordinates");

    expect(reach_svg_parse_transform("translate(10,20) scale(2)", &transform),
           "transform list parses");
    expect_point(transform, 1.0f, 1.0f, 12.0f, 22.0f, "transform list order is preserved");

    expect(reach_svg_parse_transform("rotate(90 10 10)", &transform), "centered rotation parses");
    expect_point(transform, 11.0f, 10.0f, 10.0f, 11.0f, "centered rotation keeps its center");

    expect(reach_svg_parse_transform("skewX(45) skewY(45)", &transform), "skews parse");
    expect_point(transform, 1.0f, 1.0f, 3.0f, 2.0f, "skews compose in SVG order");

    const std::string svg =
        "<svg viewBox='0 0 25 25'><defs><path d='ignored'/></defs>"
        "<g transform='translate(10 20)'><g transform='scale(2)'>"
        "<path d='visible' transform='translate(3 4)'/><circle cx='1' cy='1' r='1'/>"
        "</g></g><clipPath><circle cx='0' cy='0' r='1'/></clipPath></svg>";
    std::vector<reach_svg_drawable> drawables;
    expect(reach_svg_collect_drawables(svg, &drawables), "SVG document parses");
    expect(drawables.size() == 2, "only visible path and circle are collected");
    if (drawables.size() == 2)
    {
        expect(drawables[0].kind == REACH_SVG_DRAWABLE_PATH, "path kind is retained");
        expect_point(drawables[0].transform, 1.0f, 1.0f, 18.0f, 30.0f,
                     "nested group and element transforms compose");
        expect(drawables[1].kind == REACH_SVG_DRAWABLE_CIRCLE, "circle kind is retained");
        expect_point(drawables[1].transform, 1.0f, 1.0f, 12.0f, 22.0f,
                     "sibling inherits group transforms only");
    }

    expect(!reach_svg_parse_transform("translate(1 2 3)", &transform),
           "invalid transform arity is rejected");
    expect(!reach_svg_collect_drawables("<svg><path d='x' transform='unsupported(1)'/></svg>",
                                        &drawables),
           "unsupported element transforms fail the document");

    expect_all_svg_resources_parse();

    return failed == 0 ? 0 : 1;
}
