#include "render_d2d_internal.h"

static HRESULT reach_d2d_draw_gradient_line(
    ID2D1RenderTarget *target,
    D2D1_POINT_2F start,
    D2D1_POINT_2F end,
    reach_color color,
    float stroke_width,
    float end_alpha = 0.0f
)
{
    D2D1_GRADIENT_STOP stops[2] = {};
    stops[0].position = 0.0f;
    stops[0].color = D2D1::ColorF(color.r, color.g, color.b, color.a);
    stops[1].position = 1.0f;
    stops[1].color = D2D1::ColorF(color.r, color.g, color.b, end_alpha);

    ID2D1GradientStopCollection *stop_collection = nullptr;
    HRESULT hr = target->CreateGradientStopCollection(stops, 2, &stop_collection);

    ID2D1LinearGradientBrush *brush = nullptr;
    if (SUCCEEDED(hr)) {
        hr = target->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(start, end),
            stop_collection,
            &brush);
    }

    if (SUCCEEDED(hr)) {
        target->DrawLine(start, end, brush, stroke_width);
    }

    if (brush != nullptr) {
        brush->Release();
    }
    if (stop_collection != nullptr) {
        stop_collection->Release();
    }

    return hr;
}

static HRESULT reach_d2d_draw_gradient_arc(
    ID2D1RenderTarget *target,
    D2D1_POINT_2F start,
    D2D1_POINT_2F end,
    D2D1_SIZE_F radius,
    D2D1_SWEEP_DIRECTION sweep,
    reach_color color,
    float stroke_width,
    float end_alpha = 0.85f
)
{
    ID2D1PathGeometry *geometry = nullptr;
    ID2D1GeometrySink *sink = nullptr;
    ID2D1Factory *factory = nullptr;

    target->GetFactory(&factory);

    HRESULT hr = factory != nullptr
        ? factory->CreatePathGeometry(&geometry)
        : E_FAIL;

    if (SUCCEEDED(hr)) {
        hr = geometry->Open(&sink);
    }

    if (SUCCEEDED(hr)) {
        sink->BeginFigure(start, D2D1_FIGURE_BEGIN_HOLLOW);

        D2D1_ARC_SEGMENT arc = {};
        arc.point = end;
        arc.size = radius;
        arc.rotationAngle = 0.0f;
        arc.sweepDirection = sweep;
        arc.arcSize = D2D1_ARC_SIZE_SMALL;

        sink->AddArc(arc);
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        hr = sink->Close();
    }

    D2D1_GRADIENT_STOP stops[2] = {};
    stops[0].position = 0.0f;
    stops[0].color = D2D1::ColorF(color.r, color.g, color.b, color.a);
    stops[1].position = 1.0f;
    stops[1].color = D2D1::ColorF(color.r, color.g, color.b, end_alpha);

    ID2D1GradientStopCollection *stop_collection = nullptr;
    ID2D1LinearGradientBrush *brush = nullptr;

    if (SUCCEEDED(hr)) {
        hr = target->CreateGradientStopCollection(stops, 2, &stop_collection);
    }
    if (SUCCEEDED(hr)) {
        hr = target->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(start, end),
            stop_collection,
            &brush);
    }
    if (SUCCEEDED(hr)) {
        target->DrawGeometry(geometry, brush, stroke_width);
    }

    if (brush != nullptr) {
        brush->Release();
    }
    if (stop_collection != nullptr) {
        stop_collection->Release();
    }
    if (sink != nullptr) {
        sink->Release();
    }
    if (geometry != nullptr) {
        geometry->Release();
    }
    if (factory != nullptr) {
        factory->Release();
    }

    return hr;
}

