#include "vis.h"

#include <stdbool.h>

#include <glenv.h>

#include "station.h"
#include "network.h"
#include "source.h"
#include "sky.h"

const GLfloat COLOR_NORMAL_STA[3] = { 0.4f,  0.4f, 0.6f };
const GLfloat COLOR_ACTIVE_STA[3] = { 0.2f,  0.8f,  1.f };
const GLfloat COLOR_NORMAL_SRC[3] = {  1.f,   1.f,  1.f };
const GLfloat COLOR_ACTIVE_SRC[3] = {  1.f,  0.4f, 0.4f };

#define FLAGS NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MINIMIZABLE
#define FLAGS_EDIT NK_EDIT_ALWAYS_INSERT_MODE | NK_EDIT_SELECTABLE | NK_EDIT_AUTO_SELECT

static const char* shader_source_stations = \
    "#version 330 core\n"
    "flat in uint state;\n"
    "uniform vec3 color_normal;\n"
    "uniform vec3 color_active;\n"
    "out vec4 color_out;\n"
    "void main() {\n"
    "    color_out = vec4((state != 0u) ? color_active : color_normal, 1.f);\n"
    "}\n";

struct vis_layer_skd_state {
    GLuint VAO, VBO;
    size_t vertex_count;
    char buf_filter[9];
};

// TODO: Implement mouse picking here
bool 
vis_layer_skd_events(void* const data, const RGFW_window* const win) {
    (void) data;
    (void) win;
    return false;
}

void 
vis_layer_skd_render(const void* const data) {
    const struct vis_layer_skd_state* state = data;
    glEnable(GL_DEPTH_TEST);
    glPointSize(5.f);
    glBindVertexArray(state->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, state->VBO);  
    glDrawArrays(GL_POINTS, 0, (GLsizei) state->vertex_count);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void 
vis_layer_skd_deinit(void* const data) {
    struct vis_layer_skd_state* state = data;
    glDeleteVertexArrays(1, &state->VAO);
    glDeleteBuffers(1, &state->VBO);
}

void
vis_layer_net_update(glenv_Panel* panel, RGFW_rect original, RGFW_rect curr, void* data) {
    unsigned int pixels = (unsigned int) ((float) original.w * VIS_SKD_RATIO);
    glenv_Panel_config(panel, 
        .width = glenv_PanelWidth_fixed(pixels),
        .offset = Vis_expanded(data, original, curr) ? (float) original.w * VIS_SKD_RATIO : 0.f);
}

void 
vis_layer_net_layout(void* const data, struct nk_context* ctx, float row_height) {
    struct vis_layer_skd_state* state = data;
    nk_layout_row_dynamic(ctx, row_height + 1.f, 1);
    // pinned search bar
    nk_edit_string_zero_terminated(ctx, FLAGS_EDIT, state->buf_filter, sizeof(state->buf_filter), nk_filter_ascii);
    nk_layout_row_dynamic(ctx, row_height, 2);
    const Station* sta;
    bool* active;
    size_t i = 0;
    net_it(sta) {
        active = net_sta_active(sta->id);
        if(state->buf_filter[0] != '\0' && !cat_name_contains(sta->name, state->buf_filter)) continue;
        int temp = *active ? nk_false : nk_true;
        if(nk_check_text(ctx, sta->name, (int) cat_name_len(sta->name), temp) != temp) {
            *active = !(*active);
            GLfloat val = (*active) ? 1.f : 0.f;
            glBindBuffer(GL_ARRAY_BUFFER, state->VBO);
            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr) ((i * 4 + 3) * sizeof(GLfloat)), sizeof(GLfloat), &val);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        nk_labelf(ctx, NK_TEXT_CENTERED | NK_TEXT_ALIGN_MIDDLE, "[%c%c]", sta->id[0], sta->id[1]);
        i++;
    }
}

bool
Vis_layer_net(Vis* const vis) {
    GLuint frag = shader_compile_from_source(GL_FRAGMENT_SHADER, shader_source_stations);
    if(!frag) return false;
    // build station vertices
    GLfloat* vertices = NULL;
    const Station* sta;
    net_it(sta) {
        hh_arrput(vertices, (GLfloat) sta->lon);
        hh_arrput(vertices, (GLfloat) (90.0 - sta->lat));
        hh_arrput(vertices, 0.f);
        hh_arrput(vertices, *net_sta_active(sta->id) ? 1.f : 0.f);
    }
    // configure layer's corresponding UI element
    glenv_Panel* panel = glenv_Panel_init("stations", 
        .flags = FLAGS, 
        .rows = VIS_SKD_ROWS, 
        .right = nk_true,
        .layout = vis_layer_net_layout, 
        .update = vis_layer_net_update);
    if(panel == NULL) return false;
    // allocate data
    VisDesc desc;
    VisDesc_init(&desc, sizeof(struct vis_layer_skd_state), vis_layer_skd_deinit);
    VisDesc_configure_pass(&desc, frag, vis_layer_skd_events, vis_layer_skd_render);
    VisDesc_configure_panel(&desc, panel, NULL);
    struct vis_layer_skd_state* state = Vis_add_layer(vis, desc);
    state->vertex_count = hh_arrlen(vertices) / 4;
    state->buf_filter[0] = '\0';
    // set uniform locations
    GLint program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glUniform3f(glGetUniformLocation((GLuint) program, "color_normal"), COLOR_NORMAL_STA[0], COLOR_NORMAL_STA[1], COLOR_NORMAL_STA[2]);
    glUniform3f(glGetUniformLocation((GLuint) program, "color_active"), COLOR_ACTIVE_STA[0], COLOR_ACTIVE_STA[1], COLOR_ACTIVE_STA[2]);
    // configure buffers
    glGenVertexArrays(1, &state->VAO);
    glGenBuffers(1, &state->VBO);
    glBindVertexArray(state->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, state->VBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) (hh_arrlen(vertices) * sizeof(GLfloat)), vertices, GL_STATIC_DRAW);
    hh_arrfree(vertices);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (GLvoid*) 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    return true;
}

