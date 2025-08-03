#ifndef SKY_H__
#define SKY_H__

#include <stddef.h>
#include <stdbool.h>

#include "source.h"

typedef struct SKY_H__SourceEntry SourceEntry;
typedef struct {
    size_t count;
    SourceEntry* entries;
} Sky;

extern Sky* sky;

void
sky_init();
void
sky_free();
bool* // Returns NULL if station not found, otherwise, returns pointer to station toggle
sky_get_src(const char id[static 8], Source* out);
bool*
sky_get_src_by_idx(const size_t idx, Source* out);
void
sky_add_src(const Source src);
void
sky_dump();

#endif // SKY_H__
