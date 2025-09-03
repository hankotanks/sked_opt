#include "sky.h"

#include <stdbool.h>
#include <string.h>
#include <math.h>

#include <sofam.h>

#include "hh.h"

#include "xml.h"
#include "xml_util.h"
#include "cat.h"
#include "source.h"

struct SKY_H__SourceEntry {
    Source source;
    bool used, active;
};

static Sky SKY_H__sky; Sky* SKY = &SKY_H__sky;

void
sky_init(void) {
    HH_ASSERT(CAT->source_list != NULL, "No sources were parsed from raw catalogs.");
    SKY->count = hh_arrlen(CAT->source_list);
    HH_ASSERT(SKY->count > 0, "No sources were parsed from raw catalogs.");
    HH_CALLOC(SKY->entries, sizeof(SourceEntry) * SKY->count);
    Source src;
    size_t src_flux_count = 0;
    size_t src_band_count;
    for(size_t i = 0, j, k = hh_arrlen(CAT->flux_list); i < SKY->count; ++i) {
        strncpy(src.name, CAT->source_list[i].name_iau, 8);
        // raan
        src.raan = (double) CAT->source_list[i].raan_hrs + \
            (double) CAT->source_list[i].raan_min / 60.0 + \
            CAT->source_list[i].raan_sec / 3600.0;
        src.raan *= 15.0;
        src.raan_rad = src.raan * DPI / 180.0;
        // decl
        src.decl = fabs((double) CAT->source_list[i].decl_deg) + \
            (double) CAT->source_list[i].decl_min / 60.0 + \
            CAT->source_list[i].decl_sec / 3600.0;
        src.decl *= (CAT->source_list[i].decl_deg >= 0) ? 1.0 : -1.0;
        src.decl_rad = src.decl * DPI / 180.0;
        // epoch
        src.epoch = CAT->source_list[i].epoch;
        // crs
        src.crs[0] = cos(src.decl_rad) * cos(src.raan_rad);
        src.crs[1] = cos(src.decl_rad) * sin(src.raan_rad);
        src.crs[2] = sin(src.decl_rad);
        // check flux entries
        src_band_count = 0;
        for(j = 0; j < BAND_OTHER; ++j) src.band[j] = false;
        for(j = 0; j < k; ++j) {
            if(cat_name_eq(src.name, CAT->flux_list[j].name_iau)) {
                switch(CAT->flux_list[j].type) {
                case FLUX_B:
                    if(CAT->flux_list[j].band == BAND_OTHER) continue;
                    if(src.band[CAT->flux_list[j].band]) {
                        HH_DBG("Encountered duplicate flux reading for %.*s's %c band.",
                            (int) cat_name_len(src.name), src.name, BAND_CODES[CAT->flux_list[j].band]);
                        // skip the new flux entry if the current one has more steps
                        if(hh_arrlen(src.flux[CAT->flux_list[j].band]) > hh_arrlen(CAT->flux_list[j].entry.b.flux)) continue;
                    } else src_band_count++;
                    src.band[CAT->flux_list[j].band] = true;
                    src.flux[CAT->flux_list[j].band] = CAT->flux_list[j].entry.b.flux;
                case FLUX_M: continue;
                default: HH_UNREACHABLE;
                }
            }
        }
        if(src_band_count > 0) src_flux_count++;
        // TODO: Consider handling of sources without flux readings (src_band_count == 0)
#if 0
        if(src_band_count > 0) HH_DBG("Found flux readings for %.*s across %zu bands.", (int) cat_name_len(src.name), src.name, src_band_count);
        else HH_DBG("No flux readings found for %.*s. Skipping.", (int) cat_name_len(src.name), src.name);
#endif
        sky_add_src(src);
    }
    HH_MSG("Found flux readings for %zu out of %zu sources.", src_flux_count, SKY->count);
}

void
sky_free(void) {
    free(SKY->entries);
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
    return hash % SKY->count;
}

bool*
sky_get_src(const char id[static 8], Source* out) {
    for(size_t i = sky_hash(id), j = 0, k; j < SKY->count; ++j) {
        k = (i + j) % SKY->count;
        if(!SKY->entries[k].used) continue;
        if(cat_name_eq(SKY->entries[k].source.name, id)) {
            *out = SKY->entries[k].source;
            return &(SKY->entries[k].active);
        }
    }
    return NULL;
}

bool*
sky_get_src_by_idx(const size_t idx, Source* out) {
    if(!(SKY->entries[idx].used)) return NULL;
    if(SKY->entries[idx].used) *out = SKY->entries[idx].source;
    return &(SKY->entries[idx].active);
}

void
sky_add_src(const Source src) {
    for(size_t i = sky_hash(src.name), j = 0, k; j < SKY->count; ++j) {
        k = (i + j) % SKY->count;
        if(!SKY->entries[k].used || cat_name_eq(SKY->entries[k].source.name, src.name)) {
            SKY->entries[k].source = src;
            SKY->entries[k].used = true;
            SKY->entries[k].active = false;
            return;
        }
    }
    HH_UNREACHABLE;
}

void
sky_dump(void) {
    for(size_t i = 0; i < SKY->count; ++i) {
        if(SKY->entries[i].used) Source_dump(&(SKY->entries[i].source));
    }
}

bool
sky_xml_parse(struct xml_node* root) {
    struct xml_node* general = xml_node_find(root, "general");
    if(general == NULL) return false;
    struct xml_node* onlyUseListedSources = xml_node_find(general, "onlyUseListedSources");
    Source src; bool* active;
    if(onlyUseListedSources == NULL) {
        size_t count = 0;
        for(size_t i = 0; i < SKY->count; ++i) {
            active = sky_get_src_by_idx(i, &src);
            if(active == NULL) continue;
            *active = true;
            ++count;
        }
        HH_MSG("Added %zu sources from catalog.", count);
        return true;
    }
    struct xml_node* child;
    struct xml_string* child_name;
    char name[8];
    for(size_t i = 0; i < xml_node_children(onlyUseListedSources); ++i) {
        child = xml_node_child(onlyUseListedSources, i);
        if(xml_node_name_equals(child, "source")) {
            child_name = xml_node_content(child);
            strncpy(name, (const char*) child_name->buffer, child_name->length);
            if(child_name->length < 8) name[child_name->length] = '\0';
            if((active = sky_get_src(name, &src)) != NULL) {
                HH_MSG("Added quasar: %.*s", (int) child_name->length, child_name->buffer);
                *active = true;
            } else HH_MSG("Failed to add quasar: %.*s", (int) child_name->length, child_name->buffer);
        }
    }
    return true;
}
