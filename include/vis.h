#ifndef VIS_H__
#define VIS_H__

#include <stdbool.h>

#include <glenv.h>

#include "network.h"
#include "sky.h"

typedef struct {
    struct nk_context* ctx;
    float row_height;
} VisOverlay;

void
VisOverlay_init(VisOverlay* overlay, RGFW_window* const win);

typedef struct {
    nk_bool right;
    nk_bool width_prop;
    union { float full; float ratio; } width;
    size_t rows;
} VisPanelBounds;

#define VIS_PANEL_BOUNDS_LEFT(_width, _rows) {\
    .right = nk_false,\
    .width_prop = nk_false,\
    .width = { .full = (_width) },\
    .rows = (_rows)\
}

#define VIS_PANEL_BOUNDS_LEFT_RATIO(_width_ratio, _rows) {\
    .right = nk_false,\
    .width_prop = nk_true,\
    .width = { .ratio = (_width_ratio) },\
    .rows = (_rows)\
}

#define VIS_PANEL_BOUNDS_RIGHT(_width, _rows) {\
    .right = nk_true,\
    .width_prop = nk_false,\
    .width = { .full = _width },\
    .rows = (_rows)\
}

#define VIS_PANEL_BOUNDS_RIGHT_RATIO(_width_ratio, _rows) {\
    .right = nk_true,\
    .width_prop = nk_true,\
    .width = { .ratio = (_width_ratio) },\
    .rows = (_rows)\
}

typedef struct {
    const char* title;
    const char* parent;
    VisPanelBounds bounds;
    enum nk_panel_flags flags;
    void (*layout)(void* const data, VisOverlay* overlay);
} VisPanel;

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
    VisPanel panel;
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
    VisOverlay overlay;
    VisLayer* layers;
} Vis;

bool
Vis_init(Vis* const vis, RGFW_window* const win);
void
Vis_free(Vis* const vis);
void
Vis_update_and_draw(Vis* const vis, const RGFW_window* const win, const float gmst);
void
Vis_handle_events(Vis* const vis, const RGFW_window* const win);
// NOTE: A few rough edges with dynamically adding layers:
// - if creating a layer can fail, it needs to fail before this is invoked
// - the layer's shader program is created within this function,
//   any configuration requiring the shader to be set must be after its invocation
void*
Vis_add_layer(Vis* const vis, GLuint frag, 
    VisPanel panel, VisLayerMethods methods, size_t data_size);

// layer declarations
bool
Vis_layer_globe(Vis* const vis, const char* path_globe_image);
bool
Vis_layer_stations(Vis* const vis);
bool
Vis_layer_sources(Vis* const vis);

#endif // VIS_H__
