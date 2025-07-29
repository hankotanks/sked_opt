#ifndef VIS_H__
#define VIS_H__

#include <stdbool.h>

#include <glenv.h>

typedef struct {
    GLuint buf[3], tex, program; // VAO, VBO, EBO
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
Vis_update(Vis* const vis);
void
Vis_draw(const Vis* const vis);
void
Vis_handle_events(Vis* const vis, const RGFW_window* const win);

#endif // VIS_H__
