#ifndef CAT_H__
#define CAT_H__

#include "hh.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#define CAT_LIST \
    X(station) \
    X(position) \
    X(antenna) \
    X(mask) \
    X(source)
#if 0
    X(cat_flux) \
    X(cat_equip)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define CAT_H__UNUSED __attribute__((unused))
#else
#define CAT_H__UNUSED
#endif

#define CAT_TYPE(type_) struct CAT_H__##type_##_entry
#define CAT_DECL(type_, file_) \
    static const char* CAT_H__UNUSED CAT_H__##type_##_file = file_; \
    static bool CAT_H__UNUSED CAT_H__##type_##_parse(const char* line, CAT_TYPE(type_)* entry)

enum rack_type {
    RACK_MK3,
    RACK_MK4,
    RACK_VLBA,
    RACK_K4,
    RACK_OTHER,
};

enum tape_density {
    TAPE_LOW,
    TAPE_HIGH,
};

enum tape_width {
    TAPE_THIN,
    TAPE_THICK,
};

CAT_TYPE(station) {
    char id[2];
    char name_ant[8];
    char name_pos[8];
    enum rack_type rack;
    size_t head_count;
    enum tape_density tape_density;
    enum tape_width tape_width;
};

CAT_DECL(station, "stations.cat") {
    hh_span_t span;
    span.ptr = line;
    span.len = 0;
    // id
    if(!hh_span_next(&span)) return false;
    if(span.len != 2) return false;
    entry->id[0] = span.ptr[0];
    entry->id[1] = span.ptr[1];
    // antenna name
    if(!hh_span_next(&span)) return false;
    if(span.len > 8) return false;
    memcpy(entry->name_ant, span.ptr, HH_MIN(span.len, 8));
    if(span.len < 8) entry->name_ant[span.len] = '\0';
    // position name
    if(!hh_span_next(&span)) return false;
    if(span.len > 8) return false;
    memcpy(entry->name_pos, span.ptr, HH_MIN(span.len, 8));
    if(span.len < 8) entry->name_pos[span.len] = '\0';
    // rack type
    if(!hh_span_next(&span)) return false;
    if(hh_span_equals(span, "K4")) entry->rack = RACK_K4;
    else if(hh_span_equals(span, "Mk3")) entry->rack = RACK_MK3;
    else if(hh_span_equals(span, "Mk4")) entry->rack = RACK_MK4;
    else if(hh_span_equals(span, "VLBA")) entry->rack = RACK_VLBA;
    else entry->rack = RACK_OTHER;
    // head count
    if(!hh_span_next(&span)) return false;
    if(!hh_span_size_t(span, &entry->head_count)) return false;
    // tape density
    if(!hh_span_next(&span)) return false;
    if(strncmp(span.ptr, "Low", 3) == 0) entry->tape_density = TAPE_LOW;
    else if(strncmp(span.ptr, "High", 4) == 0) entry->tape_density = TAPE_HIGH;
    else return false;
    // tape width
    if(!hh_span_next(&span)) return false;
    if(strncmp(span.ptr, "Thin", 4) == 0) entry->tape_width = TAPE_THIN;
    else if (strncmp(span.ptr, "Thick", 5) == 0) entry->tape_width = TAPE_THICK;
    else return false;
    return true;
}

enum solution_epoch {
    EPOCH_2020C,
    EPOCH_GLB1069,
    EPOCH_OTHER,
};

CAT_TYPE(position) {
    char id[2];
    char name[8];
    double x, y, z;
    char occ[8];
    double lon, lat;
    enum solution_epoch epoch;
};