void
vis_layer_sky_update(glenv_Panel* panel, RGFW_rect original, RGFW_rect curr, void* data) {
    Vis* vis = data;
    unsigned int pixels = (unsigned int) ((float) original.w * VIS_SKD_RATIO);
    glenv_Panel_config(panel, .width = glenv_PanelWidth_fixed(pixels));
    if(Vis_expanded(vis, original, curr)) {
        glenv_Panel* parent = Vis_get_panel(vis, "run");
        HH_ASSERT(parent != NULL, "Unable to find panel 'run'.");
        glenv_Panel_config(panel, .parent = NULL);
    } else if(glenv_Panel_get_config(panel).parent == NULL) {
        glenv_Panel* parent = Vis_get_panel(vis, "stations");
        HH_ASSERT(parent != NULL, "Unable to find panel 'stations'.");
        glenv_Panel_config(panel, .parent = parent);
    }
}

void 
vis_layer_sky_layout(void* const data, struct nk_context* ctx, float row_height) {
    struct vis_layer_skd_state* state = data;
    nk_layout_row_dynamic(ctx, row_height + 1.f, 1);
    // pinned search bar
    nk_edit_string_zero_terminated(ctx, FLAGS_EDIT, state->buf_filter, sizeof(state->buf_filter), nk_filter_ascii);
    const Source* src;
    bool* active;
    size_t i = 0, j;
    sky_it(src) {
        nk_layout_row_begin(ctx, NK_DYNAMIC, row_height, 1 + (int) BAND_OTHER);
        nk_layout_row_push(ctx, 0.5f);
        active = sky_src_active(src->name);
        if(state->buf_filter[0] != '\0' && !cat_name_contains(src->name, state->buf_filter)) continue;
        int temp = *active ? nk_false : nk_true;
        if(nk_check_text(ctx, src->name, (int) cat_name_len(src->name), temp) != temp) {
            *active = !(*active);
            GLfloat val = (*active) ? 1.f : 0.f;
            glBindBuffer(GL_ARRAY_BUFFER, state->VBO);
            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr) ((i * 4 + 3) * sizeof(GLfloat)), sizeof(GLfloat), &val);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        for(j = 0; j < (size_t) BAND_OTHER; ++j) {
            nk_layout_row_push(ctx, 0.5f / (float) BAND_OTHER);
            if(src->band[j]) {
                nk_labelf(ctx, NK_TEXT_CENTERED | NK_TEXT_ALIGN_MIDDLE, "%c", BAND_CODES[j]);
            } else {
                nk_spacer(ctx);
            }
        }
        nk_layout_row_end(ctx);
        i++;
    }
}

bool
Vis_layer_sky(Vis* const vis) {
    GLuint frag = shader_compile_from_source(GL_FRAGMENT_SHADER, shader_source_stations);
    if(!frag) return false;
    // build station vertices
    GLfloat* vertices = NULL;
    const Source* src;
    sky_it(src) {
        hh_arrput(vertices, (GLfloat) src->raan);
        hh_arrput(vertices, (GLfloat) (90.0 - src->decl));
        hh_arrput(vertices, 1.f);
        hh_arrput(vertices, *sky_src_active(src->name) ? 1.f : 0.f);
    }
    // configure layer's corresponding UI element
    glenv_Panel* panel = glenv_Panel_init("sources", 
        .flags = FLAGS, 
        .rows = VIS_SKD_ROWS, 
        .right = nk_true, 
        .layout = vis_layer_sky_layout, 
        .update = vis_layer_sky_update);
    if(panel == NULL) return false;
    // allocate data
    VisDesc desc;
    VisDesc_init(&desc, sizeof(struct vis_layer_skd_state), vis_layer_skd_deinit);
    VisDesc_configure_pass(&desc, frag, vis_layer_skd_events, vis_layer_skd_render);
    VisDesc_configure_panel(&desc, panel, "stations");
    struct vis_layer_skd_state* state = Vis_add_layer(vis, desc);
    // set vertex_count
    state->vertex_count = hh_arrlen(vertices) / 4;
    state->buf_filter[0] = '\0';
    // set uniform locations
    GLint program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glUniform3f(glGetUniformLocation((GLuint) program, "color_normal"), COLOR_NORMAL_SRC[0], COLOR_NORMAL_SRC[1], COLOR_NORMAL_SRC[2]);
    glUniform3f(glGetUniformLocation((GLuint) program, "color_active"), COLOR_ACTIVE_SRC[0], COLOR_ACTIVE_SRC[1], COLOR_ACTIVE_SRC[2]);
    // configure buffers
    glGenVertexArrays(1, &state->VAO);
    glGenBuffers(1, &state->VBO);
    glBindVertexArray(state->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, state->VBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) (hh_arrlen(vertices) * sizeof(GLfloat)), vertices, GL_STATIC_DRAW);
    hh_arrfree(vertices);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (GLvoid*) 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    return true;
}

