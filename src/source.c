#include "source.h"

#include "cat.h"

void
Source_dump(const Source* const src) {
    cat_name_print(src->name);
    printf("\n");
    printf("  raan [deg]: %lf\n", src->raan);
    printf("  decl [deg]: %lf\n", src->decl);
    printf("  epoch: %lf\n", src->epoch);
}
