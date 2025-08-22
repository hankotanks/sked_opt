#include "vis.h"

#include <glenv.h>
#include <stdlib.h>

#include "hh.h"

#include "time_sys.h"

#define FLAGS NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MINIMIZABLE
#define FLAGS_EDIT NK_EDIT_ALWAYS_INSERT_MODE | NK_EDIT_SELECTABLE | NK_EDIT_AUTO_SELECT

struct vis_layer_cfg_state {
    bool defocus;
    char buf_yrs[5], buf_day[3], buf_hrs[3], buf_min[3];
    char buf_len[4];
    char buf_dur[32];
};

void
vis_layer_cfg_update(glenv_Panel* const panel, RGFW_rect original, RGFW_rect curr, void* data) {
    unsigned int pixel_offset = (unsigned int) ((float) original.w * VIS_SKD_RATIO);
    if(Vis_expanded(data, original, curr)) pixel_offset *= 2;
    glenv_Panel_config(panel, .width = glenv_PanelWidth_dynamic(1.f, pixel_offset));
}

int
nk_filter_decimal_abs(const struct nk_text_edit *edit, unsigned int c) {
    if(c == '-') return 0;
    return nk_filter_decimal(edit, c);
}

bool 
vis_layer_cfg_field(struct nk_context* ctx, char* const buf, size_t len, bool* defocus, nk_plugin_filter filter) {
    nk_flags result = nk_edit_string_zero_terminated(ctx, FLAGS_EDIT | NK_EDIT_AUTO_SELECT, buf, (int) len, filter);
    if(result & NK_EDIT_DEACTIVATED) return true;
    if((result & NK_EDIT_ACTIVE) && nk_input_is_key_pressed(&ctx->input, NK_KEY_ENTER)) {
        *defocus = true;
        return true;
    }
    return false;
}

