#include "meta.h"
#include "vis.h"

#include "tinyfiledialogs.h"

#include "hh.h"

#include "time_sys.h"
#include "sched.h"
#include "out.h"

#define FLAGS NK_WINDOW_BORDER
#define FLAGS_EDIT NK_EDIT_ALWAYS_INSERT_MODE | NK_EDIT_SELECTABLE | NK_EDIT_AUTO_SELECT

struct vis_layer_run_state {
    char buf_out[1024];
};

void
select_output_file(struct vis_layer_run_state* state) {
    const char* path_dir = hh_path_parent_in_place(hh_path(state->buf_out));
    if(!hh_path_exists(path_dir)) goto select_output_file_failure;
    const char* aFilterPatterns[] = { "*.skd" };
    const char* path = tinyfd_saveFileDialog(
        "sked_opt",
        state->buf_out,
        1,
        aFilterPatterns,
        "Standard Schedule Format (.skd)"
    );
    if(path) {
        if(hh_ends_with(path, ".skd")) strcpy(state->buf_out, path);
        else HH_ERR("Provided output path must end with '.skd' [%s].", path);
    }
    goto select_output_file_free;
select_output_file_failure:
    HH_MSG("Given output path was malformed [%s].", state->buf_out);
select_output_file_free:
    hh_arrfree(path_dir);
}

void
vis_layer_run_update(glenv_Panel* const panel, RGFW_rect original, RGFW_rect curr, void* data) {
    unsigned int pixel_offset = (unsigned int) ((float) original.w * VIS_SKD_RATIO);
    if(Vis_expanded(data, original, curr)) pixel_offset *= 2;
    glenv_Panel_config(panel, .width = glenv_PanelWidth_dynamic(1.f, pixel_offset));
}

void 
vis_layer_run_layout(void* const data, struct nk_context* ctx, float row_height) {
    struct vis_layer_run_state* state = data;
    nk_layout_row_begin(ctx, NK_DYNAMIC, row_height * 2.5f, 2);
    nk_layout_row_push(ctx, 0.75f); // output directory
    if(nk_group_begin(ctx, "group_out_path", NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, row_height, 1);
        nk_label(ctx, "out path", NK_TEXT_LEFT);
        nk_layout_row_begin(ctx, NK_DYNAMIC, row_height + 1.f, 2);
        nk_layout_row_push(ctx, 0.8f);
        nk_edit_string_zero_terminated(ctx, NK_EDIT_ALWAYS_INSERT_MODE | NK_EDIT_SELECTABLE, state->buf_out, sizeof(state->buf_out), nk_filter_ascii);
        nk_layout_row_push(ctx, 0.2f);
        if(nk_button_label(ctx, "browse")) select_output_file(state);
        nk_layout_row_end(ctx);
        nk_group_end(ctx);
    }
    nk_layout_row_push(ctx, 0.25f); // run
    double mjd = DateTime_to_mjd(TIME_SYS->start);
    if(nk_group_begin(ctx, "group_sched", NK_WINDOW_NO_SCROLLBAR)) {
        nk_layout_row_dynamic(ctx, row_height, 1);
        nk_label(ctx, "scheduling", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, row_height + 1.f, 1);
        if(mjd < 0.0) nk_widget_disable_begin(ctx);
        if(nk_button_label(ctx, "Start")) {
            char* path = hh_path(state->buf_out);
            char* path_dir = hh_path_parent(path);
            if(hh_path_exists(path_dir) && hh_ends_with(path, ".skd")) {
                // parse the text field back into META
                hh_arrclear(META->path_parent);
                hh_strput(META->path_parent, path_dir);
                hh_arrclear(META->name);
                hh_strput(META->name, (char*) hh_path_name(path));
                // remove extension
                for(size_t i = 5; i > 0; --i) HH_ASSERT(hh_arrpop(META->name) == (".skd")[i - 1], "Unreachable!");
                hh_arrput(META->name, '\0');
                // TODO: The following 4 lines are duplicated in `args` in main.c
                // consider refactoring
                Sched* skd = Sched_init(SCHED_DEFAULT);
                generate_schedule(skd);
                generate_statistics(skd);
                Sched_free(skd);
            } else {
                HH_ERR("Provided output path must end with '.skd' [%s].", state->buf_out);
                const char* path_fix = hh_path_join(hh_path(META->path_parent), meta_file());
                strcpy(state->buf_out, path_fix);
                hh_arrfree(path_fix);
            }
        }
        if(mjd < 0.0) nk_widget_disable_end(ctx);
        nk_group_end(ctx);
    }
    nk_layout_row_end(ctx);
}

bool
Vis_layer_run(Vis* const vis) {
    glenv_Panel* panel = glenv_Panel_init("run", 
        .flags = FLAGS, 
        .rows = VIS_RUN_ROWS, 
        .bottom = nk_true,
        .layout = vis_layer_run_layout);
    if(panel == NULL) return false;
    glenv_Panel_config(panel, .width = glenv_PanelWidth_dynamic(1.f, 0));
    VisDesc desc;
    VisDesc_init(&desc, sizeof(struct vis_layer_run_state), NULL);
    VisDesc_configure_panel(&desc, panel, NULL);
    struct vis_layer_run_state* state = Vis_add_layer(vis, desc);
    // TODO: consider reading name field from XML if provided
    char* path = hh_path(META->path_parent);
    char* path_out = hh_path_join(hh_path(path), "out");
    if(!hh_path_exists(path_out)) {
        hh_arrfree(path_out);
        path_out = path;
    } else hh_arrfree(path);
    path_out = hh_path_join(path_out, meta_file());
    snprintf(state->buf_out, sizeof(state->buf_out), "%s", path_out);
    hh_arrfree(path_out);
    return true;
}
