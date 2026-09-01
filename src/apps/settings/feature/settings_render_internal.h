#ifndef REACH_APPS_SETTINGS_RENDER_INTERNAL_H
#define REACH_APPS_SETTINGS_RENDER_INTERNAL_H

#include "reach/apps/settings/settings.h"
#include "reach/core/render_commands.h"
#include "reach/features/common/ui_controls.h"

float reach_settings_scale(const reach_settings_render_input *input, float value);
reach_color reach_settings_color_with_alpha(reach_color color, float alpha);

void reach_settings_push_rect(reach_render_command_buffer *commands, reach_rect_f32 rect,
                              float radius, reach_color color);
void reach_settings_push_masked_rect(reach_render_command_buffer *commands, reach_rect_f32 rect,
                                     float radius, int32_t corner_mask, reach_color color);
void reach_settings_push_bordered_background(reach_render_command_buffer *commands,
                                             reach_rect_f32 rect, float radius, float width,
                                             reach_color background, reach_color border);
void reach_settings_push_text(reach_render_command_buffer *commands, reach_rect_f32 rect,
                              const uint16_t *text, float size, int32_t weight, int32_t alignment,
                              reach_color color, int32_t ellipsis);
void reach_settings_push_icon(reach_render_command_buffer *commands, reach_rect_f32 rect,
                              reach_color color, reach_vector_icon_id icon_id, float inset_ratio);
void reach_settings_push_app_icon(reach_render_command_buffer *commands, reach_rect_f32 rect,
                                  uint64_t icon_id, float alpha);

reach_ui_button_style reach_settings_button_style(const reach_settings_render_input *input,
                                                  reach_color background);
reach_ui_button_style reach_settings_muted_button_style(const reach_settings_render_input *input);
reach_ui_selection_item_style reach_settings_pill_style(const reach_settings_render_input *input,
                                                        reach_color accent);

void reach_settings_render_wifi_page(const reach_settings_render_input *input,
                                     reach_render_command_buffer *commands);
void reach_settings_render_bluetooth_page(const reach_settings_render_input *input,
                                          reach_render_command_buffer *commands);

#endif
