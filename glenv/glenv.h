#ifndef __GLENV_H__
#define __GLENV_H__

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

typedef struct GLENV_H__glenv_Panel glenv_Panel;
typedef void (*glenv_PanelLayout)(void* const data, struct nk_context* ctx, float row_height);
typedef void (*glenv_PanelResize)(glenv_Panel* const panel, RGFW_rect original, RGFW_rect curr, void* data);

// initialize a glenv_Panel
glenv_Panel*
glenv_Panel_init(const char* title, enum nk_panel_flags flags, glenv_PanelLayout layout);
// set its parent panel, this determines rendering position
void
glenv_Panel_set_parent(glenv_Panel* const panel, const glenv_Panel* const parent);
void
glenv_Panel_set_resize(glenv_Panel* const panel, glenv_PanelResize resize);

// retrieve the panel's title
const char*
glenv_Panel_title(const glenv_Panel* const panel);

// retrieve the panel's parent
const glenv_Panel*
glenv_Panel_parent(const glenv_Panel* const panel);

// render the panel,
// provided that it is properly configured
// NOTE: does NOT recursively render parents
void 
glenv_Panel_render(glenv_Panel* const panel, void* data);
void
glenv_Panel_resize(glenv_Panel* const panel, void* data);

// detect if mouse clicks are consumed by nuklear in this frame
nk_bool
glenv_consumed_mouse(void);

// configure panel behavior,
// one of these functions MUST be called,
// or the panel will not be rendered
void
glenv_Panel_config_left(glenv_Panel* const panel, size_t rows, unsigned int pixels, unsigned int offset);
void
glenv_Panel_config_left_ratio(glenv_Panel* const panel, size_t rows, float ratio, float offset);
void
glenv_Panel_config_right(glenv_Panel* const panel, size_t rows, unsigned int pixels, unsigned int offset);
void
glenv_Panel_config_right_ratio(glenv_Panel* const panel, size_t rows, float ratio, float offset);

#endif // __GLENV_H__
