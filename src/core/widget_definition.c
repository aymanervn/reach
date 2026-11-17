#include "reach/core/config.h"

const reach_widget_definition *reach_widget_definition_at(size_t index)
{
    static const reach_widget_definition definitions[REACH_MAX_DESKTOP_WIDGETS] = {
        {(const uint16_t *)u"Clock & date", 0, 244, 144, {220, 136, 360, 190}},
        {(const uint16_t *)u"System monitor", 1, 320, 236, {300, 224, 440, 320}},
        {(const uint16_t *)u"Now playing", 2, 300, 196, {280, 180, 440, 280}},
        {(const uint16_t *)u"Calendar", 3, 300, 300, {280, 278, 420, 390}},
        {(const uint16_t *)u"Stopwatch", 6, 280, 160, {240, 160, 400, 240}},
        {(const uint16_t *)u"Pomodoro", 7, 280, 200, {260, 180, 420, 280}},
    };
    return index < REACH_MAX_DESKTOP_WIDGETS ? &definitions[index] : NULL;
}
