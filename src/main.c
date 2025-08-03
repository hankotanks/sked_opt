#define HH_IMPL
#include "hh.h"
#undef HH_IMPL

#include <glenv.h>

#include "cat.h"
#include "network.h"
#include "vis.h"
#include "sky.h"

// window configuration options
#define WINDOW_TITLE "sked_opt"
#define WINDOW_W 800
#define WINDOW_H 600

int main(void) {
    const char* path_root = hh_path(PROJECT_ROOT);
    // parse catalog
    char* path_cat = hh_path_join(hh_path(path_root), "catalogs");
    cat_init(path_cat);
    hh_arrfree(path_cat);
    // initialize network
    net_init();
    // initialize sky
    sky_init();
    // initialize window
    RGFW_window* window = RGFW_createWindow(WINDOW_TITLE, RGFW_RECT(0, 0, WINDOW_W, WINDOW_H), RGFW_windowCenter);
    RGFW_window_setMinSize(window, RGFW_AREA(WINDOW_W, WINDOW_H));
    glenv_init(window);
    // create visualization layers
    Vis vis;
    Vis_init(&vis, window);
    char* path_globe_image = hh_path_join(hh_path_join(hh_path(path_root), "assets"), "globe.bmp");
    Vis_layer_globe(&vis, path_globe_image);
    Vis_layer_net(&vis);
    Vis_layer_sky(&vis);
    hh_arrfree(path_globe_image);
    glClearColor(0.f, 0.f, 0.f, 1.f);
#if 1
    // event loop
    glClearColor(0.f, 0.f, 0.f, 1.f);
    while(RGFW_window_shouldClose(window) == RGFW_FALSE) {
        while(RGFW_window_checkEvent(window)) {
            if(window->event.type == RGFW_windowResized) 
                glViewport(0, 0, (GLsizei) window->r.w, (GLsizei) window->r.h);
            else if(window->event.type == RGFW_quit) break; 
            Vis_handle_events(&vis, window);
        }
        glenv_new_frame();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Vis_update_and_draw(&vis, 0.f);
        glenv_render(NK_ANTI_ALIASING_ON);
    }
#endif
    RGFW_window_close(window);
    glenv_deinit();
    // clean up
    net_free();
    sky_free();
    Vis_free(&vis);
    // finally free catalog
    cat_free();
    hh_arrfree(path_root);
    return 0;
}
