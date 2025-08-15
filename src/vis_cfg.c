#include "vis.h"

#include <glenv.h>
#include <stdlib.h>

#include "tinyfiledialogs.h"
#include "hh.h"

#include "time_sys.h"

#define FLAGS NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MINIMIZABLE
#define FLAGS_EDIT NK_EDIT_ALWAYS_INSERT_MODE | NK_EDIT_SELECTABLE | NK_EDIT_AUTO_SELECT

struct vis_layer_cfg_state {
    char buf_yrs[5], buf_day[3], buf_hrs[3], buf_min[3];
    char buf_dur[32];
    char buf_out[1024];
};

void
select_output_file(struct vis_layer_cfg_state* state) {
    char* aDefaultPathAndOrFile = NULL;
    hh_strput(aDefaultPathAndOrFile, state->buf_yrs);
    hh_strput(aDefaultPathAndOrFile, months[(int) time_sys->start.mon]);
    hh_strput(aDefaultPathAndOrFile, state->buf_day);
    hh_strput(aDefaultPathAndOrFile, "_");
    hh_strput(aDefaultPathAndOrFile, state->buf_hrs);
    hh_strput(aDefaultPathAndOrFile, state->buf_min);
    hh_strput(aDefaultPathAndOrFile, ".skd");
    const char* aFilterPatterns[] = { "*.skd" };
    const char* path = tinyfd_saveFileDialog(
        "sked_opt",
        aDefaultPathAndOrFile,
        1,
        aFilterPatterns,
        "Standard Schedule File Format"
    );
    hh_arrfree(aDefaultPathAndOrFile);
    if(path) strcpy(state->buf_out, path);
}

void
vis_layer_cfg_update(glenv_Panel* const panel, RGFW_rect original, RGFW_rect curr, void* data) {
    unsigned int pixel_offset = (unsigned int) ((float) original.w * VIS_SKD_RATIO);
    if(Vis_expanded(data, original, curr)) pixel_offset *= 2;
    glenv_Panel_config(panel, .width = glenv_PanelWidth_dynamic(1.f, pixel_offset));
}

