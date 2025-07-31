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
Sky_init();
void
Sky_free();
bool* // Returns NULL if station not found, otherwise, returns pointer to station toggle
Sky_get_src(const char id[static 8], Source* out);
bool*
Sky_get_src_by_idx(const size_t idx, Source* out);
void
Sky_add_src(const Source src);
void
Sky_dump();

#endif // SKY_H__
