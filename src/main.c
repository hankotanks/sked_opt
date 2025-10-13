#define HH_IMPL
#include "hh.h"
#undef HH_IMPL

#include <glenv.h>

#include "astro.h"
#include "xml.h"
#include "xml_util.h"
#include "cat.h"
#include "network.h"
#include "meta.h"
#include "vis.h"
#include "sky.h"
#include "time_sys.h"
#include "out.h"
#include "sched.h"

// window configuration options
#define WINDOW_TITLE "sked_opt"
#define WINDOW_W 800
#define WINDOW_H 600

// TODO: VieSchedpp.xml configuration files
// contain a list of catalog paths, when supplied, we should use these instead
void
configure_using_xml(const char* const path_xml) {
    char* contents = hh_read_entire_file(path_xml);
    HH_ASSERT(contents != NULL, "Failed to read provided configuration [%s].", path_xml);
    struct xml_document* doc = xml_parse_document_skip_preamble(contents);
    HH_ASSERT(doc != NULL, "Failed to parse provided configuration [%s].", path_xml);
    // update state using xml_document
    struct xml_node* root = xml_document_root(doc);
    if(!(net_xml_parse(root) && sky_xml_parse(root) && time_sys_xml_parse(root)))
        HH_MSG("Failed to parse provided configuration [%s].", path_xml);
    // read metadata from <general>
    struct xml_node* general = xml_node_find(root, "general");
    HH_ASSERT(general != NULL, "Failed to parse provided configuration [%s].", path_xml);
    struct xml_node* experimentName = xml_node_find(general, "experimentName");
    HH_ASSERT(experimentName != NULL, "Failed to parse provided configuration [%s].", path_xml);
    (void) hh_arradd(META->name, xml_node_content(experimentName)->length + 1);
    strncpy(META->name, (const char*) xml_node_content(experimentName)->buffer, xml_node_content(experimentName)->length);
    // clean up
    xml_document_free(doc, false);
    hh_arrfree(contents);
}

bool
args(int argc, char* argv[]) {
    if(argc == 1) goto args_default_meta;
    // validate configuration path
    char* path_xml = hh_path(argv[1]);
    HH_ASSERT(hh_path_exists(path_xml) && hh_path_is_file(path_xml), 
        "Unable to locate provided configuration [%s].", argv[1]);
    HH_MSG("Found configuration at [%s].", path_xml);
    // get parent path
    META->path_parent = hh_path_parent(path_xml);
    HH_ASSERT(hh_path_exists(META->path_parent), 
        "Unable to locate provided configuration [%s].", argv[1]);
    // parse xml
    configure_using_xml(path_xml);
    hh_arrfree(path_xml);
    if(argc == 3 && ((strcmp(argv[2], "--headless") == 0) || strcmp(argv[2], "-H") == 0)) {
        // TODO: The following 4 lines are duplicated in vis_run.c
        // consider refactoring
        Sched* skd = Sched_init(SCHED_DEFAULT);
        generate_schedule(skd);
        generate_statistics(skd);
        Sched_free(skd);
        return true;
    }
    return false;
args_default_meta:
    META->path_parent = hh_path(".");
    HH_ASSERT(hh_path_exists(META->path_parent), "Unreachable!");
    META->name = NULL;
    return false;
}

int 
main(int argc, char* argv[]) {
    const char* path_root = hh_path(PROJECT_ROOT);
    // parse catalog
    char* path_cat = hh_path_join(hh_path(path_root), "catalogs");
    cat_init(path_cat);
    hh_arrfree(path_cat);
    // initialize network
    net_init();
    // initialize sky
    sky_init();
    // initialize time system
    time_sys_init();
    // initialize earth params
    earth_params_init();
    // CLI arguments
    meta_init();
    if(args(argc, argv)) goto main_headless_cleanup;
    // initialize window
    RGFW_window* window = RGFW_createWindow(WINDOW_TITLE, RGFW_RECT(0, 0, WINDOW_W, WINDOW_H), RGFW_windowCenter);
    RGFW_window_setMinSize(window, RGFW_AREA(WINDOW_W, WINDOW_H));
    glenv_init(window);
    // create visualization layers
    Vis vis;
    Vis_init(&vis, window);
    char* path_globe_image = hh_path_join(hh_path_join(hh_path(path_root), "assets"), "globe.bmp");
    Vis_layer_globe(&vis, path_globe_image);
    Vis_layer_run(&vis);
    Vis_layer_cfg(&vis);
    Vis_layer_net(&vis);
    Vis_layer_sky(&vis);
    hh_arrfree(path_globe_image);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    // event loop
    glClearColor(0.f, 0.f, 0.f, 1.f);
    while(RGFW_window_shouldClose(window) == RGFW_FALSE) {
        while(RGFW_window_checkEvent(window)) {
            if(window->event.type == RGFW_quit) break; 
            Vis_handle_events(&vis, window);
        }
        glenv_new_frame();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Vis_update_and_draw(&vis, (float) DateTime_to_gmst(TIME_SYS->start));
        glenv_render(NK_ANTI_ALIASING_ON);
    }
    glenv_deinit();
    RGFW_window_close(window);
    // clean up
    Vis_free(&vis);
main_headless_cleanup:
    meta_free();
    earth_params_free();
    net_free();
    sky_free();
    // finally free catalog
    cat_free();
    hh_arrfree(path_root);
    return 0;
}
