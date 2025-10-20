#define HH_ARGS \
    HH_ARG_OPT(char*, path_cfg, "-c", "path to XML configuration", NULL, args_parse_path_cfg, args_parse_path_clean) \
    HH_ARG_OPT(bool,  headless, "-H", "skip GUI configuration panel", false, NULL, NULL) \
    HH_ARG_OPT(bool,  gen_stat, "-S", "generate stats alongside schedule", false, NULL, NULL) \
    HH_ARG_OPT(char*, path_out, "-o", "output path", NULL, args_parse_path_out, args_parse_path_clean)

#include "hh.h"

void*
args_parse_path_cfg(char* arg, int* ok) {
	char* path = hh_path(arg);
	if(!hh_path_exists(path) || !hh_path_is_file(path)) {
		HH_ERR("Provided input file path argument did not exist [%s].", path);
		(*ok) = 0;
		return NULL;
	}
    if(!hh_ends_with(path, ".xml")) {
        HH_ERR("Provided configuration file path must have an '.xml' extension.");
        (*ok) = 0;
        return NULL;
    }
	return path;
}

void* HH_UNUSED
args_parse_path_out(char* arg, int* ok) {
    char* path = hh_path(arg);
    char* path_parent = hh_path_parent(path);
	if(!hh_path_exists(path_parent) || hh_path_is_file(path_parent)) {
		HH_ERR("Provided output file path contained nonexistent directories [%s].", path);
		(*ok) = 0;
		return NULL;
	}
    if(!hh_ends_with(path, ".skd")) {
        HH_ERR("Provided output file path must have an '.skd' extension.");
        (*ok) = 0;
        return NULL;
    }
    hh_arrfree(path_parent);
    (*ok) = 1;
	return path;
}

void
args_parse_path_clean(void* val) {
	hh_arrfree(val);
}

#define HH_IMPL
#include "hh.h"
#undef HH_IMPL

#include <glenv.h>
#include <sofam.h>

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
const char WINDOW_TITLE[256] = "sked_opt";
#define WINDOW_W 800
#define WINDOW_H 600

// TODO: VieSchedpp.xml configuration files
// contain a list of catalog paths, when supplied, we should use these instead
void
configure_using_xml(const char* const path_xml, bool name) {
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
    if(name) {
        struct xml_node* experimentName = xml_node_find(general, "experimentName");
        HH_ASSERT(experimentName != NULL, "Failed to parse provided configuration [%s].", path_xml);
        (void) hh_arradd(META->name, xml_node_content(experimentName)->length + 1);
        strncpy(META->name, (const char*) xml_node_content(experimentName)->buffer, xml_node_content(experimentName)->length);
    }
    // clean up
    xml_document_free(doc, false);
    hh_arrfree(contents);
}

bool
args(int argc, char* argv[]) {
    // parse CLI args
    if(!hh_args_parse(argc, argv)) {
        HH_ERR("Failed to parse CLI arguments.");
        // TODO: hh_args need auto-generated help printouts
        exit(1);
    }
    // parse XML configuration
    if(hh_args->path_cfg) {
        HH_MSG("Found configuration at [%s].", hh_args->path_cfg);
        configure_using_xml(hh_args->path_cfg, (hh_args->path_out) ? false : true);
    }
    // assign output name and path
    if(hh_args->path_out) {
        META->path_parent = hh_path_parent(hh_args->path_out);
        HH_ASSERT(META->name == NULL, "Unreachable!");
        hh_strput(META->name, hh_path_name(hh_args->path_out));
        // strip extension
        for(size_t i = 5; i > 0; --i) HH_ASSERT(hh_arrpop(META->name) == (".skd")[i - 1], "Unreachable!");
        hh_arrput(META->name, '\0');    
    } else if(hh_args->path_cfg) {
        META->path_parent = hh_path_parent(hh_args->path_cfg);
    } else {
        META->path_parent = hh_path(".");
        HH_ASSERT(hh_path_exists(META->path_parent), "Unreachable!");
    }
    // configure statistics generation
    if(hh_args->gen_stat) META->generate_statistics = true;
    // skip GUI creation if running headless
    if(hh_args->headless) {
        Sched_init_output_and_free(SCHED_DEFAULT);
        return true;
    }
    // clean up hh_args allocation
    hh_args_clean();
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
    // event loop
    glClearColor(0.f, 0.f, 0.f, 1.f);
    while(RGFW_window_shouldClose(window) == RGFW_FALSE) {
        while(RGFW_window_checkEvent(window)) {
            if(window->event.type == RGFW_quit) break; 
            Vis_handle_events(&vis, window);
        }
        glenv_new_frame();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Vis_update_and_draw(&vis, DateTime_to_gmst(TIME_SYS->start) * 180.0 / DPI);
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
