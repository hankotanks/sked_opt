#ifndef GLENV_H__
#define GLENV_H__

// adapted from ColleagueRiley [1] and Nuklear demos [2]
// [1] https://github.com/ColleagueRiley/nuklear_rgfw/blob/main/rgfw_opengl2/nuklear_rgfw_gl2.h
// [2] https://github.com/Immediate-Mode-UI/Nuklear/blob/master/demo/sdl_opengl2/nuklear_sdl_gl2.h

#include <GL/glew.h>
#include <RGFW.h>
#include <X11/Xlib.h>
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include <nuklear.h>

#if defined(__GNUC__) || defined(__clang__)
#define GLENV_UNUSED __attribute__((unused))
#else
#define GLENV_UNUSED
#endif

#ifndef GLENV_TEXT_BUFFER_SIZE
#define GLENV_TEXT_BUFFER_SIZE 1024
#endif

#ifndef GLENV_SEGMENT_COUNT
#define GLENV_SEGMENT_COUNT 22
#endif

// glenv_init
// initialize the renderingt context
// NOTE: window must be initialized beforehand and glewInit called
NK_API struct nk_context* 
glenv_init(RGFW_window* win);
// glenv_deinit
// clean up resources at the end of the program
NK_API void 
glenv_deinit(void);
// glenv_render
// to be called at the end of the event loop
NK_API void 
glenv_render(enum nk_anti_aliasing AA);
// glenv_new_frame
// to be called at the start of the event loop
NK_API void 
glenv_new_frame(void);

// detect if mouse clicks are consumed by nuklear in this frame
nk_bool
glenv_consumed_mouse(void);

//
// glenv_Panel
//

typedef struct GLENV_H__glenv_Panel glenv_Panel;
typedef void (*glenv_PanelLayout)(void* const data, struct nk_context* ctx, float row_height);
typedef void (*glenv_PanelUpdate)(glenv_Panel* const panel, RGFW_rect original, RGFW_rect curr, void* data);

static inline void GLENV_UNUSED 
glenv_PanelLayout_dummy(void* const data, struct nk_context* ctx, float row_height) {
    (void) data;
    nk_layout_row_dynamic(ctx, row_height, 1);
    nk_label(ctx, "TODO", NK_TEXT_ALIGN_CENTERED);
}

typedef struct {
    nk_bool prop;
    union {
        struct { float ratio; unsigned int pixel_offset; } dynamic;
        unsigned int pixels;
    } width;
} glenv_PanelWidth;

#define \
glenv_PanelWidth_dynamic(ratio, pixel_offset) \
    ((glenv_PanelWidth) { nk_true, {{ (ratio), (pixel_offset) }}})

static inline glenv_PanelWidth GLENV_UNUSED
glenv_PanelWidth_fixed(unsigned int pixels) {
    glenv_PanelWidth w;
    w.prop = nk_false;
    w.width.pixels = pixels;
    return w;
}

typedef struct {
    const glenv_Panel* parent;
    enum nk_panel_flags flags;
    glenv_PanelLayout layout;
    glenv_PanelUpdate update;
    nk_bool right;
    nk_bool bottom;
    glenv_PanelWidth width;
    float offset;
    size_t rows;
} glenv_PanelConfig;

#define glenv_Panel_init(title, ...) GLENV_H__glenv_Panel_init((title), (glenv_PanelConfig) { \
    .parent = NULL, \
    .flags  = NK_WINDOW_BORDER | NK_WINDOW_TITLE, \
    .layout = glenv_PanelLayout_dummy, \
    .update = NULL, \
    .right  = nk_false, \
    .bottom = nk_false, \
    .width  = glenv_PanelWidth_dynamic(1.f, 0.f), \
    .offset = 0.f, \
    .rows   = 1, \
    __VA_ARGS__ \
})

#define glenv_Panel_config(panel, ...) GLENV_H__glenv_Panel_config((panel), (glenv_PanelConfig) { \
    .parent = glenv_Panel_get_config((panel)).parent, \
    .flags  = glenv_Panel_get_config((panel)).flags, \
    .layout = glenv_Panel_get_config((panel)).layout, \
    .update = glenv_Panel_get_config((panel)).update, \
    .right  = glenv_Panel_get_config((panel)).right, \
    .bottom = glenv_Panel_get_config((panel)).bottom, \
    .width  = glenv_Panel_get_config((panel)).width, \
    .offset = glenv_Panel_get_config((panel)).offset, \
    .rows   = glenv_Panel_get_config((panel)).rows, \
    __VA_ARGS__ \
})

// retrieve the panel's title
const char*
glenv_Panel_get_title(const glenv_Panel* const panel);
glenv_PanelConfig
glenv_Panel_get_config(const glenv_Panel* const panel);

// render the panel,
// provided that it is properly configured
// NOTE: does NOT recursively render parents
void 
glenv_Panel_render(glenv_Panel* const panel, void* data);
void
glenv_Panel_update(glenv_Panel* const panel, void* data);

// glenv_Panel INTERNAL
glenv_Panel*
GLENV_H__glenv_Panel_init(const char* title, glenv_PanelConfig config);
void
GLENV_H__glenv_Panel_config(glenv_Panel* const panel, glenv_PanelConfig config);

#endif // GLENV_H__
