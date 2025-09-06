#ifndef VIS_H__
#define VIS_H__

#include <X11/Xlib.h>
#include <stdbool.h>

#include <glenv.h>

#include "hh.h"

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
    bool (*events)(void* const data, const RGFW_window* const win); // TODO: return value not considered in Vis_handle_events
    void (*render)(const void* const data);
    GLuint program;
    GLint loc_proj;
    GLint loc_view;
    GLint loc_gmst;
} VisPass;

typedef enum { NONE = 0, PASS, PANEL, BOTH } VisType;

typedef struct {
    VisType type;
    size_t data_size;
    struct {
        GLuint frag;
        bool (*events)(void* const data, const RGFW_window* const win);
        void (*render)(const void* const data);
    } pass;
    struct {
        glenv_Panel* panel;
        const char* parent_title;
    } panel;
    void (*deinit)(void* const data);
} VisDesc;

void
VisDesc_init(VisDesc* const desc, size_t data_size, void (*deinit)(void* const data));
void
VisDesc_configure_panel(VisDesc* const desc, glenv_Panel* panel, const char* parent_title);
void
VisDesc_configure_pass(VisDesc* const desc, GLuint frag,
    bool (*events)(void* const data, const RGFW_window* const win),
    void (*render)(const void* const data));

typedef struct {
    VisCamera camera;
    struct {
        bool init;
        bool drag;
        int mouse_x;
        int mouse_y;
    } cont;
    GLuint vert;
    struct {
        VisType type;
        void* data;
        glenv_Panel* panel;
        VisPass pass;
        void (*deinit)(void* const data);
    }* layers;
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
Vis_add_layer(Vis* const vis, VisDesc desc);
glenv_Panel*
Vis_get_panel(Vis* const vis, const char* title);

//
// sked_opt specific declarations
//

#define VIS_CFG_ROWS 5
#define VIS_SKD_ROWS 5
#define VIS_RUN_ROWS 3

#define VIS_CFG_RATIO 0.75f
#define VIS_SKD_RATIO 0.25f

static inline bool HH_UNUSED
Vis_expanded(const Vis* const vis, RGFW_rect original, RGFW_rect curr) {
    (void) vis;
    return curr.w > (int) ((float) original.w * (VIS_CFG_RATIO + VIS_SKD_RATIO * 2));
}

// layer declarations
bool
Vis_layer_net(Vis* const vis);
bool
Vis_layer_sky(Vis* const vis);
bool
Vis_layer_cfg(Vis* const vis);
bool
Vis_layer_run(Vis* const vis);
bool
Vis_layer_globe(Vis* const vis, const char* path_globe_image);

#endif // VIS_H__
