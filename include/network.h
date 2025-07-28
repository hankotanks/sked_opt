#ifndef NETWORK_H__
#define NETWORK_H__

#include <stddef.h>
#include <stdbool.h>

#include "hh.h"

#include "cat.h"

size_t cat_name_len(const char name[static 8]) {
    return memchr(name, '\0', 8) ? strlen(name) : 8;
}

bool cat_name_eq(const char fst[static 8], const char snd[static 8]) {
    size_t len = cat_name_len(fst);
    if(len != cat_name_len(snd)) return false;
    return memcmp(fst, snd, len) == 0;
}

void cat_name_print(const char name[static 8]) {
    printf("%.*s\n", (int) cat_name_len(name), name);
}

typedef struct {
    char id[2];
    char name[8];
    double x, y, z, lat, lon;
    enum dish_axes axes;
    struct dish_limits axes_limits[2];
} Station;

void Station_dump(const Station* const sta) {
    printf("%c%c [", sta->id[0], sta->id[1]);
    bool term = false;
    for(size_t i = 0; i < 8; ++i) term |= sta->name[i] == '\0';
    if(term) printf("%s", sta->name);
    else printf("%.*s", 8, sta->name);
    printf("]\n");
    printf("  ecef: [%.4lf, %.4lf, %.4lf]\n", sta->x, sta->y, sta->z);
    printf("  lat: %.2lf\n", sta->lat);
    printf("  lon: %.2lf\n", sta->lon);
    printf("  axis limits [");
    switch(sta->axes) {
    case AXES_AZEL:
        printf("azi");
        break;
    case AXES_HADC:
        printf("hr-angle");
        break;
    case AXES_XYEW:
        printf("e-w");
        break;
    case AXES_XYNS:
        printf("n-s");
        break;
    default: HH_UNREACHABLE;
    }
    printf("]:\n");
    printf("    max slew rate [deg/min]: %.2lf\n", sta->axes_limits[0].rate);
    printf("    acc. [deg/min^2]: %zu\n", sta->axes_limits[0].c);
    printf("    min. [deg]: %.2lf\n", sta->axes_limits[0].limits[0]);
    printf("    max. [deg]: %.2lf\n", sta->axes_limits[0].limits[1]);
    printf("  axis limits [");
    switch(sta->axes) {
    case AXES_AZEL:
        printf("el");
        break;
    case AXES_HADC:
        printf("decl");
        break;
    case AXES_XYEW:
        printf("n-s");
        break;
    case AXES_XYNS:
        printf("e-w");
        break;
    default: HH_UNREACHABLE;
    }
    printf("]:\n");
    printf("    max slew rate [deg/min]: %.2lf\n", sta->axes_limits[1].rate);
    printf("    acc. [deg/min^2]: %zu\n", sta->axes_limits[1].c);
    printf("    phys. min. [deg]: %.2lf\n", sta->axes_limits[1].limits[0]);
    printf("    phys. max. [deg]: %.2lf\n", sta->axes_limits[1].limits[1]);
}

typedef struct {
    Station station;
    bool used;
} StationEntry;

typedef struct {
    size_t count;
    StationEntry* entries;
} Network;

static size_t Network_hash(const Network* const net, const char id[static 2]) {
    return (size_t) ((unsigned char) id[0] << 8 | (unsigned char) id[1]) % net->count;
}

void Network_add_sta(const Network* const net, const Station sta) {
    for(size_t i = Network_hash(net, sta.id), j = 0, k; j < net->count; ++j) {
        k = (i + j) % net->count;
        if(!net->entries[k].used || (memcmp(net->entries[k].station.id, sta.id, 2) == 0)) {
            net->entries[k].station = sta;
            net->entries[k].used = true;
            return;
        }
    }
    HH_ASSERT(false, "Unreachable! Network ran out of space.");
}

Station* Network_get_sta(const Network* const net, const char id[static 2]) {
    for(size_t i = Network_hash(net, id), j = 0, k; j < net->count; ++j) {
        k = (i + j) % net->count;
        if(!net->entries[k].used) return NULL;
        if(memcmp(net->entries[k].station.id, id, 2) == 0) return &(net->entries[k].station);
    }
    return NULL;
}

void Network_init(Network* const net) {
    HH_ASSERT(cat->station_list != NULL, "No stations were parsed from raw catalogs.");
    net->count = hh_arrlen(cat->station_list);
    HH_ASSERT(net->count > 0, "No stations were parsed from raw catalogs.");
    net->entries = malloc(sizeof(StationEntry) * net->count);
    HH_CALLOC(net->entries, sizeof(StationEntry) * net->count);
    {
        Station sta;
        for(size_t i = 0, j; i < net->count; ++i) {
            // TODO: There is a single parsed station with `\0\0` id and no data
            // this should be caught in cat.h in the future
            if(cat->station_list[i].id[0] == '\0') continue;
            // stations.cat
            memcpy(sta.id, cat->station_list[i].id, 2);
            memcpy(sta.name, cat->station_list[i].name_pos, 8);
            // position.cat
            for(j = 0; j < hh_arrlen(cat->position_list); ++j) {
                if(cat_name_eq(cat->station_list[i].name_pos, cat->position_list[j].name)) {
                    sta.x = cat->position_list[j].x;
                    sta.y = cat->position_list[j].y;
                    sta.z = cat->position_list[j].z;
                    sta.lat = cat->position_list[j].lat;
                    sta.lon = cat->position_list[j].lon;
                    break;
                }
            }
            // antenna.cat
            for(j = 0; j < hh_arrlen(cat->antenna_list); ++j) {
                if(cat_name_eq(cat->station_list[i].name_ant, cat->antenna_list[j].name)) {
                    sta.axes = cat->antenna_list[j].axes;
                    sta.axes_limits[0] = cat->antenna_list[j].axes_limits[0];
                    sta.axes_limits[1] = cat->antenna_list[j].axes_limits[1];
                    break;
                }
            }
            // add the station
            Network_add_sta(net, sta);
        }
    }
}

void Network_free(const Network* const net) {
    free(net->entries);
}

void Network_dump(const Network* const net) {
    for(size_t i = 0; i < net->count; ++i) {
        if(net->entries[i].used) Station_dump(&(net->entries[i].station));
    }
}

#endif // NETWORK_H__
