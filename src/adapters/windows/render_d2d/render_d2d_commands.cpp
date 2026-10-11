#include "render_d2d_internal.h"
#include "reach/support/workload.h"

#include <math.h>

static reach_result reach_d2d_execute_command(reach_render_backend *backend,
                                              ID2D1RenderTarget *target,
                                              const reach_render_command *command)
{
    if (command->type == REACH_RENDER_COMMAND_BLURRED_IMAGE)
    {
        if (command->icon_id == 0)
        {
            return REACH_OK;
        }

        (void)reach_d2d_draw_blurred_image(backend, command);
        return REACH_OK;
    }

    if (command->type == REACH_RENDER_COMMAND_ICON)
    {
        if (command->icon_id == 0)
        {
            return REACH_OK;
        }

        (void)reach_d2d_draw_icon(backend, command);
        return REACH_OK;
    }

    if (command->type == REACH_RENDER_COMMAND_VECTOR_ICON && command->icon_id != 0)
    {
        return reach_d2d_draw_vector_icon(backend, command);
    }

    if (command->type == REACH_RENDER_COMMAND_SHADOW)
    {
        return reach_d2d_draw_shadow(backend, command);
    }

    if (command->type == REACH_RENDER_COMMAND_NOTCHED_ROUNDED_RECT)
    {
        return reach_d2d_fill_notched_rounded_rect(target, command);
    }

    if (command->type == REACH_RENDER_COMMAND_TRIANGLE)
    {
        return reach_d2d_draw_triangle(target, command);
    }

    if (command->type == REACH_RENDER_COMMAND_ICON_TINT)
    {
        if (command->icon_id == 0)
        {
            return REACH_OK;
        }

        (void)reach_d2d_draw_icon_tint(backend, command);
        return REACH_OK;
    }

    if (command->type == REACH_RENDER_COMMAND_RECT)
    {
        return reach_d2d_fill_rect_or_rounded_rect(target, command);
    }

    if (command->type == REACH_RENDER_COMMAND_ARC_STROKE)
    {
        return reach_d2d_draw_arc_stroke(target, command);
    }

    if (command->type == REACH_RENDER_COMMAND_CLIPPED_ROUNDED_RECT)
    {
        return reach_d2d_draw_clipped_rounded_rect(target, command);
    }

    if (command->type == REACH_RENDER_COMMAND_TEXT)
    {
        return reach_d2d_draw_text(backend, command);
    }

    if (command->type == REACH_RENDER_COMMAND_TEXTBOX)
    {
        return reach_d2d_draw_textbox(backend, command);
    }

    return REACH_OK;
}

static int32_t reach_d2d_scissor_visible(const reach_render_command *command)
{
    return command->scissor_rect.width > 0.0f && command->scissor_rect.height > 0.0f;
}

reach_result reach_d2d_execute_commands(reach_render_backend *backend,
                                        const reach_render_command *commands, size_t count)
{
    ID2D1RenderTarget *target = reach_d2d_target(backend);
    for (size_t index = 0; index < count; ++index)
    {
        const reach_render_command *command = &commands[index];
        if (command->has_scissor && !reach_d2d_scissor_visible(command))
        {
            continue;
        }
        if (command->has_scissor)
        {
            target->PushAxisAlignedClip(
                D2D1::RectF(command->scissor_rect.x, command->scissor_rect.y,
                            command->scissor_rect.x + command->scissor_rect.width,
                            command->scissor_rect.y + command->scissor_rect.height),
                D2D1_ANTIALIAS_MODE_ALIASED);
        }
        reach_result result = reach_d2d_execute_command(backend, target, command);
        if (command->has_scissor)
        {
            target->PopAxisAlignedClip();
        }
        if (result != REACH_OK)
        {
            return result;
        }
    }
    return REACH_OK;
}

static reach_result reach_d2d_execute_tinted_group(reach_render_backend *backend,
                                                   ID2D1RenderTarget *target,
                                                   const reach_render_group &group,
                                                   const reach_render_command *commands,
                                                   size_t count, float opacity, double now_seconds)
{
    if (!isfinite(group.bounds.width) || !isfinite(group.bounds.height) ||
        group.bounds.width <= 0.0f || group.bounds.height <= 0.0f ||
        group.bounds.width > 16384.0f || group.bounds.height > 16384.0f)
        return REACH_INVALID_ARGUMENT;
    UINT width = (UINT)ceilf(group.bounds.width);
    UINT height = (UINT)ceilf(group.bounds.height);
    if ((uint64_t)width * height * 4 > UINT64_C(128) * 1024 * 1024)
        return REACH_ERROR;
    ComPtr<ID2D1BitmapRenderTarget> coverage;
    D2D1_SIZE_F size = D2D1::SizeF((float)width, (float)height);
    D2D1_SIZE_U pixels = D2D1::SizeU(width, height);
    D2D1_PIXEL_FORMAT format =
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED);
    HRESULT hr = target->CreateCompatibleRenderTarget(
        &size, &pixels, &format, D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE, &coverage);
    if (FAILED(hr))
        return REACH_ERROR;
    coverage->BeginDraw();
    coverage->Clear(D2D1::ColorF(0, 0, 0, 0));
    coverage->SetTransform(D2D1::Matrix3x2F::Translation(-group.bounds.x, -group.bounds.y));
    ID2D1RenderTarget *previous = backend->command_target;
    backend->command_target = coverage.Get();
    reach_result result = reach_d2d_execute_commands(backend, commands, count);
    backend->command_target = previous;
    hr = coverage->EndDraw();
    if (result != REACH_OK)
        return result;
    if (FAILED(hr))
        return REACH_ERROR;
    ComPtr<ID2D1Bitmap> bitmap;
    ComPtr<ID2D1SolidColorBrush> brush;
    hr = coverage->GetBitmap(&bitmap);
    reach_color tint = reach_render_group_tint_at(&group, now_seconds);
    tint.a *= opacity;
    if (SUCCEEDED(hr))
        hr = target->CreateSolidColorBrush(reach_d2d_color(tint), &brush);
    if (FAILED(hr))
        return REACH_ERROR;
    D2D1_ANTIALIAS_MODE antialias = target->GetAntialiasMode();
    target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
    D2D1_RECT_F destination =
        D2D1::RectF(group.bounds.x, group.bounds.y, group.bounds.x + (float)width,
                    group.bounds.y + (float)height);
    target->FillOpacityMask(bitmap.Get(), brush.Get(), D2D1_OPACITY_MASK_CONTENT_GRAPHICS,
                            &destination);
    target->SetAntialiasMode(antialias);
    return REACH_OK;
}

