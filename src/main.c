#define HH_IMPL
#include "hh.h"
#undef HH_IMPL

#include <stdio.h>

#include "cat.h"
#include "network.h"
#include "sky.h"

#define CAT_DUMP

int main(void) {
    char* path = hh_path(PROJECT_ROOT);
    hh_path_join(path, "catalogs");
    cat_parse(path);
    // network
    Network net;
    Network_init(&net);
#ifdef CAT_DUMP
    Network_dump(&net);
#endif
    Network_free(&net);
    // source list
    Sky sky;
    Sky_init(&sky);
#ifdef CAT_DUMP
    Sky_dump(&sky);
#endif
    Sky_free(&sky);
    // clean up catalog
    cat_clean();
    return 0;
}