void 
vis_layer_cfg_layout(void* const data, struct nk_context* ctx, float row_height) {
    struct vis_layer_cfg_state* state = data;
    nk_layout_row_dynamic(ctx, row_height * 2.5f, 4);
    double mjd = DateTime_to_mjd(time_sys->start);
    if(nk_group_begin(ctx, "group_start_date", NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, row_height, 1);
        nk_label(ctx, "start date", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, row_height + 1.f, 3);
        if(nk_edit_string_zero_terminated(ctx, FLAGS_EDIT, state->buf_yrs, sizeof(state->buf_yrs), nk_filter_decimal))
            time_sys->start.yrs = (size_t) atoi(state->buf_yrs);
        time_sys->start.mon = (enum month) (nk_combo(ctx, months, sizeof(months) / sizeof(months[0]), 
            (int) time_sys->start.mon, (int) row_height, nk_vec2(200.f, row_height * 6.f)));    
        if(nk_edit_string_zero_terminated(ctx, FLAGS_EDIT, state->buf_day, sizeof(state->buf_day), nk_filter_decimal))
            time_sys->start.day = (size_t) atoi(state->buf_day);
        nk_group_end(ctx);
    }
    if(nk_group_begin(ctx, "group_start_time", NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, row_height, 1);
        nk_label(ctx, "start time", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, row_height + 1.f, 2);
        if(nk_edit_string_zero_terminated(ctx, FLAGS_EDIT, state->buf_hrs, sizeof(state->buf_hrs), nk_filter_decimal))
            time_sys->start.hrs = (size_t) atoi(state->buf_hrs);
        if(nk_edit_string_zero_terminated(ctx, FLAGS_EDIT, state->buf_min, sizeof(state->buf_min), nk_filter_decimal))
            time_sys->start.min = (size_t) atoi(state->buf_min);
        nk_group_end(ctx);
    }
    if(nk_group_begin(ctx, "group_duration", NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, row_height, 1);
        nk_label(ctx, "duration", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, row_height + 1.f, 1);
        char* temp;
        if(nk_edit_string_zero_terminated(ctx, FLAGS_EDIT, state->buf_dur, sizeof(state->buf_dur), nk_filter_float)) 
            time_sys->duration = (unsigned int) (strtod(state->buf_dur, &temp) * 60.0 * 60.0);
        nk_group_end(ctx);
    }
    if(nk_group_begin(ctx, "group_mjd", NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, row_height, 1);
        nk_label(ctx, "mjd", NK_TEXT_LEFT);
        nk_labelf_colored(ctx, NK_TEXT_LEFT, 
            (mjd < 0.0) ? nk_rgb(255, 0, 0) : ctx->style.text.color, 
            (mjd < 0.0) ? "invalid" : "%lf", mjd);
        nk_group_end(ctx);
    }
    nk_layout_row_begin(ctx, NK_DYNAMIC, row_height * 2.5f, 2);
    nk_layout_row_push(ctx, 0.75f); // output directory
    if(nk_group_begin(ctx, "group_out_path", NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, row_height, 1);
        nk_label(ctx, "out path", NK_TEXT_LEFT);
        nk_layout_row_begin(ctx, NK_DYNAMIC, row_height + 1.f, 2);
        nk_layout_row_push(ctx, 0.8f);
        if(nk_edit_string_zero_terminated(ctx, NK_EDIT_ALWAYS_INSERT_MODE | NK_EDIT_SELECTABLE, state->buf_out, sizeof(state->buf_out), nk_filter_ascii)) {
            
        }
        nk_layout_row_push(ctx, 0.2f);
        if(nk_button_label(ctx, "browse")) select_output_file(state);
        nk_layout_row_end(ctx);
        nk_group_end(ctx);
    }
    nk_layout_row_push(ctx, 0.25f); // final date
    if(nk_group_begin(ctx, "group_final_date", NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, row_height, 1);
        DateTime dt_end = DateTime_from_mjd(mjd + time_sys->duration / 86400.0);
        nk_label(ctx, "final date", NK_TEXT_LEFT);
        nk_labelf(ctx, NK_TEXT_LEFT, "%.4zu-%s-%.2zu %.2zu:%.2zu:%02.0lf",
            dt_end.yrs, months[(size_t) dt_end.mon], dt_end.day, 
            dt_end.hrs, dt_end.min, dt_end.sec);
        nk_group_end(ctx);
    }
    nk_layout_row_end(ctx);
}

bool
Vis_layer_cfg(Vis* const vis) {
    glenv_Panel* panel = glenv_Panel_init("config", 
        .flags = FLAGS, 
        .rows = VIS_CFG_ROWS, 
        .layout = vis_layer_cfg_layout, 
        .update = vis_layer_cfg_update);
    if(panel == NULL) return false;
    VisLayerDesc desc;
    VisLayerDesc_init(&desc, sizeof(struct vis_layer_cfg_state));
    VisLayerDesc_configure_panel(&desc, panel, NULL);
    struct vis_layer_cfg_state* state = Vis_add_layer(vis, desc);
    if(state == NULL) return false;
    snprintf(state->buf_yrs, sizeof(state->buf_yrs), "%zu", time_sys->start.yrs);
    snprintf(state->buf_day, sizeof(state->buf_day), "%zu", time_sys->start.day);
    snprintf(state->buf_hrs, sizeof(state->buf_hrs), "%zu", time_sys->start.hrs);
    snprintf(state->buf_min, sizeof(state->buf_min), "%zu", time_sys->start.min);
    snprintf(state->buf_dur, sizeof(state->buf_dur), "%lf", (double) time_sys->duration / 3600.0);
    char* path_out = hh_path(".");
    if(!hh_path_exists(path_out)) {
        if(path_out != NULL) hh_arrfree(path_out);
        state->buf_out[0] = '\0';
        return false;
    } else {
        hh_strput(path_out, "/");
        hh_strput(path_out, state->buf_yrs);
        hh_strput(path_out, months[(int) time_sys->start.mon]);
        hh_strput(path_out, state->buf_day);
        hh_strput(path_out, "_");
        hh_strput(path_out, state->buf_hrs);
        hh_strput(path_out, state->buf_min);
        hh_strput(path_out, ".skd");
        snprintf(state->buf_out, sizeof(state->buf_out), "%s", path_out);
    };
    hh_arrfree(path_out);
    return true;
}