CAT_DECL(position, "position.cat") {
    hh_span_t span;
    span.ptr = line;
    span.len = 0;
    // id
    if(!hh_span_next(&span)) return false;
    if(span.len != 2) return false;
    entry->id[0] = span.ptr[0];
    entry->id[1] = span.ptr[1];
    // name
    if(!hh_span_next(&span)) return false;
    if(span.len > 8) return false;
    memcpy(entry->name, span.ptr, HH_MIN(span.len, 8));
    if(span.len < 8) entry->name[span.len] = '\0';
    // x
    if(!hh_span_next(&span)) return false;
    if(!hh_span_double(span, &entry->x)) return false;
    // y
    if(!hh_span_next(&span)) return false;
    if(!hh_span_double(span, &entry->y)) return false;
    // z
    if(!hh_span_next(&span)) return false;
    if(!hh_span_double(span, &entry->z)) return false;
    // occ
    if(!hh_span_next(&span)) return false;
    if(span.len != 8) return false;
    memcpy(entry->name, span.ptr, 8);
    // lon
    if(!hh_span_next(&span)) return false;
    if(!hh_span_double(span, &entry->lon)) return false;
    // lat
    if(!hh_span_next(&span)) return false;
    if(!hh_span_double(span, &entry->lat)) return false;
    // epoch
    if(!hh_span_next(&span)) return false;
    if(hh_span_equals(span, "2020c")) entry->epoch = EPOCH_2020C;
    else if(hh_span_equals(span, "GLB1069")) entry->epoch = EPOCH_GLB1069;
    else entry->epoch = EPOCH_OTHER;
    return true;
}

enum dish_axes {
    AXES_AZEL,
    AXES_XYNS,
    AXES_HADC,
    AXES_XYEW,
};

struct dish_limits {
    double rate;
    double limits[2];
    size_t c;
};

CAT_TYPE(antenna) {
    char id;
    char name[8];
    enum dish_axes axis;
    double offset;
    struct dish_limits axis_limits[2];
    double diam;
    char po[2], eq[3], ms[2];
};

CAT_DECL(antenna, "antenna.cat") {
    hh_span_t span;
    span.ptr = line;
    span.len = 0;
    // id
    if(!hh_span_next(&span)) return false;
    if(span.len != 1) return false;
    entry->id = span.ptr[0];
    // name
    if(!hh_span_next(&span)) return false;
    if(span.len > 8) return false;
    memcpy(entry->name, span.ptr, HH_MIN(span.len, 8));
    if(span.len < 8) entry->name[span.len] = '\0';
    // axis
    if(!hh_span_next(&span)) return false;
    if(hh_span_equals(span, "AZEL")) entry->axis = AXES_AZEL;
    else if(hh_span_equals(span, "HADC")) entry->axis = AXES_HADC;
    else if(hh_span_equals(span, "XYNS")) entry->axis = AXES_XYNS;
    else if(hh_span_equals(span, "XYEW")) entry->axis = AXES_XYEW;
    else return false;
    // offset
    if(!hh_span_next(&span)) return false;
    if(!hh_span_double(span, &entry->offset)) return false;
    // axis_limits
    for(size_t i = 0, j; i <= 1; ++i) {
        if(!hh_span_next(&span)) return false;
        if(!hh_span_double(span, &(entry->axis_limits[i].rate))) return false;
        if(!hh_span_next(&span)) return false;
        if(!hh_span_size_t(span, &(entry->axis_limits[i].c))) return false;
        for(j = 0; j <= 1; ++j) {
            if(!hh_span_next(&span)) return false;
            if(!hh_span_double(span, &(entry->axis_limits[i].limits[j]))) return false;
        }
    }
    // diam
    if(!hh_span_next(&span)) return false;
    if(!hh_span_double(span, &entry->diam)) return false;
    // po
    if(!hh_span_next(&span)) return false;
    if(span.len != 2) return false;
    entry->po[0] = span.ptr[0];
    entry->po[1] = span.ptr[1];
    // eq
    if(!hh_span_next(&span)) return false;
    if(span.len < 2 || span.len > 3) return false;
    if(hh_span_equals(span, "--")) {
        entry->eq[0] = '\0';
    } else {
        entry->eq[0] = span.ptr[0];
        entry->eq[1] = span.ptr[1];
        if(span.len == 3) entry->eq[2] = span.ptr[2];
        else entry->eq[2] = '\0';
    }
    // ms
    if(!hh_span_next(&span)) return false;
    if(span.len != 2) return false;
    entry->ms[0] = span.ptr[0];
    entry->ms[1] = span.ptr[1];
    return true;
}

