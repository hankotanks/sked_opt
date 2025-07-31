#ifndef VIS_H__
#define VIS_H__

#include <stdbool.h>

#include <glenv.h>

GLuint
shader_compile_from_source(GLenum type, const char* source);

typedef struct {
    GLfloat azi, ele, rad;
    GLfloat min, max;
    GLfloat aspect;
    GLfloat proj[16];
    GLfloat view[16];
} VisCamera;

void
VisCamera_init(VisCamera* camera, const RGFW_window* const win);
void
VisCamera_update_projection(VisCamera* camera, const RGFW_window* const win);

typedef struct {
    bool (*events)(void* const data, const RGFW_window* const win); // TODO: return of VisLayerMethods.events is not considered in Vis_handle_events
    void (*render)(const void* const data);
    void (*deinit)(void* const data);
} VisLayerMethods;

typedef struct {
    void* data;
    glenv_Panel panel;
    VisLayerMethods methods;
    GLuint program;
    GLint loc_proj;
    GLint loc_view;
    GLint loc_gmst;
} VisLayer;

typedef struct {
    VisCamera camera;
    struct {
        bool init;
        bool drag;
        int mouse_x;
        int mouse_y;
    } cont;
    GLuint vert;
    VisLayer* layers;
} Vis;

bool
Vis_init(Vis* const vis, RGFW_window* const win);
void
Vis_free(Vis* const vis);
void
Vis_update_and_draw(Vis* const vis, const float gmst);
void
Vis_handle_events(Vis* const vis, const RGFW_window* const win);
// NOTE: A few rough edges with dynamically adding layers:
// - if creating a layer can fail, it needs to fail before this is invoked
// - the layer's shader program is created within this function,
//   any configuration requiring the shader to be set must be after its invocation
void*
Vis_add_layer(Vis* const vis, GLuint frag, VisLayerMethods methods, size_t data_size);
void
Vis_attach_panel(Vis* const vis, glenv_Panel panel, const char* parent);

// layer declarations
bool
Vis_layer_globe(Vis* const vis, const char* path_globe_image);
bool
Vis_layer_net(Vis* const vis);
bool
Vis_layer_sky(Vis* const vis);

#endif // VIS_H__
