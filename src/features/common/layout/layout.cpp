#include "reach/features/common/layout.h"

static uint32_t reach_layout_condition_bit(reach_layout_condition condition)
{
    return (uint32_t)1u << (uint32_t)condition;
}

static int32_t reach_layout_condition_valid(reach_layout_condition condition)
{
    return (uint32_t)condition < (uint32_t)REACH_LAYOUT_CONDITION_COUNT;
}

static reach_layout_participant_state *
reach_layout_participant_at(reach_layout *layout, reach_layout_participant participant)
{
    if (layout == nullptr || (size_t)participant >= layout->participant_count)
    {
        return nullptr;
    }
    return &layout->participants[participant];
}

reach_result reach_layout_register(reach_layout *layout, int32_t base_layer,
                                   reach_layout_participant *out_participant)
{
    if (layout == nullptr || out_participant == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (layout->participant_count >= REACH_LAYOUT_MAX_PARTICIPANTS)
    {
        return REACH_ERROR;
    }

    reach_layout_participant_state *state = &layout->participants[layout->participant_count];
    *state = {};
    state->base_layer = base_layer;
    state->wants_visible = 1;

    *out_participant = (reach_layout_participant)layout->participant_count;
    ++layout->participant_count;
    return REACH_OK;
}

reach_result reach_layout_register_override(reach_layout *layout,
                                            reach_layout_participant participant,
                                            reach_layout_condition condition, int32_t layer)
{
    reach_layout_participant_state *state = reach_layout_participant_at(layout, participant);
    if (state == nullptr || !reach_layout_condition_valid(condition))
    {
        return REACH_INVALID_ARGUMENT;
    }

    state->layer_conditions |= reach_layout_condition_bit(condition);
    state->layer_overrides[condition] = layer;
    return REACH_OK;
}

reach_result reach_layout_register_visibility(reach_layout *layout,
                                              reach_layout_participant participant,
                                              reach_layout_condition condition, int32_t visible)
{
    reach_layout_participant_state *state = reach_layout_participant_at(layout, participant);
    if (state == nullptr || !reach_layout_condition_valid(condition))
    {
        return REACH_INVALID_ARGUMENT;
    }

    state->visibility_conditions |= reach_layout_condition_bit(condition);
    state->visibility_overrides[condition] = visible ? 1 : 0;
    return REACH_OK;
}

void reach_layout_set_condition(reach_layout *layout, reach_layout_condition condition,
                                int32_t active)
{
    if (layout == nullptr || !reach_layout_condition_valid(condition))
    {
        return;
    }

    uint32_t bit = reach_layout_condition_bit(condition);
    if (active)
    {
        layout->active_conditions |= bit;
    }
    else
    {
        layout->active_conditions &= ~bit;
    }
}

void reach_layout_set_layer_intent(reach_layout *layout, reach_layout_participant participant,
                                   int32_t active, int32_t layer)
{
    reach_layout_participant_state *state = reach_layout_participant_at(layout, participant);
    if (state == nullptr)
    {
        return;
    }
    state->layer_intent_active = active ? 1 : 0;
    state->layer_intent = layer;
}

void reach_layout_set_visible(reach_layout *layout, reach_layout_participant participant,
                              int32_t wants_visible)
{
    reach_layout_participant_state *state = reach_layout_participant_at(layout, participant);
    if (state != nullptr)
    {
        state->wants_visible = wants_visible ? 1 : 0;
    }
}

static reach_layout_entry reach_layout_resolve_participant(const reach_layout *layout, size_t index)
{
    const reach_layout_participant_state *state = &layout->participants[index];

    reach_layout_entry entry = {};
    entry.participant = (reach_layout_participant)index;
    entry.layer = state->base_layer;
    entry.visible = state->wants_visible ? 1 : 0;

    int32_t layer_overridden = 0;
    int32_t visibility_overridden = 0;

    for (uint32_t condition = 0; condition < (uint32_t)REACH_LAYOUT_CONDITION_COUNT; ++condition)
    {
        uint32_t bit = (uint32_t)1u << condition;
        if ((layout->active_conditions & bit) == 0)
        {
            continue;
        }
        if ((state->layer_conditions & bit) != 0)
        {
            int32_t candidate = state->layer_overrides[condition];
            if (!layer_overridden || candidate > entry.layer)
            {
                entry.layer = candidate;
                layer_overridden = 1;
            }
        }
        if ((state->visibility_conditions & bit) != 0)
        {
            int32_t candidate = state->visibility_overrides[condition];
            if (!visibility_overridden || candidate < entry.visible)
            {
                entry.visible = candidate;
                visibility_overridden = 1;
            }
        }
    }

    if (state->layer_intent_active && (!layer_overridden || state->layer_intent > entry.layer))
    {
        entry.layer = state->layer_intent;
    }

    return entry;
}

void reach_layout_resolve(const reach_layout *layout, reach_layout_plan *out_plan)
{
    if (out_plan == nullptr)
    {
        return;
    }

    *out_plan = {};
    if (layout == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < layout->participant_count; ++index)
    {
        reach_layout_entry entry = reach_layout_resolve_participant(layout, index);

        size_t at = out_plan->count;
        while (at > 0 && out_plan->entries[at - 1].layer < entry.layer)
        {
            out_plan->entries[at] = out_plan->entries[at - 1];
            --at;
        }
        out_plan->entries[at] = entry;
        ++out_plan->count;
    }
}

int32_t reach_layout_plan_equal(const reach_layout_plan *left, const reach_layout_plan *right)
{
    if (left == nullptr || right == nullptr)
    {
        return left == right ? 1 : 0;
    }
    if (left->count != right->count)
    {
        return 0;
    }

    for (size_t index = 0; index < left->count; ++index)
    {
        if (left->entries[index].participant != right->entries[index].participant ||
            left->entries[index].layer != right->entries[index].layer ||
            left->entries[index].visible != right->entries[index].visible)
        {
            return 0;
        }
    }
    return 1;
}
