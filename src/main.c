#define HH_IMPL
#include "hh.h"
#undef HH_IMPL

#include <stdio.h>

#include "cat.h"
#include "network.h"

int main(void) {
    char* path = hh_path(PROJECT_ROOT);
    hh_path_join(path, "catalogs");
    cat_parse(path);
    Network net;
    Network_init(&net);
    Network_dump(&net);
    Network_free(&net);
    cat_clean();
    return 0;
}
