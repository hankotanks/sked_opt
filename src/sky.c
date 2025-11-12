#include "sky.h"

#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include <sofam.h>

#include "hh.h"

#include "xml.h"
#include "xml_util.h"
#include "cat.h"
#include "source.h"

struct SKY_H__SKY {
    size_t count;
    struct {
        Source source;
        bool used, active;
    }* entries;
};

static struct SKY_H__SKY SKY_H__SKY; 
struct SKY_H__SKY* SKY = &SKY_H__SKY;

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

void
sky_add(const Source src) {
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
sky_init(void) {
    HH_ASSERT(CAT->source_list != NULL, "No sources were parsed from raw catalogs.");
    SKY->count = hh_arrlen(CAT->source_list);
    HH_ASSERT(SKY->count > 0, "No sources were parsed from raw catalogs.");
    HH_CALLOC(SKY->entries, sizeof(*(SKY->entries)) * SKY->count);
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
        sky_add(src);
    }
    HH_MSG("Found flux readings for %zu out of %zu sources.", src_flux_count, SKY->count);
}

void
sky_free(void) {
    free(SKY->entries);
}

const Source*
sky_src(const char id[static 8]) {
    for(size_t i = sky_hash(id), j = 0, k; j < SKY->count; ++j) {
        k = (i + j) % SKY->count;
        if(!SKY->entries[k].used) continue;
        if(cat_name_eq(SKY->entries[k].source.name, id)) return &(SKY->entries[k].source);
    }
    return NULL;
}

bool*
sky_src_active(const char id[static 8]) {
    for(size_t i = sky_hash(id), j = 0, k; j < SKY->count; ++j) {
        k = (i + j) % SKY->count;
        if(!SKY->entries[k].used) continue;
        if(cat_name_eq(SKY->entries[k].source.name, id)) return &(SKY->entries[k].active);
    }
    return NULL;
}

void
sky_dump(void) {
    for(size_t i = 0; i < SKY->count; ++i) {
        if(SKY->entries[i].used) Source_dump(&(SKY->entries[i].source));
    }
}

// TODO: This is a rough comparison just to aid
// the construction of schedule configs
// In the future, once I have a good metric, this could
// be added as a sort option in the UI
int 
compare_flux_densities(const void* fst, const void* snd) {
    const Source* src_fst = (const Source*)(*(const uintptr_t*)fst);
    const Source* src_snd = (const Source*)(*(const uintptr_t*)snd);

    double src_fst_score = 0.0;
    double src_snd_score = 0.0;
    size_t src_fst_band_count = 0;
    size_t src_snd_band_count = 0;
    for(size_t i = 0; i < (size_t) BAND_OTHER; ++i) {
        if(src_fst->band[i]) {
            double flux_max = -DBL_MAX;
            for(size_t j = 0; j < hh_arrlen(src_fst->flux[i]); ++j) {
                double flux = src_fst->flux[i][j].flux;
                flux_max = (flux > flux_max) ? flux : flux_max;
            }
            src_fst_score += flux_max;
            src_fst_band_count++;
        }
        if(src_snd->band[i]) {
            double flux_max = -DBL_MAX;
            for(size_t j = 0; j < hh_arrlen(src_snd->flux[i]); ++j) {
                double flux = src_snd->flux[i][j].flux;
                flux_max = (flux > flux_max) ? flux : flux_max;
            }
            src_snd_score += flux_max;
            src_snd_band_count++;
        }
    }
    if(src_fst_band_count > 0) src_fst_score /= (double) src_fst_band_count;
    if(src_snd_band_count > 0) src_snd_score /= (double) src_snd_band_count;
    if(src_fst_score < src_snd_score) return -1;
    else if(src_fst_score > src_snd_score) return 1;
    else return 0;
}

void
sky_dump_flux_scores(void) {
    uintptr_t* ranking = NULL;
    const Source* src;
    sky_it(src) hh_arrput(ranking, (uintptr_t) src);
    qsort(ranking, hh_arrlen(ranking), sizeof(uintptr_t), compare_flux_densities);
    for(size_t i = 0; i < hh_arrlen(ranking); ++i) {
        src = (const Source*) ranking[i];
        printf("%4zu ", i);
        cat_name_print(src->name);   
        printf(": ");
        for(size_t j = 0; j < (size_t) BAND_OTHER; ++j) {
            if(!(src->band[j])) continue;
            printf("%c ", BAND_CODES[(enum band) j]);
            double flux_max = -DBL_MAX;
            for(size_t k = 0; k < hh_arrlen(src->flux[j]); ++k) {
                double flux = src->flux[j][k].flux;
                flux_max = (flux > flux_max) ? flux : flux_max;
            }
            printf("[%.3f], ", flux_max);
        }
        printf("\n");
    }
}

bool
sky_xml_parse(struct xml_node* root) {
    struct xml_node* general = xml_node_find(root, "general");
    if(general == NULL) return false;
    struct xml_node* onlyUseListedSources = xml_node_find(general, "onlyUseListedSources");
    if(onlyUseListedSources == NULL) {
        size_t count = 0;
        const Source* src;
        sky_it(src) {
            *(sky_src_active(src->name)) = true;
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
            if(sky_src_active(name) != NULL) {
                HH_MSG("Added quasar: %.*s", (int) child_name->length, child_name->buffer);
                *(sky_src_active(name)) = true;
            } else HH_MSG("Failed to add quasar: %.*s", (int) child_name->length, child_name->buffer);
        }
    }
    return true;
}

//
// helper functions
//

size_t
SKY_H__sky_it(size_t i, const Source** src, bool only_active) {
    (*src) = NULL;
    while(i < SKY->count) {
        (*src) = &(SKY->entries[i++].source);
        if(SKY->entries[i - 1].used) {
            if(!only_active || SKY->entries[i - 1].active) return i;
        }
    }
    return SIZE_MAX;
}
