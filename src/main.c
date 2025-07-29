#define HH_IMPL
#include "hh.h"
#undef HH_IMPL

#include <stdio.h>

#if 0
#include "cat.h"
#include "network.h"
#include "sky.h"
#endif
#include "vis.h"
#include "vis_globe.h"

// window configuration options
#define WINDOW_TITLE "sked_opt"
#define WINDOW_W 800
#define WINDOW_H 600

int main(void) {
    const char* path_root = hh_path(PROJECT_ROOT);
#if 0
    { // parse catalog
        char* path_cat = hh_path_join(hh_path(path_root), "catalogs");
        cat_parse(path_cat);
        hh_arrfree(path_cat);
    }
    { // network
        Network net;
        Network_init(&net);
        Network_free(&net);
    }
    { // source list
        Sky sky;
        Sky_init(&sky);
        Sky_free(&sky);
    }
    // clean up catalog
    cat_clean();
#endif
    // initialize window
    RGFW_window* window = RGFW_createWindow(WINDOW_TITLE, RGFW_RECT(0, 0, WINDOW_W, WINDOW_H), RGFW_windowCenter);
    RGFW_window_setMinSize(window, RGFW_AREA(WINDOW_W, WINDOW_H));
    if(glewInit() != GLEW_OK) {
        HH_ERR("Failed to initialize GLEW.");
        return 1;
    }
    // initialize glenv.h
    glenv_init(window);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    // create visualization layers
    Vis vis;
    Vis_init(&vis, window);
    char* path_globe_image = hh_path_join(hh_path_join(hh_path(path_root), "assets"), "globe.bmp");
    Vis_add_globe_layer(&vis, path_globe_image);
    hh_arrfree(path_globe_image);
    // event loop
    while(RGFW_window_shouldClose(window) == RGFW_FALSE) {
        while(RGFW_window_checkEvent(window)) {
            if(window->event.type == RGFW_windowResized) 
                glViewport(0, 0, (GLsizei) window->r.w, (GLsizei) window->r.h);
            else if(window->event.type == RGFW_quit) break; 
            Vis_handle_events(&vis, window);
        }
        glenv_new_frame();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Vis_update_and_draw(&vis);
        glenv_render(NK_ANTI_ALIASING_ON);
    }
    glenv_deinit();
    RGFW_window_close(window);
    return 0;
}
