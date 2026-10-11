#include "render_d2d_internal.h"

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
        return reach_d2d_sync_retained(backend, commands);
    }

    D2D1::Matrix3x2F content = D2D1::Matrix3x2F::Scale(commands->content_transform.scale_x,
                                                       commands->content_transform.scale_y) *
                               D2D1::Matrix3x2F::Translation(commands->content_transform.offset_x,
                                                             commands->content_transform.offset_y);
    D2D1::Matrix3x2F root = D2D1::Matrix3x2F::Identity();
    float root_opacity = 1.0f;
    if (commands->has_presentation)
    {
        const reach_render_group &group = commands->presentation;
        float scale = group.scale.generation ? reach_motion_value(&group.scale) : 1.0f;
        root = D2D1::Matrix3x2F::Scale(scale, scale,
                                       D2D1::Point2F(group.bounds.x + group.bounds.width * 0.5f,
                                                     group.bounds.y + group.bounds.height * 0.5f)) *
               D2D1::Matrix3x2F::Translation(reach_motion_value(&group.x),
                                             reach_motion_value(&group.y));
        root_opacity = group.opacity.generation ? reach_motion_value(&group.opacity) : 1.0f;
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
            target->PushAxisAlignedClip(D2D1::RectF(group->clip.x, group->clip.y,
                                                    group->clip.x + group->clip.width,
                                                    group->clip.y + group->clip.height),
                                        D2D1_ANTIALIAS_MODE_ALIASED);
        float opacity = root_opacity;
        if (group != nullptr)
        {
            float scale = group->scale.generation ? reach_motion_value(&group->scale) : 1.0f;
            D2D1::Matrix3x2F local =
                D2D1::Matrix3x2F::Scale(scale, scale,
                                        D2D1::Point2F(group->bounds.x, group->bounds.y)) *
                D2D1::Matrix3x2F::Translation(reach_motion_value(&group->x),
                                              reach_motion_value(&group->y));
            target->SetTransform(local * content * root);
            if (group->opacity.generation)
                opacity *= reach_motion_value(&group->opacity);
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
