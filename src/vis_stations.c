#include "vis.h"

#include <stdbool.h>

#include <glenv.h>

#include "network.h"

const GLfloat FST_COLOR[3] = { 1.f, 0.f, 0.f };
const GLfloat SND_COLOR[3] = { 0.f, 1.f, 0.f };

static const char* shader_source_stations = \
    "#version 330 core\n"
    "flat in uint f_type;\n"
    "uniform vec3 fst_color;\n"
    "uniform vec3 snd_color;\n"
    "out vec4 f_color;\n"
    "void main() {\n"
    "    bool b_type = (f_type != 0u);\n"
    "    f_color = vec4(b_type ? fst_color : snd_color, 1.f);\n"
    "}\n";

struct stations_layer {
    GLuint VAO, VBO;
    size_t station_count;
};

bool 
stations_layer_events(void* const data, const RGFW_window* const win) {
    (void) data;
    (void) win;
    return false;
}

void 
stations_layer_render(const void* const data) {
    const struct stations_layer* layer = data;
    glEnable(GL_DEPTH_TEST);
    glPointSize(5.f);
    glBindVertexArray(layer->VAO);    
    glDrawArrays(GL_POINTS, 0, (GLsizei) layer->station_count);
}

void 
stations_layer_deinit(void* const data) {
    struct stations_layer* layer = data;
    glDeleteVertexArrays(1, &layer->VAO);
    glDeleteBuffers(1, &layer->VBO);
}

bool
Vis_add_stations(Vis* const vis, const Network* const net) {
    GLuint frag = shader_compile_from_source(GL_FRAGMENT_SHADER, shader_source_stations);
    if(!frag) return false;
    // build station vertices
    GLfloat* vertices = NULL;
    Station* sta;
    for(size_t i = 0; i < net->count; ++i) {
        sta = Network_get_sta_by_idx(net, i);
        if(sta == NULL) continue;
        hh_arrput(vertices, (GLfloat) sta->lon);
        hh_arrput(vertices, (GLfloat) (90.0 - sta->lat));
        hh_arrput(vertices, 0.f);
    }
    // allocate data
    struct stations_layer* data = Vis_add_layer(vis, frag, (VisLayerMethods) {
        .events = stations_layer_events,
        .render = stations_layer_render,
        .deinit = stations_layer_deinit }, sizeof(struct stations_layer));
    data->station_count = hh_arrlen(vertices) / 3;
    // set uniform locations
    GLint program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glUniform3f(glGetUniformLocation((GLuint) program, "fst_color"), FST_COLOR[0], FST_COLOR[1], FST_COLOR[2]);
    glUniform3f(glGetUniformLocation((GLuint) program, "snd_color"), SND_COLOR[0], SND_COLOR[1], SND_COLOR[2]);
    // configure buffers
    glGenVertexArrays(1, &data->VAO);
    glGenBuffers(1, &data->VBO);
    glBindVertexArray(data->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, data->VBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) (hh_arrlen(vertices) * sizeof(GLfloat)), vertices, GL_STATIC_DRAW);
    hh_arrfree(vertices);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 3, (GLvoid*) 0);
    glEnableVertexAttribArray(0);
    return true;
}
