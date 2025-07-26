#include "cat.h"

#define HH_IMPL
#include "hh.h"

#include <stdio.h>

int main(void) {
    char* path = hh_path(ROOT);
    hh_path_join(path, "catalogs");
    cat_parse(path);
    return 0;
}
