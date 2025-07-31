#ifndef __GLENV_H__
#define __GLENV_H__

// adapted from ColleagueRiley [1] and Nuklear demos [2]
// [1] https://github.com/ColleagueRiley/nuklear_rgfw/blob/main/rgfw_opengl2/nuklear_rgfw_gl2.h
// [2] https://github.com/Immediate-Mode-UI/Nuklear/blob/master/demo/sdl_opengl2/nuklear_sdl_gl2.h

#include <GL/glew.h>
#include <RGFW.h>
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include <nuklear.h>

#ifndef GLENV_TEXT_BUFFER_SIZE
#define GLENV_TEXT_BUFFER_SIZE 1024
#endif

#ifndef GLENV_SEGMENT_COUNT
#define GLENV_SEGMENT_COUNT 22
#endif

NK_API struct nk_context* glenv_init(RGFW_window* win);
NK_API void glenv_deinit(void);
// glenv_render
// to be called at the end of the event loop
NK_API void glenv_render(enum nk_anti_aliasing AA);
// glenv_new_frame
// to be called at the end of the event loop
NK_API void glenv_new_frame(void);

typedef struct {
    nk_bool right;
    nk_bool width_prop;
    union { float full; float ratio; } width;
    size_t rows;
} glenv_PanelBounds;

typedef struct GLENV_H__glenv_Panel glenv_Panel;
struct GLENV_H__glenv_Panel {
    const char* title;
    const glenv_Panel* parent;
    glenv_PanelBounds bounds;
    enum nk_panel_flags flags;
    void (*layout)(void* const data, struct nk_context* ctx, float row_height);
};

void 
glenv_Panel_render(glenv_Panel* panel, void* data);

#define glenv_PanelBounds_left(_width, _rows) { \
    .right = nk_false, \
    .width_prop = nk_false, \
    .width = { .full = (_width) }, \
    .rows = (_rows) \
}

#define glenv_PanelBounds_left_ratio(_width_ratio, _rows) { \
    .right = nk_false, \
    .width_prop = nk_true, \
    .width = { .ratio = (_width_ratio) }, \
    .rows = (_rows) \
}

#define glenv_PanelBounds_right(_width, _rows) { \
    .right = nk_true, \
    .width_prop = nk_false, \
    .width = { .full = _width }, \
    .rows = (_rows) \
}

#define glenv_PanelBounds_right_ratio(_width_ratio, _rows) { \
    .right = nk_true, \
    .width_prop = nk_true, \
    .width = { .ratio = (_width_ratio) }, \
    .rows = (_rows) \
}

#endif // __GLENV_H__
