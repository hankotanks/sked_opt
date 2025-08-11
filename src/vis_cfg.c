#include "vis.h"

#include <stdlib.h>

#include "hh.h"

#include "time_sys.h"

#define FLAGS NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MINIMIZABLE
#define FLAGS_EDIT NK_EDIT_ALWAYS_INSERT_MODE | NK_EDIT_SELECTABLE | NK_EDIT_AUTO_SELECT

struct vis_layer_cfg_state {
    char buf_yrs[5], buf_day[3], buf_hrs[3], buf_min[3];
    char buf_dur[32];
};

void 
vis_layer_cfg_layout(void* const data, struct nk_context* ctx, float row_height) {
    struct vis_layer_cfg_state* state = data;
    nk_layout_row_dynamic(ctx, row_height, 1);
    nk_label(ctx, "start date", NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, row_height + 1.f, 3);
    ctx->style.edit.text_normal = nk_rgb(255, 255, 255);
    ctx->style.edit.text_active = nk_rgb(255, 255, 255);
    ctx->style.edit.text_hover  = nk_rgb(255, 255, 255);
    if(nk_edit_string_zero_terminated(ctx, FLAGS_EDIT, state->buf_yrs, sizeof(state->buf_yrs), nk_filter_decimal))
        time_sys->start.yrs = (size_t) atoi(state->buf_yrs);
    time_sys->start.mon = (enum month) (nk_combo(ctx, months, sizeof(months) / sizeof(months[0]), 
        (int) time_sys->start.mon, (int) row_height, nk_vec2(200.f, row_height * 6.f)));    
    if(nk_edit_string_zero_terminated(ctx, FLAGS_EDIT, state->buf_day, sizeof(state->buf_day), nk_filter_decimal))
        time_sys->start.day = (size_t) atoi(state->buf_day);
    nk_layout_row_dynamic(ctx, row_height, 1);
    nk_label(ctx, "start time", NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, row_height + 1.f, 2);
    if(nk_edit_string_zero_terminated(ctx, FLAGS_EDIT, state->buf_hrs, sizeof(state->buf_hrs), nk_filter_decimal))
        time_sys->start.hrs = (size_t) atoi(state->buf_hrs);
    if(nk_edit_string_zero_terminated(ctx, FLAGS_EDIT, state->buf_min, sizeof(state->buf_min), nk_filter_decimal))
        time_sys->start.min = (size_t) atoi(state->buf_min);
    nk_layout_row_dynamic(ctx, row_height, 2);
    double mjd = DateTime_to_mjd(time_sys->start);
    nk_labelf_colored(ctx, NK_TEXT_LEFT, 
        (mjd < 0.0) ? nk_rgb(255, 0, 0) : ctx->style.text.color, 
        (mjd < 0.0) ? "mjd: invalid" : "mjd: %lf", mjd);
    { // TODO: consider refactoring out to a glenv.h function
        nk_layout_row_dynamic(ctx, 1.f, 1);
        nk_spacing(ctx, 1);
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
        struct nk_rect bounds = nk_layout_widget_bounds(ctx);
        nk_stroke_line(canvas, 
            bounds.x, bounds.y + bounds.h / 2 - row_height * 0.2f, 
            bounds.x + bounds.w, bounds.y + bounds.h / 2 - row_height * 0.2f, 
            1.f, nk_rgb(128,128,128)
        );
    }
    nk_layout_row_dynamic(ctx, row_height, 1);
    nk_label(ctx, "duration [h]", NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, row_height + 1.f, 1);
    char* temp;
    if(nk_edit_string_zero_terminated(ctx, FLAGS_EDIT, state->buf_dur, sizeof(state->buf_dur), nk_filter_float)) 
        time_sys->duration = (unsigned int) (strtod(state->buf_dur, &temp) * 60.0 * 60.0);
    nk_layout_row_dynamic(ctx, row_height, 1);
    DateTime dt_end = DateTime_from_mjd(mjd + time_sys->duration / 86400.0);
    nk_labelf(ctx, NK_TEXT_LEFT, "final date: %.4zu-%s-%.2zu %.2zu:%.2zu:%02.0lf",
        dt_end.yrs, months[(size_t) dt_end.mon], dt_end.day, dt_end.hrs, dt_end.min, dt_end.sec);
}

bool
Vis_layer_cfg(Vis* const vis) {
    glenv_Panel* panel = glenv_Panel_init("config", FLAGS, vis_layer_cfg_layout);
    if(panel == NULL) return false;
    glenv_Panel_config_right(panel, 9, 250.f);
    VisLayerDesc desc;
    VisLayerDesc_init(&desc, sizeof(struct vis_layer_cfg_state));
    VisLayerDesc_configure_panel(&desc, panel, NULL);
    struct vis_layer_cfg_state* state = Vis_add_layer(vis, desc);
    if(state == NULL) return false;
    snprintf(state->buf_yrs, sizeof(state->buf_yrs), "%zu", time_sys->start.yrs);
    snprintf(state->buf_day, sizeof(state->buf_day), "%zu", time_sys->start.day);
    snprintf(state->buf_hrs, sizeof(state->buf_hrs), "%zu", time_sys->start.hrs);
    snprintf(state->buf_min, sizeof(state->buf_min), "%zu", time_sys->start.min);
    snprintf(state->buf_dur, sizeof(state->buf_dur), "%g", (double) time_sys->duration / 3600.0);
    return true;
}
