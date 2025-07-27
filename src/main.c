#define HH_IMPL
#include "hh.h"
#undef HH_IMPL

#include <stdio.h>

#include "cat.h"

int main(void) {
    char* path = hh_path(PROJECT_ROOT);
    hh_path_join(path, "catalogs");
    cat_parse(path);
    return 0;
}
