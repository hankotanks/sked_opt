#ifndef SKY_H__
#define SKY_H__

#include <stddef.h>

#include "source.h"

typedef struct SKY_H__SourceEntry SourceEntry;
typedef struct {
    size_t count;
    SourceEntry* entries;
} Sky;

void
Sky_init(Sky* const sky);
void
Sky_free(const Sky* const sky);
Source* 
Sky_get_src(const Sky* const sky, const char id[static 8]);
void
Sky_add_src(const Sky* const sky, const Source src);
void
Sky_dump(const Sky* const sky);

#endif // SKY_H__
