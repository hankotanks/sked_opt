#include "vis.h"

#include "hh.h"

#include "time_sys.h"

#define FLAGS NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MINIMIZABLE

void 
vis_layer_cfg_layout(void* const data, struct nk_context* ctx, float row_height) {
    (void) data;
    DateTime dt = time_sys_to_datetime(0);
    nk_layout_row_dynamic(ctx, row_height, 2);
    nk_label(ctx, "year", NK_TEXT_CENTERED | NK_TEXT_ALIGN_MIDDLE);
    nk_labelf(ctx, NK_TEXT_CENTERED | NK_TEXT_ALIGN_MIDDLE, "%zu", dt.year);
    nk_label(ctx, "month", NK_TEXT_CENTERED | NK_TEXT_ALIGN_MIDDLE);
    const char* month;
    switch(dt.month) {
    case JAN: month = "JAN"; break;
    case FEB: month = "FEB"; break;
    case MAR: month = "MAR"; break;
    case APR: month = "APR"; break;
    case MAY: month = "MAY"; break;
    case JUN: month = "JUN"; break;
    case JUL: month = "JUL"; break;
    case AUG: month = "AUG"; break;
    case SEP: month = "SEP"; break;
    case OCT: month = "OCT"; break;
    case NOV: month = "NOV"; break;
    case DEC: month = "DEC"; break;
    default: HH_UNREACHABLE;
    }
    nk_labelf(ctx, NK_TEXT_CENTERED | NK_TEXT_ALIGN_MIDDLE, "%s", month);
    nk_label(ctx, "day", NK_TEXT_CENTERED | NK_TEXT_ALIGN_MIDDLE);
    nk_labelf(ctx, NK_TEXT_CENTERED | NK_TEXT_ALIGN_MIDDLE, "%zu", dt.day);
    nk_label(ctx, "hour", NK_TEXT_CENTERED | NK_TEXT_ALIGN_MIDDLE);
    nk_labelf(ctx, NK_TEXT_CENTERED | NK_TEXT_ALIGN_MIDDLE, "%zu", dt.hour);
    nk_label(ctx, "min", NK_TEXT_CENTERED | NK_TEXT_ALIGN_MIDDLE);
    nk_labelf(ctx, NK_TEXT_CENTERED | NK_TEXT_ALIGN_MIDDLE, "%zu", dt.min);
    nk_label(ctx, "sec", NK_TEXT_CENTERED | NK_TEXT_ALIGN_MIDDLE);
    nk_labelf(ctx, NK_TEXT_CENTERED | NK_TEXT_ALIGN_MIDDLE, "%lf", dt.sec);
}

bool
Vis_layer_cfg(Vis* const vis) {
    glenv_Panel* panel = glenv_Panel_init("config", FLAGS, vis_layer_cfg_layout);
    if(panel == NULL) return false;
    glenv_Panel_config_left_ratio(panel, 5, 0.2f);
    VisLayerDesc desc;
    VisLayerDesc_init(&desc, 0);
    VisLayerDesc_configure_panel(&desc, panel, "sources");
    Vis_add_layer(vis, desc);
    return true;
}
