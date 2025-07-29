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

#endif // __GLENV_H__