void 
vis_layer_cfg_layout(void* const data, struct nk_context* ctx, float row_height) {
    struct vis_layer_cfg_state* state = data;
    nk_layout_row_dynamic(ctx, row_height * 2.5f, 3);
    if(nk_group_begin(ctx, "group_start_date", NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, row_height, 1);
        nk_label(ctx, "start date", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, row_height + 1.f, 3);
        if(vis_layer_cfg_field(ctx, state->buf_yrs, sizeof(state->buf_yrs), &state->defocus, nk_filter_decimal_abs)) {
            DateTime start_temp = time_sys->start;
            start_temp.yrs = (size_t) atoi(state->buf_yrs);
            if(DateTime_to_mjd(start_temp) < 0.0) 
                snprintf(state->buf_yrs, sizeof(state->buf_yrs), "%zu", time_sys->start.yrs);
            else time_sys->start.yrs = start_temp.yrs;
        }
        time_sys->start.mon = (enum month) (nk_combo(ctx, months, sizeof(months) / sizeof(months[0]), 
            (int) time_sys->start.mon - 1, (int) row_height, nk_vec2(200.f, row_height * 6.f)) + 1);    
        if(vis_layer_cfg_field(ctx, state->buf_day, sizeof(state->buf_day), &state->defocus, nk_filter_decimal_abs)) {
            size_t day = (size_t) atoi(state->buf_day);
            if(day == 0 || day > months_count_days(time_sys->start.yrs, time_sys->start.mon)) 
                snprintf(state->buf_day, sizeof(state->buf_day), "%zu", time_sys->start.day);
            else time_sys->start.day = day;
        }
        nk_group_end(ctx);
    }
    if(nk_group_begin(ctx, "group_start_time", NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, row_height, 1);
        nk_label(ctx, "start time", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, row_height + 1.f, 2);
        if(vis_layer_cfg_field(ctx, state->buf_hrs, sizeof(state->buf_hrs), &state->defocus, nk_filter_decimal_abs)) {
            size_t hrs = (size_t) atoi(state->buf_hrs);
            if((hrs == 0 && strlen(state->buf_hrs) == 0) || hrs > 23) 
                snprintf(state->buf_hrs, sizeof(state->buf_hrs), "%zu", time_sys->start.hrs);
            else time_sys->start.hrs = hrs;
        }
        if(vis_layer_cfg_field(ctx, state->buf_min, sizeof(state->buf_min), &state->defocus, nk_filter_decimal_abs)) {
            size_t min = (size_t) atoi(state->buf_min);
            if((min == 0 && strlen(state->buf_min) == 0) || min > 59) 
                snprintf(state->buf_min, sizeof(state->buf_min), "%zu", time_sys->start.min);
            else time_sys->start.min = min;
        }
        nk_group_end(ctx);
    }
    char* temp;
    if(nk_group_begin(ctx, "group_duration", NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, row_height, 1);
        nk_label(ctx, "duration [h]", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, row_height + 1.f, 1);
        if(vis_layer_cfg_field(ctx, state->buf_dur, sizeof(state->buf_dur), &state->defocus, nk_filter_float)) {
            unsigned int duration = (unsigned int) (strtod(state->buf_dur, &temp) * 3600.0);
            if(duration < time_sys->scan_length) {
                snprintf(state->buf_dur, sizeof(state->buf_dur), "%u", time_sys->duration);
                HH_MSG("Schedule duration must be greater than the scan length.");
            } else time_sys->duration = duration;
        }
        nk_group_end(ctx);
    }
    if(nk_group_begin(ctx, "group_final_date", NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, row_height, 1);
        nk_label(ctx, "final date", NK_TEXT_LEFT);
        nk_label(ctx, time_sys_text(time_sys->duration), NK_TEXT_LEFT);
        nk_group_end(ctx);
    }
    if(nk_group_begin(ctx, "group_scan_length", NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, row_height, 1);
        nk_label(ctx, "scan length [s]", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, row_height + 1.f, 1);
        if(vis_layer_cfg_field(ctx, state->buf_len, sizeof(state->buf_len), &state->defocus, nk_filter_decimal_abs)) {
            unsigned int scan_length = (unsigned int) strtoul(state->buf_len, &temp, 10);
            if(scan_length == 0 || scan_length >= time_sys->duration) {
                snprintf(state->buf_len, sizeof(state->buf_len), "%u", time_sys->scan_length);
                HH_MSG("Scan length must be less than the total schedule duration.");
            } else time_sys->scan_length = scan_length;
        }
        nk_group_end(ctx);
    }
    if(nk_group_begin(ctx, "group_scan_blocks", NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, row_height, 1);
        nk_label(ctx, "scan blocks", NK_TEXT_LEFT);
        nk_labelf(ctx, NK_TEXT_LEFT, "%u", time_sys->duration / time_sys->scan_length);
        nk_group_end(ctx);
    }
    if(state->defocus) {
        nk_edit_focus(ctx, NK_EDIT_AUTO_SELECT);
        state->defocus = false;
    }
}

bool
Vis_layer_cfg(Vis* const vis) {
    glenv_Panel* panel = glenv_Panel_init("config", 
        .flags = FLAGS, 
        .rows = VIS_CFG_ROWS, 
        .layout = vis_layer_cfg_layout, 
        .update = vis_layer_cfg_update);
    if(panel == NULL) return false;
    VisDesc desc;
    VisDesc_init(&desc, sizeof(struct vis_layer_cfg_state), NULL);
    VisDesc_configure_panel(&desc, panel, NULL);
    struct vis_layer_cfg_state* state = Vis_add_layer(vis, desc);
    if(state == NULL) return false;
    snprintf(state->buf_yrs, sizeof(state->buf_yrs), "%zu", time_sys->start.yrs);
    snprintf(state->buf_day, sizeof(state->buf_day), "%zu", time_sys->start.day);
    snprintf(state->buf_hrs, sizeof(state->buf_hrs), "%zu", time_sys->start.hrs);
    snprintf(state->buf_min, sizeof(state->buf_min), "%zu", time_sys->start.min);
    snprintf(state->buf_dur, sizeof(state->buf_dur), "%lf", (double) time_sys->duration / 3600.0);
    // length of scan in seconds
    snprintf(state->buf_len, sizeof(state->buf_len), "%u", time_sys->scan_length);
    return true;
}
