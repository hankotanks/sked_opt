#ifndef SKY_H__
#define SKY_H__

#include <stddef.h>
#include <stdbool.h>

#include "xml.h"
#include "source.h"

// globals
extern struct SKY_H__SKY* SKY;

// macros
#define sky_it(it_)        sky_it_filter(it_, false)
#define sky_it_active(it_) sky_it_filter(it_, true)
#define sky_count          (*((size_t*) SKY))

// interface
void
sky_init(void);
void
sky_free(void);
const Source*
sky_src(const char id[static 8]);
bool*
sky_src_active(const char id[static 8]);
void
sky_dump(void);
bool
sky_xml_parse(struct xml_node* root);

// macro internals
#define sky_it_filter(it_, only_active_) \
    for(size_t it_##_idx = SKY_H__sky_it(0, &it_, only_active_); \
        it_##_idx != SIZE_MAX; \
        it_##_idx = SKY_H__sky_it(it_##_idx, &it_, only_active_))
size_t SKY_H__sky_it(size_t, const Source**, bool);

#endif // SKY_H__