reach_result reach_d2d_execute(reach_render_backend *backend,
                               const reach_render_command_buffer *commands)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(commands != nullptr);

    ID2D1RenderTarget *target = reach_d2d_target(backend);
    if (backend == nullptr || target == nullptr || commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (reach_d2d_retained_supported(backend))
    {
        reach_workload_add(REACH_WORKLOAD_RENDER_EXECUTIONS, 1);
        reach_workload_add(backend->compositor != nullptr ? REACH_WORKLOAD_WUC_EXECUTIONS
                                                          : REACH_WORKLOAD_DCOMP_EXECUTIONS,
                           1);
        return reach_d2d_sync_retained(backend, commands);
    }

    reach_workload_add(REACH_WORKLOAD_RENDER_EXECUTIONS, 1);
    reach_workload_add(REACH_WORKLOAD_FALLBACK_EXECUTIONS, 1);
    D2D1::Matrix3x2F content = D2D1::Matrix3x2F::Scale(commands->content_transform.scale_x,
                                                       commands->content_transform.scale_y) *
                               D2D1::Matrix3x2F::Translation(commands->content_transform.offset_x,
                                                             commands->content_transform.offset_y);
    D2D1::Matrix3x2F root = D2D1::Matrix3x2F::Identity();
    float root_opacity = 1.0f;
    double now_seconds = reach_motion_now_seconds();
    if (commands->has_presentation)
    {
        const reach_render_group &group = commands->presentation;
        float scale =
            group.scale.generation ? reach_motion_value_at(&group.scale, now_seconds) : 1.0f;
        root = D2D1::Matrix3x2F::Scale(scale, scale,
                                       D2D1::Point2F(group.bounds.x + group.bounds.width * 0.5f,
                                                     group.bounds.y + group.bounds.height * 0.5f)) *
               D2D1::Matrix3x2F::Translation(reach_motion_value_at(&group.x, now_seconds),
                                             reach_motion_value_at(&group.y, now_seconds));
        root_opacity =
            group.opacity.generation ? reach_motion_value_at(&group.opacity, now_seconds) : 1.0f;
    }
    reach_result outcome = REACH_OK;
    for (size_t index = 0; index < commands->count && outcome == REACH_OK; ++index)
    {
        reach_render_command command = commands->commands[index];
        const reach_render_group *group = nullptr;
        for (size_t at = 0; at < commands->group_count; ++at)
            if (command.group_id == commands->groups[at].id)
                group = &commands->groups[at];
        target->SetTransform(content * root);
        if (group != nullptr && group->has_clip)
        {
            reach_rect_f32 clip = reach_render_group_clip_at(group, now_seconds);
            target->PushAxisAlignedClip(
                D2D1::RectF(clip.x, clip.y, clip.x + clip.width, clip.y + clip.height),
                D2D1_ANTIALIAS_MODE_ALIASED);
        }
        float opacity = root_opacity;
        if (group != nullptr)
        {
            float scale =
                group->scale.generation ? reach_motion_value_at(&group->scale, now_seconds) : 1.0f;
            D2D1::Matrix3x2F local =
                D2D1::Matrix3x2F::Scale(scale, scale,
                                        D2D1::Point2F(group->bounds.x, group->bounds.y)) *
                D2D1::Matrix3x2F::Translation(reach_motion_value_at(&group->x, now_seconds),
                                              reach_motion_value_at(&group->y, now_seconds));
            target->SetTransform(local * content * root);
            if (group->opacity.generation)
                opacity *= reach_motion_value_at(&group->opacity, now_seconds);
        }
        if (group != nullptr && group->has_tint)
        {
            size_t end = index + 1;
            while (end < commands->count && commands->commands[end].group_id == group->id)
                ++end;
            outcome =
                reach_d2d_execute_tinted_group(backend, target, *group, commands->commands + index,
                                               end - index, opacity, now_seconds);
            if (group->has_clip)
                target->PopAxisAlignedClip();
            index = end - 1;
            continue;
        }
        command.color.a *= opacity;
        command.text_color.a *= opacity;
        command.placeholder_color.a *= opacity;
        command.selection_color.a *= opacity;
        bool scissor = command.has_scissor && reach_d2d_scissor_visible(&command);
        if (scissor)
            target->PushAxisAlignedClip(
                D2D1::RectF(command.scissor_rect.x, command.scissor_rect.y,
                            command.scissor_rect.x + command.scissor_rect.width,
                            command.scissor_rect.y + command.scissor_rect.height),
                D2D1_ANTIALIAS_MODE_ALIASED);
        if (!command.has_scissor || scissor)
            outcome = reach_d2d_execute_command(backend, target, &command);
        if (scissor)
            target->PopAxisAlignedClip();
        if (group != nullptr && group->has_clip)
            target->PopAxisAlignedClip();
    }
    target->SetTransform(D2D1::Matrix3x2F::Identity());
    return outcome;
}