enum station_mask {
    MASK_COORD,
    MASK_HORIZON,
};

CAT_TYPE(mask) {
    enum station_mask type;
    char name[8];
    char id[2];
    size_t count;
    union {
        double azi_el[81];
        double dec_ha[61];
    } entries;
};

CAT_DECL(mask, "mask.cat") {
    hh_span_t span;
    span.ptr = line;
    span.len = 0;
    // count (must set it to 0 pre-emptively)
    entry->count = 0;
    // type
    if(!hh_span_next(&span)) return false;
    if(span.len != 1) return false;
    // keep track of whether this entry is a continuation
    bool ext = false;
    if(span.ptr[0] == 'H') entry->type = MASK_HORIZON;
    else if(span.ptr[0] == 'C') entry->type = MASK_COORD;
    else if(span.ptr[0] == '-') { entry -= 1; ext = true; } 
    else return false;
    if(!ext) {
        // name
        if(!hh_span_next(&span)) return false;
        if(span.len > 8) return false;
        memcpy(entry->name, span.ptr, HH_MIN(span.len, 8));
        if(span.len < 8) entry->name[span.len] = '\0';
        // id
        if(!hh_span_next(&span)) return false;
        if(span.len != 2) return false;
        entry->id[0] = span.ptr[0];
        entry->id[1] = span.ptr[1];
    }
    // entries and count
#define CAT_H__ENTRIES ((entry->type == MASK_HORIZON) ? \
    entry->entries.azi_el : \
    entry->entries.dec_ha)
#define CAT_H__ENTRIES_LEN ((entry->type == MASK_HORIZON) ? \
    (sizeof(entry->entries.azi_el) / sizeof(entry->entries.azi_el[0])) : \
    (sizeof(entry->entries.dec_ha) / sizeof(entry->entries.dec_ha[0])))
    if(!hh_span_next(&span)) return false;
    do {
        if(!hh_span_double(span, CAT_H__ENTRIES + entry->count)) return false;
        hh_span_next(&span);
        entry->count++;
    } while(span.len && entry->count < CAT_H__ENTRIES_LEN);
#undef CAT_H__ENTRIES
#undef CAT_H__ENTRIES_LEN
    return !ext;
}

enum quasar_origin {
    FROM_GSFC,
    FROM_ICRF3,
    FROM_2010A,
    FROM_ICRF2,
    FROM_OTHER,
};

CAT_TYPE(source) {
    unsigned char name_iau[8];
    unsigned char name_common[8];
    size_t raan_hrs;
    size_t raan_min;
    double raan_sec;
    long decl_deg;
    size_t decl_min;
    double decl_sec;
    double epoch;
    enum quasar_origin origin;
};

