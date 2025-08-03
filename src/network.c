#include "network.h"

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "cat.h"

struct NETWORK_H__StationEntry {
    Station station;
    bool used;
    bool active;
};

static Network NETWORK_H__net; Network* net = &NETWORK_H__net;

static size_t 
Network_hash(const char id[static 2]) {
    return (size_t) ((unsigned char) id[0] << 8 | (unsigned char) id[1]) % net->count;
}

void
net_add_sta(const Station sta) {
    for(size_t i = Network_hash(sta.id), j = 0, k; j < net->count; ++j) {
        k = (i + j) % net->count;
        if(!net->entries[k].used || (memcmp(net->entries[k].station.id, sta.id, 2) == 0)) {
            net->entries[k].station = sta;
            net->entries[k].used = true;
            net->entries[k].active = false;
            return;
        }
    }
    HH_UNREACHABLE;
}

bool*
net_get_sta(const char id[static 2], Station* out) {
    for(size_t i = Network_hash(id), j = 0, k; j < net->count; ++j) {
        k = (i + j) % net->count;
        if(!net->entries[k].used) continue;
        if(memcmp(net->entries[k].station.id, id, 2) == 0) {
            *out = net->entries[k].station;
            return &(net->entries[k].active);
        }
    }
    return NULL;
}

bool*
net_get_sta_by_idx(const size_t idx, Station* out) {
    if(!(net->entries[idx].used)) return NULL;
    if(net->entries[idx].used) *out = net->entries[idx].station;
    return &(net->entries[idx].active);
}

void
net_init() {
    HH_ASSERT(cat->station_list != NULL, "No stations were parsed from raw catalogs.");
    net->count = hh_arrlen(cat->station_list);
    HH_ASSERT(net->count > 0, "No stations were parsed from raw catalogs.");
    HH_CALLOC(net->entries, sizeof(StationEntry) * net->count);
    Station sta;
    bool pos_added;
    bool ant_added;
    for(size_t i = 0, j; i < net->count; ++i) {
        pos_added = false;
        ant_added = false;
        // TODO: There is a single parsed station with `\0\0` id and no data
        // this should be caught in cat.h in the future
        if(cat->station_list[i].id[0] == '\0') continue;
        // stations.cat
        memcpy(sta.id, cat->station_list[i].id, 2);
        strncpy(sta.name, cat->station_list[i].name_pos, 8);
        // position.cat
        for(j = 0; j < hh_arrlen(cat->position_list); ++j) {
            if(cat_name_eq(cat->station_list[i].name_pos, cat->position_list[j].name)) {
                sta.x = cat->position_list[j].x;
                sta.y = cat->position_list[j].y;
                sta.z = cat->position_list[j].z;
                sta.lat = cat->position_list[j].lat;
                sta.lon = cat->position_list[j].lon;
                pos_added = true;
                break;
            }
        }
        // antenna.cat
        for(j = 0; j < hh_arrlen(cat->antenna_list); ++j) {
            if(cat_name_eq(cat->station_list[i].name_ant, cat->antenna_list[j].name)) {
                sta.axes = cat->antenna_list[j].axes;
                sta.axes_limits[0] = cat->antenna_list[j].axes_limits[0];
                sta.axes_limits[1] = cat->antenna_list[j].axes_limits[1];
                ant_added = true;
                break;
            }
        }
        // add the station
        if(ant_added && pos_added) net_add_sta(sta);
    }
}

void
net_free() {
    free(net->entries);
}

void
net_dump() {
    for(size_t i = 0; i < net->count; ++i) {
        if(net->entries[i].used) Station_dump(&(net->entries[i].station));
    }
}