reach_result reach_d2d_draw_notched_rounded_rect(
    ID2D1RenderTarget *target,
    const reach_render_command *command
)
{
    if (target == nullptr || command == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    float x = command->rect.x;
    float y = command->rect.y;
    float w = command->rect.width;
    float h = command->rect.height;
    float r = command->radius;
    float notch_width = command->notch_width;
    float notch_height = command->notch_height;
    float notch_center = command->notch_center_x;
    float body_bottom = y + h - notch_height;

    if (r < 0.0f) {
        r = 0.0f;
    }
    if (r > w * 0.5f) {
        r = w * 0.5f;
    }
    if (r > (h - notch_height) * 0.5f) {
        r = (h - notch_height) * 0.5f;
    }
    if (notch_width < 0.0f) {
        notch_width = 0.0f;
    }
    if (notch_height < 0.0f) {
        notch_height = 0.0f;
    }

    float notch_left = notch_center - notch_width * 0.5f;
    float notch_right = notch_center + notch_width * 0.5f;

    if (notch_left < x + r) {
        notch_left = x + r;
    }
    if (notch_right > x + w - r) {
        notch_right = x + w - r;
    }

    notch_center = (notch_left + notch_right) * 0.5f;

    ID2D1SolidColorBrush *brush = nullptr;
    ID2D1Factory *factory = nullptr;
    ID2D1PathGeometry *geometry = nullptr;
    ID2D1GeometrySink *sink = nullptr;

    HRESULT hr = target->CreateSolidColorBrush(reach_d2d_color(command->color), &brush);

    if (SUCCEEDED(hr)) {
        target->GetFactory(&factory);
        hr = factory != nullptr
            ? factory->CreatePathGeometry(&geometry)
            : E_FAIL;
    }

    if (SUCCEEDED(hr)) {
        hr = geometry->Open(&sink);
    }

    if (SUCCEEDED(hr)) {
        sink->BeginFigure(D2D1::Point2F(x + r, y), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(x + w - r, y));

        D2D1_ARC_SEGMENT arc = {};
        arc.size = D2D1::SizeF(r, r);
        arc.rotationAngle = 0.0f;
        arc.sweepDirection = D2D1_SWEEP_DIRECTION_CLOCKWISE;
        arc.arcSize = D2D1_ARC_SIZE_SMALL;

        arc.point = D2D1::Point2F(x + w, y + r);
        sink->AddArc(arc);

        sink->AddLine(D2D1::Point2F(x + w, body_bottom - r));

        arc.point = D2D1::Point2F(x + w - r, body_bottom);
        sink->AddArc(arc);

        sink->AddLine(D2D1::Point2F(notch_right, body_bottom));
        sink->AddLine(D2D1::Point2F(notch_center, body_bottom + notch_height));
        sink->AddLine(D2D1::Point2F(notch_left, body_bottom));
        sink->AddLine(D2D1::Point2F(x + r, body_bottom));

        arc.point = D2D1::Point2F(x, body_bottom - r);
        sink->AddArc(arc);

        sink->AddLine(D2D1::Point2F(x, y + r));

        arc.point = D2D1::Point2F(x + r, y);
        sink->AddArc(arc);

        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        hr = sink->Close();
    }

    if (SUCCEEDED(hr)) {
        if (command->stroke_width > 0.0f) {
            target->DrawGeometry(geometry, brush, command->stroke_width);
        } else {
            target->FillGeometry(geometry, brush);
        }
    }

    if (sink != nullptr) {
        sink->Release();
    }
    if (geometry != nullptr) {
        geometry->Release();
    }
    if (factory != nullptr) {
        factory->Release();
    }
    if (brush != nullptr) {
        brush->Release();
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

reach_result reach_d2d_draw_triangle(
    ID2D1RenderTarget *target,
    const reach_render_command *command
)
{
    if (target == nullptr || command == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    ID2D1SolidColorBrush *brush = nullptr;
    ID2D1Factory *factory = nullptr;
    ID2D1PathGeometry *geometry = nullptr;
    ID2D1GeometrySink *sink = nullptr;

    HRESULT hr = target->CreateSolidColorBrush(reach_d2d_color(command->color), &brush);

    if (SUCCEEDED(hr)) {
        target->GetFactory(&factory);
        hr = factory != nullptr
            ? factory->CreatePathGeometry(&geometry)
            : E_FAIL;
    }

    if (SUCCEEDED(hr)) {
        hr = geometry->Open(&sink);
    }

    if (SUCCEEDED(hr)) {
        float x = command->rect.x;
        float y = command->rect.y;
        float w = command->rect.width;
        float h = command->rect.height;

        sink->BeginFigure(D2D1::Point2F(x, y), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(x + w, y));
        sink->AddLine(D2D1::Point2F(x + w * 0.5f, y + h));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);

        hr = sink->Close();
    }

    if (SUCCEEDED(hr)) {
        target->FillGeometry(geometry, brush);
    }

    if (sink != nullptr) {
        sink->Release();
    }
    if (geometry != nullptr) {
        geometry->Release();
    }
    if (factory != nullptr) {
        factory->Release();
    }
    if (brush != nullptr) {
        brush->Release();
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}

reach_result reach_d2d_draw_notch_stroke(
    ID2D1RenderTarget *target,
    const reach_render_command *command
)
{
    if (target == nullptr || command == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    ID2D1SolidColorBrush *brush = nullptr;
    HRESULT hr = target->CreateSolidColorBrush(reach_d2d_color(command->color), &brush);

    if (FAILED(hr) || brush == nullptr) {
        return REACH_ERROR;
    }

    float stroke_width = command->stroke_width > 0.0f
        ? command->stroke_width
        : 1.0f;

    float x = command->rect.x;
    float y = command->rect.y;
    float w = command->rect.width;
    float h = command->rect.height;

    D2D1_POINT_2F left = D2D1::Point2F(x, y);
    D2D1_POINT_2F tip = D2D1::Point2F(x + w * 0.5f, y + h);
    D2D1_POINT_2F right = D2D1::Point2F(x + w, y);

    target->DrawLine(left, tip, brush, stroke_width);
    target->DrawLine(tip, right, brush, stroke_width);

    brush->Release();

    return REACH_OK;
}

reach_result reach_d2d_draw_rect_or_rounded_rect(
    ID2D1RenderTarget *target,
    const reach_render_command *command
)
{
    if (target == nullptr || command == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    ID2D1SolidColorBrush *brush = nullptr;
    HRESULT hr = target->CreateSolidColorBrush(reach_d2d_color(command->color), &brush);

    if (FAILED(hr) || brush == nullptr) {
        return REACH_ERROR;
    }

    D2D1_ROUNDED_RECT rect = D2D1::RoundedRect(
        D2D1::RectF(
            command->rect.x,
            command->rect.y,
            command->rect.x + command->rect.width,
            command->rect.y + command->rect.height),
        command->radius,
        command->radius);

    if (command->type == REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE) {
        float stroke_width = command->stroke_width > 0.0f
            ? command->stroke_width
            : 1.0f;

        target->DrawRoundedRectangle(rect, brush, stroke_width);
    } else {
        target->FillRoundedRectangle(rect, brush);
    }

    brush->Release();

    return REACH_OK;
}

reach_result reach_d2d_draw_clipped_rounded_rect(
    ID2D1RenderTarget *target,
    const reach_render_command *command
)
{
    if (target == nullptr || command == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    ID2D1SolidColorBrush *brush = nullptr;
    HRESULT hr = target->CreateSolidColorBrush(reach_d2d_color(command->color), &brush);
    if (FAILED(hr) || brush == nullptr) {
        return REACH_ERROR;
    }

    ID2D1Factory *factory = nullptr;
    ID2D1RoundedRectangleGeometry *clip_geometry = nullptr;
    ID2D1Layer *layer = nullptr;

    target->GetFactory(&factory);
    if (factory == nullptr) {
        brush->Release();
        return REACH_ERROR;
    }

    D2D1_ROUNDED_RECT clip_rect = D2D1::RoundedRect(
        D2D1::RectF(
            command->clip_rect.x,
            command->clip_rect.y,
            command->clip_rect.x + command->clip_rect.width,
            command->clip_rect.y + command->clip_rect.height),
        command->clip_radius,
        command->clip_radius);

    hr = factory->CreateRoundedRectangleGeometry(clip_rect, &clip_geometry);
    if (SUCCEEDED(hr)) {
        hr = target->CreateLayer(nullptr, &layer);
    }

    if (SUCCEEDED(hr)) {
        D2D1_LAYER_PARAMETERS parameters = D2D1::LayerParameters(
            D2D1::RectF(
                command->clip_rect.x,
                command->clip_rect.y,
                command->clip_rect.x + command->clip_rect.width,
                command->clip_rect.y + command->clip_rect.height),
            clip_geometry);

        target->PushLayer(parameters, layer);

        D2D1_ROUNDED_RECT rect = D2D1::RoundedRect(
            D2D1::RectF(
                command->rect.x,
                command->rect.y,
                command->rect.x + command->rect.width,
                command->rect.y + command->rect.height),
            command->radius,
            command->radius);
        target->FillRoundedRectangle(rect, brush);
        target->PopLayer();
    }

    if (layer != nullptr) {
        layer->Release();
    }
    if (clip_geometry != nullptr) {
        clip_geometry->Release();
    }
    if (factory != nullptr) {
        factory->Release();
    }
    if (brush != nullptr) {
        brush->Release();
    }

    return SUCCEEDED(hr) ? REACH_OK : REACH_ERROR;
}
