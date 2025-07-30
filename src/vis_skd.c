#include "vis.h"

#include <stdbool.h>

#include <glenv.h>

#include "network.h"
#include "sky.h"

// TODO: remove these includes when I stop randomly setting station/source activity
#include <stdlib.h>
#include <time.h>

const GLfloat COLOR_NORMAL_STA[3] = { 1.f, 0.4f, 0.f };
const GLfloat COLOR_ACTIVE_STA[3] = { 0.2f, 0.8f, 0.2f };
const GLfloat COLOR_NORMAL_SRC[3] = { 1.f, 1.f, 1.f };
const GLfloat COLOR_ACTIVE_SRC[3] = { 1.f, 0.8f, 0.0f };

static const char* shader_source_stations = \
    "#version 330 core\n"
    "flat in uint is_active;\n"
    "out vec4 color_out;\n"
    "uniform vec3 color_normal;\n"
    "uniform vec3 color_active;\n"
    "void main() {\n"
    "    color_out = vec4((is_active != 0u) ? color_normal : color_active, 1.f);\n"
    "}\n";

struct layer_data {
    GLuint VAO, VBO;
    size_t vertex_count;
};

bool 
layer_events(void* const data, const RGFW_window* const win) {
    (void) data;
    (void) win;
    return false;
}

void 
layer_render(const void* const data) {
    const struct layer_data* layer = data;
    glEnable(GL_DEPTH_TEST);
    glPointSize(5.f);
    glBindVertexArray(layer->VAO);    
    glDrawArrays(GL_POINTS, 0, (GLsizei) layer->vertex_count);
}

void 
layer_deinit(void* const data) {
    struct layer_data* layer = data;
    glDeleteVertexArrays(1, &layer->VAO);
    glDeleteBuffers(1, &layer->VBO);
}

bool
Vis_add_stations(Vis* const vis, const Network* const net) {
    // TODO: remove when i stop randomizing
    srand((unsigned int) time(NULL));
    GLuint frag = shader_compile_from_source(GL_FRAGMENT_SHADER, shader_source_stations);
    if(!frag) return false;
    // build station vertices
    GLfloat* vertices = NULL;
    Station* sta;
    bool active;
    for(size_t i = 0; i < net->count; ++i) {
        active = Network_get_sta_by_idx(net, i, &sta);
        // TODO: stop randomly setting active and use user selection
        active = ((bool) (rand() < (RAND_MAX / 2)));
        if(sta == NULL) continue;
        hh_arrput(vertices, (GLfloat) sta->lon);
        hh_arrput(vertices, (GLfloat) (90.0 - sta->lat));
        hh_arrput(vertices, 0.f);
        hh_arrput(vertices, active ? 1.f : 0.f);
    }
    // allocate data
    struct layer_data* data = Vis_add_layer(vis, frag, (VisLayerMethods) {
        .events = layer_events,
        .render = layer_render,
        .deinit = layer_deinit }, sizeof(struct layer_data));
    data->vertex_count = hh_arrlen(vertices) / 4;
    // set uniform locations
    GLint program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glUniform3f(glGetUniformLocation((GLuint) program, "color_normal"), COLOR_NORMAL_STA[0], COLOR_NORMAL_STA[1], COLOR_NORMAL_STA[2]);
    glUniform3f(glGetUniformLocation((GLuint) program, "color_active"), COLOR_ACTIVE_STA[0], COLOR_ACTIVE_STA[1], COLOR_ACTIVE_STA[2]);
    // configure buffers
    glGenVertexArrays(1, &data->VAO);
    glGenBuffers(1, &data->VBO);
    glBindVertexArray(data->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, data->VBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) (hh_arrlen(vertices) * sizeof(GLfloat)), vertices, GL_STATIC_DRAW);
    hh_arrfree(vertices);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (GLvoid*) 0);
    glEnableVertexAttribArray(0);
    return true;
}

bool
Vis_add_sources(Vis* const vis, const Sky* const sky) {
    // TODO: remove when i stop randomizing
    srand((unsigned int) time(NULL));
    GLuint frag = shader_compile_from_source(GL_FRAGMENT_SHADER, shader_source_stations);
    if(!frag) return false;
    // build station vertices
    GLfloat* vertices = NULL;
    Source* src;
    bool active;
    for(size_t i = 0; i < sky->count; ++i) {
        active = Sky_get_src_by_idx(sky, i, &src);
        // TODO: stop randomly setting active and use user selection
        active = ((bool) (rand() < (RAND_MAX / 2)));
        if(src == NULL) continue;
        hh_arrput(vertices, (GLfloat) src->raan);
        hh_arrput(vertices, (GLfloat) (90.0 - src->decl));
        hh_arrput(vertices, 1.f);
        hh_arrput(vertices, active ? 1.f : 0.f);
    }
    // allocate data
    struct layer_data* data = Vis_add_layer(vis, frag, (VisLayerMethods) {
        .events = layer_events,
        .render = layer_render,
        .deinit = layer_deinit }, sizeof(struct layer_data));
    data->vertex_count = hh_arrlen(vertices) / 4;
    // set uniform locations
    GLint program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glUniform3f(glGetUniformLocation((GLuint) program, "color_normal"), COLOR_NORMAL_SRC[0], COLOR_NORMAL_SRC[1], COLOR_NORMAL_SRC[2]);
    glUniform3f(glGetUniformLocation((GLuint) program, "color_active"), COLOR_ACTIVE_SRC[0], COLOR_ACTIVE_SRC[1], COLOR_ACTIVE_SRC[2]);
    // configure buffers
    glGenVertexArrays(1, &data->VAO);
    glGenBuffers(1, &data->VBO);
    glBindVertexArray(data->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, data->VBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) (hh_arrlen(vertices) * sizeof(GLfloat)), vertices, GL_STATIC_DRAW);
    hh_arrfree(vertices);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (GLvoid*) 0);
    glEnableVertexAttribArray(0);
    return true;
}
