#include "network.h"

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "xml.h"
#include "xml_util.h"
#include "cat.h"

struct NETWORK_H__StationEntry {
    Station station;
    bool used;
    bool active;
};

static Network NETWORK_H__net; Network* net = &NETWORK_H__net;

static size_t 
net_hash(const char id[static 2]) {
    return (size_t) ((unsigned char) id[0] << 8 | (unsigned char) id[1]) % net->count;
}

void
net_add_sta(const Station sta) {
    for(size_t i = net_hash(sta.id), j = 0, k; j < net->count; ++j) {
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
    for(size_t i = net_hash(id), j = 0, k; j < net->count; ++j) {
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
net_init(void) {
    HH_ASSERT(cat->station_list != NULL, "No stations were parsed from raw catalogs.");
    net->count = hh_arrlen(cat->antenna_list);
    HH_CALLOC(net->entries, sizeof(StationEntry) * net->count);
    Station sta;
    for(size_t i = 0, j, k = hh_arrlen(cat->position_list); i < net->count; ++i) {
        strncpy(sta.name, cat->antenna_list[i].name, 8);
        sta.axes = cat->antenna_list[i].axes;
        sta.axes_limits[0] = cat->antenna_list[i].axes_limits[0];
        sta.axes_limits[1] = cat->antenna_list[i].axes_limits[1];
        for(j = 0; j < k; ++j) {
            if(cat_name_eq(sta.name, cat->position_list[j].name)) {
                memcpy(sta.id, cat->position_list[j].id, 2);
                sta.x = cat->position_list[j].x;
                sta.y = cat->position_list[j].y;
                sta.z = cat->position_list[j].z;
                sta.lat = cat->position_list[j].lat;
                sta.lon = cat->position_list[j].lon;
                goto net_init_add_sta;
            }
        }
        continue;
net_init_add_sta:
        net_add_sta(sta);
    }
}

void
net_free(void) {
    free(net->entries);
}

void
net_dump(void) {
    for(size_t i = 0; i < net->count; ++i) {
        if(net->entries[i].used) Station_dump(&(net->entries[i].station));
    }
}

bool
net_xml_parse(struct xml_node* root) {
    struct xml_node* general = xml_node_find(root, "general");
    if(general == NULL) return false;
    struct xml_node* stations = xml_node_find(general, "stations");
    if(stations == NULL) return false;
    struct xml_node* child;
    struct xml_string* name;
    Station sta;
    bool* active;
    for(size_t i = 0; i < xml_node_children(stations); ++i) {
        child = xml_node_child(stations, i);
        if(xml_node_name_equals(child, "station")) {
            name = xml_node_content(child);
            for(size_t i = 0; i < net->count; ++i) {
                active = net_get_sta_by_idx(i, &sta);
                if(active == NULL) continue;
                if(cat_name_len(sta.name) != name->length) continue;
                if(memcmp(name->buffer, sta.name, name->length) == 0) {
                    HH_MSG("Added station: %.*s", (int) name->length, name->buffer);
                    *active = true;
                }
            }
        }
    }
    return true;
}

