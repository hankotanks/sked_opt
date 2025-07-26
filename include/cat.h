#ifndef CAT_H__
#define CAT_H__

#include "hh.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#define CAT_LIST \
    X(cat_station) \
    X(cat_pos)
#if 0
    X(cat_antenna) \
    X(cat_mask) \
    X(cat_source) \
    X(cat_flux) \
    X(cat_equip)
#endif

#if defined(__GNUC__) || defined(__clang__)
#define CAT_PARSER_UNUSED __attribute__((unused))
#else
#define CAT_PARSER_UNUSED
#endif

#define CAT_TYPE(type_) struct type_##_entry
#define CAT_DECL(type_, file_) \
    static const char* CAT_PARSER_UNUSED type_##_file = file_; \
    static bool CAT_PARSER_UNUSED type_##_parse(const char* line, CAT_TYPE(type_)* entry)

enum StationRackType {
    RACK_MK3,
    RACK_MK4,
    RACK_VLBA,
    RACK_K4,
    RACK_OTHER,
};

enum StationTapeDensity {
    TAPE_LOW,
    TAPE_HIGH,
};

enum StationTapeWidth {
    TAPE_THIN,
    TAPE_THICK,
};

CAT_TYPE(cat_station) {
    char id[2];
    char name_ant[8];
    char name_pos[8];
    enum StationRackType rack;
    size_t head_count;
    enum StationTapeDensity tape_density;
    enum StationTapeWidth tape_width;
};

CAT_DECL(cat_station, "stations.cat") {
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
    memcpy(entry->name_ant, span.ptr, HH_MIN(span.len, 8));
    if(span.len < 8) entry->name_ant[span.len] = '\0';
    // position name
    if(!hh_span_next(&span)) return false;
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
    char* endptr = NULL;
    entry->head_count = (size_t) strtol(span.ptr, &endptr, 10);
    if(endptr == NULL) return false;
    if(endptr != (span.ptr + (ptrdiff_t) span.len)) return false;
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

enum PositionEpoch {
    EPOCH_2020C,
    EPOCH_GLB1069,
    EPOCH_OTHER,
};

CAT_TYPE(cat_pos) {
    char id[2];
    char name[8];
    double x, y, z;
    char occ[8];
    double lon, lat;
    enum PositionEpoch epoch;
};

CAT_DECL(cat_pos, "position.cat") {
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
    memcpy(entry->name, span.ptr, HH_MIN(span.len, 8));
    if(span.len < 8) entry->name[span.len] = '\0';
    // x
    if(!hh_span_next(&span)) return false;
    char* endptr = NULL;
    entry->x = strtod(span.ptr, &endptr);
    if(endptr == NULL) return false;
    if(endptr != (span.ptr + (ptrdiff_t) span.len)) return false;
    // y
    if(!hh_span_next(&span)) return false;
    endptr = NULL;
    entry->y = strtod(span.ptr, &endptr);
    if(endptr == NULL) return false;
    if(endptr != (span.ptr + (ptrdiff_t) span.len)) return false;
    // z
    if(!hh_span_next(&span)) return false;
    endptr = NULL;
    entry->z = strtod(span.ptr, &endptr);
    if(endptr == NULL) return false;
    if(endptr != (span.ptr + (ptrdiff_t) span.len)) return false;
    // occ
    if(!hh_span_next(&span)) return false;
    if(span.len != 8) return false;
    memcpy(entry->name, span.ptr, 8);
    // lon
    if(!hh_span_next(&span)) return false;
    endptr = NULL;
    entry->lon = strtod(span.ptr, &endptr);
    if(endptr == NULL) return false;
    if(endptr != (span.ptr + (ptrdiff_t) span.len)) return false;
    // lat
    if(!hh_span_next(&span)) return false;
    endptr = NULL;
    entry->lat = strtod(span.ptr, &endptr);
    if(endptr == NULL) return false;
    if(endptr != (span.ptr + (ptrdiff_t) span.len)) return false;
    // epoch
    if(!hh_span_next(&span)) return false;
    if(hh_span_equals(span, "2020c")) entry->epoch = EPOCH_2020C;
    else if(hh_span_equals(span, "GLB1069")) entry->epoch = EPOCH_GLB1069;
    else entry->epoch = EPOCH_OTHER;
    return true;
}

static struct {
#define X(type_) CAT_TYPE(type_)* type_##_list;
    CAT_LIST
#undef X
} cat = {
#define X(type_) .type_##_list = NULL,
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
        hh_path_join(path_file, type_##_file); \
        file = fopen(path_file, "r"); \
        HH_ASSERT_MSG(file, "Failed to open catalog [%s].", path_file); \
        CAT_TYPE(type_) entry; \
        while((line_read = hh_getline(&line, &line_len, file)) != -1) { \
            line_temp = hh_skip_whitespace(line); \
            if(line_temp[0] == '*' || line_temp[0] == '\0') continue; \
            if(type_##_parse(line, &entry)) hh_arrput(cat.type_##_list, entry); \
        } \
        HH_MSG("Parsed %zu entries from [%s].", \
            hh_arrlen(cat.type_##_list), path_file); \
        fclose(file); \
        hh_arrfree(path_file); \
    } while(0);
    CAT_LIST
#undef X
}

#if 0
enum AntennaAxes {
    AXES_AZEL,
    AXES_XYNS,
    AXES_HADC,
    AXES_XYEW,
};

struct AntennaAxisLimits {
    double rate;
    double c;
    double limits[2];
};

CAT_TYPE(cat_Antenna) {
    unsigned char id;
    unsigned char name[8];
    enum AntennaAxes axis;
    double offset;
    struct AntennaAxisLimits axis_limits[2];
    double diam;
    unsigned char po[2];
    unsigned char eq[3];
    unsigned char ms[2];
};

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

enum MaskType {
    MASK_COORD,
    MASK_HORIZON,
};

CAT_TYPE(cat_Mask) {
    enum MaskType type;
    unsigned char name[8];
    unsigned char id[2];
    size_t count;
    union {
        double azi_el[80];
        double dec_ha[60];
    } entries;
};

enum SourceFrom {
    FROM_GSFC,
    FROM_ICRF3,
    FROM_2010A,
    FROM_ICRF2,
    FROM_OTHER,
};

CAT_TYPE(cat_Source) {
    unsigned char name_iau[8];
    unsigned char name_common[8];
    size_t raan_hrs;
    size_t raan_min;
    double raan_sec;
    size_t decl_deg;
    size_t decl_min;
    double decl_sec;
    double epoch;
    enum SourceFrom origin;
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
