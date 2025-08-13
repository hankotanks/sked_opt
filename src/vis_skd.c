#include "vis.h"

#include <stdbool.h>

#include <glenv.h>

#include "network.h"
#include "sky.h"

const GLfloat COLOR_NORMAL_STA[3] = { 0.4f,  0.4f, 0.6f };
const GLfloat COLOR_ACTIVE_STA[3] = { 0.2f,  0.8f,  1.f };
const GLfloat COLOR_NORMAL_SRC[3] = {  1.f,   1.f,  1.f };
const GLfloat COLOR_ACTIVE_SRC[3] = {  1.f,  0.4f, 0.4f };

#define FLAGS NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MINIMIZABLE

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
vis_layer_net_resize(glenv_Panel* panel, RGFW_rect original, RGFW_rect curr, void* data) {
    (void) data;
    unsigned int pixels = (unsigned int) ((float) original.w * VIS_NET_RATIO);
    bool large = curr.w > (int) ((float) original.w * (VIS_CFG_RATIO + VIS_NET_RATIO + VIS_SKY_RATIO));
    glenv_Panel_config_right(panel, VIS_NET_ROWS, pixels, large ? (unsigned int) ((float) original.w * VIS_SKY_RATIO) : 0);
}

void 
vis_layer_net_layout(void* const data, struct nk_context* ctx, float row_height) {
    struct vis_layer_skd_state* state = data;
    nk_layout_row_dynamic(ctx, row_height, 2);
    Station sta;
    bool* active;
    for(size_t i = 0, j = 0; i < net->count; ++i) {
        active = net_get_sta_by_idx(i, &sta);
        if(active == NULL) continue;
        int temp = *active ? nk_false : nk_true;
        if(nk_check_text(ctx, sta.name, (int) cat_name_len(sta.name), temp) != temp) {
            *active = !(*active);
            GLfloat val = (*active) ? 1.f : 0.f;
            glBindBuffer(GL_ARRAY_BUFFER, state->VBO);
            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr) ((j * 4 + 3) * sizeof(GLfloat)), sizeof(GLfloat), &val);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        nk_labelf(ctx, NK_TEXT_CENTERED | NK_TEXT_ALIGN_MIDDLE, "[%c%c]", sta.id[0], sta.id[1]);
        j++;
    }
}

bool
Vis_layer_net(Vis* const vis) {
    GLuint frag = shader_compile_from_source(GL_FRAGMENT_SHADER, shader_source_stations);
    if(!frag) return false;
    // build station vertices
    GLfloat* vertices = NULL;
    Station sta;
    bool* active;
    for(size_t i = 0; i < net->count; ++i) {
        active = net_get_sta_by_idx(i, &sta);
        if(active == NULL) continue;
        hh_arrput(vertices, (GLfloat) sta.lon);
        hh_arrput(vertices, (GLfloat) (90.0 - sta.lat));
        hh_arrput(vertices, 0.f);
        hh_arrput(vertices, (*active) ? 1.f : 0.f);
    }
    // configure layer's corresponding UI element
    glenv_Panel* panel = glenv_Panel_init("stations", FLAGS, vis_layer_net_layout);
    if(panel == NULL) return false;
    glenv_Panel_set_resize(panel, vis_layer_net_resize);
    // allocate data
    VisLayerDesc desc;
    VisLayerDesc_init(&desc, sizeof(struct vis_layer_skd_state));
    VisLayerDesc_configure_pass(&desc, frag, (VisPassMethods) { vis_layer_skd_events, vis_layer_skd_render, vis_layer_skd_deinit });
    VisLayerDesc_configure_panel(&desc, panel, NULL);
    struct vis_layer_skd_state* state = Vis_add_layer(vis, desc);
    state->vertex_count = hh_arrlen(vertices) / 4;
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
vis_layer_sky_resize(glenv_Panel* panel, RGFW_rect original, RGFW_rect curr, void* data) {
    Vis* vis = data;
    unsigned int pixels = (unsigned int) ((float) original.w * VIS_SKY_RATIO);
    glenv_Panel_config_right(panel, VIS_SKY_ROWS, pixels, 0);
    if(curr.w > (int) ((float) original.w * (VIS_CFG_RATIO + VIS_NET_RATIO + VIS_SKY_RATIO))) {
        glenv_Panel_set_parent(panel, NULL);
    } else {
        glenv_Panel* parent = Vis_get_panel(vis, "stations");
        HH_ASSERT(parent != NULL, "Unable to find panel 'stations'.");
        glenv_Panel_set_parent(panel, parent);
    }
}

void 
vis_layer_sky_layout(void* const data, struct nk_context* ctx, float row_height) {
    struct vis_layer_skd_state* state = data;
    nk_layout_row_dynamic(ctx, row_height, 1);
    Source src;
    bool* active;
    for(size_t i = 0, j = 0; i < net->count; ++i) {
        active = sky_get_src_by_idx(i, &src);
        if(active == NULL) continue;
        int temp = *active ? nk_false : nk_true;
        if(nk_check_text(ctx, src.name, (int) cat_name_len(src.name), temp) != temp) {
            *active = !(*active);
            GLfloat val = (*active) ? 1.f : 0.f;
            glBindBuffer(GL_ARRAY_BUFFER, state->VBO);
            glBufferSubData(GL_ARRAY_BUFFER, (GLintptr) ((j * 4 + 3) * sizeof(GLfloat)), sizeof(GLfloat), &val);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        j++;
    }
}

bool
Vis_layer_sky(Vis* const vis) {
    GLuint frag = shader_compile_from_source(GL_FRAGMENT_SHADER, shader_source_stations);
    if(!frag) return false;
    // build station vertices
    GLfloat* vertices = NULL;
    Source src;
    bool* active;
    for(size_t i = 0; i < sky->count; ++i) {
        active = sky_get_src_by_idx(i, &src);
        if(active == NULL) continue;
        hh_arrput(vertices, (GLfloat) src.raan);
        hh_arrput(vertices, (GLfloat) (90.0 - src.decl));
        hh_arrput(vertices, 1.f);
        hh_arrput(vertices, (*active) ? 1.f : 0.f);
    }
    // configure layer's corresponding UI element
    glenv_Panel* panel = glenv_Panel_init("sources", FLAGS, vis_layer_sky_layout);
    if(panel == NULL) return false;
    glenv_Panel_set_resize(panel, vis_layer_sky_resize);
    // allocate data
    VisLayerDesc desc;
    VisLayerDesc_init(&desc, sizeof(struct vis_layer_skd_state));
    VisLayerDesc_configure_pass(&desc, frag, (VisPassMethods) { vis_layer_skd_events, vis_layer_skd_render, vis_layer_skd_deinit });
    VisLayerDesc_configure_panel(&desc, panel, "stations");
    struct vis_layer_skd_state* state = Vis_add_layer(vis, desc);
    // set vertex_count
    state->vertex_count = hh_arrlen(vertices) / 4;
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

