#define HH_IMPL
#include "hh.h"
#undef HH_IMPL

#include <stdio.h>

#include "cat.h"
#include "network.h"
#include "sky.h"
#include "vis.h"

// window configuration options
#define WINDOW_TITLE "sked_viewer"
#define WINDOW_BOUNDS RGFW_RECT(0, 0, 800, 600)

int main(void) {
    const char* path_root = hh_path(PROJECT_ROOT);
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
//
//
//
    RGFW_window* window = RGFW_createWindow(WINDOW_TITLE, WINDOW_BOUNDS, RGFW_windowCenter);
    RGFW_window_setMinSize(window, RGFW_AREA(WINDOW_BOUNDS.w, WINDOW_BOUNDS.h));
    if(glewInit() != GLEW_OK) {
        HH_ERR("Failed to initialize GLEW.");
        return 1;
    }
    glenv_init(window);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    Vis vis;
    char* path_globe_image = hh_path_join(hh_path_join(hh_path(path_root), "assets"), "globe.bmp");
    Vis_init(&vis, window, path_globe_image);
    hh_arrfree(path_globe_image);
//
//
//
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
