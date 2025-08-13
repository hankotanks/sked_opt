#include "sky.h"

#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "hh.h"

#include "xml.h"
#include "xml_util.h"
#include "cat.h"
#include "source.h"

struct SKY_H__SourceEntry {
    Source source;
    bool used, active;
};

static Sky SKY_H__sky; Sky* sky = &SKY_H__sky;

void
sky_init(void) {
    HH_ASSERT(cat->source_list != NULL, "No sources were parsed from raw catalogs.");
    sky->count = hh_arrlen(cat->source_list);
    HH_ASSERT(sky->count > 0, "No sources were parsed from raw catalogs.");
    HH_CALLOC(sky->entries, sizeof(SourceEntry) * sky->count);
    Source src;
    for(size_t i = 0; i < sky->count; ++i) {
        strncpy(src.name, cat->source_list[i].name_iau, 8);
        // raan
        src.raan = (double) cat->source_list[i].raan_hrs + \
            (double) cat->source_list[i].raan_min / 60.0 + \
            cat->source_list[i].raan_sec / 3600.0;
        src.raan *= 15.0;
        // decl
        src.decl = fabs((double) cat->source_list[i].decl_deg) + \
            (double) cat->source_list[i].decl_min / 60.0 + \
            cat->source_list[i].decl_sec / 3600.0;
        src.decl *= (cat->source_list[i].decl_deg >= 0) ? 1.0 : -1.0;
        // epoch
        src.epoch = cat->source_list[i].epoch;
        // add source
        sky_add_src(src);
    }
}

void
sky_free(void) {
    free(sky->entries);
}

static size_t 
sky_hash(const char id[static 8]) {
    unsigned long hash = 14695981039346656037UL;
    unsigned char curr;
    bool term = false;
    for(size_t i = 0; i < 8; ++i) {
        curr = term ? '\0' : (unsigned char) id[i];
        term |= id[i] == '\0';
        hash ^= curr;
        hash *= 1099511628211UL;
    }
    return hash % sky->count;
}

bool*
sky_get_src(const char id[static 8], Source* out) {
    for(size_t i = sky_hash(id), j = 0, k; j < sky->count; ++j) {
        k = (i + j) % sky->count;
        if(!sky->entries[k].used) continue;
        if(cat_name_eq(sky->entries[k].source.name, id)) {
            *out = sky->entries[k].source;
            return &(sky->entries[k].active);
        }
    }
    return NULL;
}

bool*
sky_get_src_by_idx(const size_t idx, Source* out) {
    if(!(sky->entries[idx].used)) return NULL;
    if(sky->entries[idx].used) *out = sky->entries[idx].source;
    return &(sky->entries[idx].active);
}

void
sky_add_src(const Source src) {
    for(size_t i = sky_hash(src.name), j = 0, k; j < sky->count; ++j) {
        k = (i + j) % sky->count;
        if(!sky->entries[k].used || cat_name_eq(sky->entries[k].source.name, src.name)) {
            sky->entries[k].source = src;
            sky->entries[k].used = true;
            sky->entries[k].active = false;
            return;
        }
    }
    HH_UNREACHABLE;
}

void
sky_dump(void) {
    for(size_t i = 0; i < sky->count; ++i) {
        if(sky->entries[i].used) Source_dump(&(sky->entries[i].source));
    }
}

void
sky_xml_parse(struct xml_node* root) {
    struct xml_node* general = xml_node_find(root, "general");
    struct xml_node* onlyUseListedSources = xml_node_find(general, "onlyUseListedSources");
    Source src; bool* active;
    if(onlyUseListedSources == NULL) {
        for(size_t i = 0; i < sky->count; ++i) {
            active = sky_get_src_by_idx(i, &src);
            if(active == NULL) continue;
            *active = true;
        }
        return;
    }
    struct xml_node* child;
    struct xml_string* name;
    for(size_t i = 0; i < xml_node_children(onlyUseListedSources); ++i) {
        child = xml_node_child(onlyUseListedSources, i);
        if(xml_node_name_equals(child, "source")) {
            name = xml_node_content(child);
            for(size_t i = 0; i < sky->count; ++i) {
                active = sky_get_src_by_idx(i, &src);
                if(active == NULL) continue;
                if(cat_name_len(src.name) != name->length) continue;
                if(memcmp(name->buffer, src.name, name->length) == 0) {
                    HH_MSG("Added quasar: %.*s.", (int) name->length, name->buffer);
                    *active = true;
                }
            }
        }
    }
}
