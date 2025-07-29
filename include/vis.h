#ifndef VIS_H__
#define VIS_H__

#include <stdbool.h>

#include <glenv.h>

#if 0
typedef struct {
    void* data;
    bool (*events)(void* const data, const RGFW_window* const win);
    void (*update)(void* const data);
    void (*render)(const void* const data);
} VisLayer;
#endif

typedef struct {
    GLuint VAO, VBO, EBO, tex, program;
    GLint loc_proj;
    GLint loc_view;
    struct {
        GLfloat azi, ele, rad;
        GLfloat min, max;
        GLfloat aspect;
        GLfloat proj[16];
        GLfloat view[16];
    } camera;
    struct {
        bool init;
        bool drag;
        int mouse_x;
        int mouse_y;
    } cont;
} Vis;

bool
Vis_init(Vis* const vis, const RGFW_window* const win, const char* path_globe_image);
void
Vis_free(Vis* const vis);
void
Vis_update_and_draw(Vis* const vis);
void
Vis_handle_events(Vis* const vis, const RGFW_window* const win);

#endif // VIS_H__