CAT_DECL(source, "source.cat.geodetic.good") {
    hh_span_t span;
    span.ptr = line;
    span.len = 0;
    // name_iau
    if(!hh_span_next(&span)) return false;
    if(span.len != 8) return false;
    memcpy(entry->name_iau, span.ptr, 8);
    // name_common
    if(!hh_span_next(&span)) return false;
    if(span.len == 1 && span.ptr[0] == '$') entry->name_common[0] = '\0';
    else if(span.len > 8) return false;
    else {
        memcpy(entry->name_common, span.ptr, HH_MIN(span.len, 8));
        if(span.len < 8) entry->name_common[span.len] = '\0';
    }
    // raan
    if(!hh_span_next(&span)) return false;
    if(!hh_span_size_t(span, &entry->raan_hrs)) return false;
    if(!hh_span_next(&span)) return false;
    if(!hh_span_size_t(span, &entry->raan_min)) return false;
    if(!hh_span_next(&span)) return false;
    if(!hh_span_double(span, &entry->raan_sec)) return false;
    //decl
    if(!hh_span_next(&span)) return false;
    if(!hh_span_long(span, &entry->decl_deg)) return false;
    if(!hh_span_next(&span)) return false;
    if(!hh_span_size_t(span, &entry->decl_min)) return false;
    if(!hh_span_next(&span)) return false;
    if(!hh_span_double(span, &entry->decl_sec)) return false;
    // epoch
    if(!hh_span_next(&span)) return false;
    if(!hh_span_double(span, &entry->decl_sec)) return false;
    // skip null field
    if(!hh_span_next(&span)) return false;
    // origin
    if(!hh_span_next(&span)) return false;
    if(hh_span_equals(span, "GSFC")) entry->origin = FROM_GSFC;
    else if(hh_span_equals(span, "ICRF3")) entry->origin = FROM_ICRF3;
    else if(hh_span_equals(span, "ICRF2")) entry->origin = FROM_ICRF2;
    else if(hh_span_equals(span, "2010a")) entry->origin = FROM_2010A;
    else entry->origin = FROM_OTHER;
    return true;
}

static struct {
#define X(type_) CAT_TYPE(type_)* CAT_H__##type_##_list;
    CAT_LIST
#undef X
} cat = {
#define X(type_) .CAT_H__##type_##_list = NULL,
    CAT_LIST
#undef X
};

void cat_parse(const char* path) {
    FILE* file;
    char* path_file;
    char* line = NULL;
    const char* line_temp;
    size_t line_len; ptrdiff_t line_read;
#define X(type_) \
    do { \
        path_file = hh_path(path); \
        hh_path_join(path_file, CAT_H__##type_##_file); \
        file = fopen(path_file, "r"); \
        HH_ASSERT_MSG(file, "Failed to open catalog [%s].", path_file); \
        hh_arradd(cat.CAT_H__##type_##_list, 1); \
        while((line_read = hh_getline(&line, &line_len, file)) != -1) { \
            line_temp = hh_skip_whitespace(line); \
            if(line_temp[0] == '*' || line_temp[0] == '\0') continue; \
            if(CAT_H__##type_##_parse(line, &hh_arrlast(cat.CAT_H__##type_##_list))) hh_arradd(cat.CAT_H__##type_##_list, 1); \
        } \
        HH_MSG("Parsed %zu entries from [%s].", \
            hh_arrlen(cat.CAT_H__##type_##_list), path_file); \
        fclose(file); \
        hh_arrfree(path_file); \
    } while(0);
    CAT_LIST
#undef X
}

#if 0
enum EquipHeadStacks {
    HEADS_1X56000,
    HEADS_2X56000,
    HEADS_K4,
    HEADS_S2_LP,
    HEADS_S2_SLP,
};

enum EquipBand {
    BAND_X,
    BAND_S,
    BAND_C,
    BAND_K,
    BAND_OTHER,
};

CAT_TYPE(cat_Equip) {
    unsigned char name_ant[8];
    unsigned char id[2];
    unsigned char name_dat[8];
    enum EquipHeadStacks heads;
    size_t tape_count;
    size_t tape_speed;
    size_t x_flux;
    size_t s_flux;
    enum EquipBand x, s;
    // TODO: Omitting SEFD param/Equip field
};

// TODO: flux.cat

enum FluxEntryType {
    FLUX_B,
    FLUX_M,
};

CAT_TYPE(cat_Flux) {
    unsigned char name_iau[8];
    enum EquipBand band;
    enum FluxEntryType type;
    union {
        struct {
            double flux_total;
            double flux_total_limit;
            double flux_corr;
            double flux_corr_limit;
        } b;
        struct {
            double flux;
            double major_axis;
            double ratio;
            double pa;
            double offsets[2];
        } m;
    } entry;
};

// TODO: modes.cat
// The idea is to read the modes from modes.cat,
// then construct a list of stations that can participate in a scan of each mode type
// using the values presented in equip.cat
#endif // temp

#endif // CAT_H__
