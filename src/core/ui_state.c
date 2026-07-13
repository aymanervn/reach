#include "reach/core/ui_state.h"

void reach_dock_model_defaults(reach_dock_model *dock)
{
    if (dock == 0)
    {
        return;
    }

    dock->height = 64.0f;
    dock->width = 560.0f;
    dock->icon_size = 40.0f;
    dock->gap = 12.0f;
    dock->visible = 1;
    dock->auto_hide = 1;
}
