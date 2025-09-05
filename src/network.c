#include "network.h"

#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "xml.h"
#include "xml_util.h"
#include "cat.h"
#include "station.h"

struct NETWORK_H__NET {
    size_t count;
    struct {
        Station station;
        bool used;
        bool active;
    }* entries;
};

static struct NETWORK_H__NET NETWORK_H__NET; 
struct NETWORK_H__NET* NET = &NETWORK_H__NET;

static size_t 
net_hash(const char id[static 2]) {
    return (size_t) ((unsigned char) id[0] << 8 | (unsigned char) id[1]) % NET->count;
}

void
net_add(const Station sta) {
    for(size_t i = net_hash(sta.id), j = 0, k; j < NET->count; ++j) {
        k = (i + j) % NET->count;
        if(!(NET->entries[k].used)) {
            NET->entries[k].station = sta;
            NET->entries[k].used = true;
            NET->entries[k].active = false;
            return;
        }
    }
    HH_UNREACHABLE;
}

const Station*
net_sta(const char id[static 2]) {
    for(size_t i = net_hash(id), j = 0, k; j < NET->count; ++j) {
        k = (i + j) % NET->count;
        if(!NET->entries[k].used) continue;
        if(memcmp(NET->entries[k].station.id, id, 2) == 0) {
            return &(NET->entries[k].station);
        }
    }
    return NULL;
}

bool*
net_sta_active(const char id[static 2]) {
    for(size_t i = net_hash(id), j = 0, k; j < NET->count; ++j) {
        k = (i + j) % NET->count;
        if(!NET->entries[k].used) continue;
        if(memcmp(NET->entries[k].station.id, id, 2) == 0) {
            return &(NET->entries[k].active);
        }
    }
    return NULL;
}

void
net_init(void) {
    HH_ASSERT(CAT->station_list != NULL, "No stations were parsed from raw catalogs.");
    NET->count = hh_arrlen(CAT->antenna_list);
    HH_CALLOC(NET->entries, sizeof(*(NET->entries)) * NET->count);
    Station sta;
    size_t len_sta = hh_arrlen(CAT->station_list);
    size_t len_pos = hh_arrlen(CAT->position_list);
    size_t len_eqp = hh_arrlen(CAT->equip_list);
    char sta_name_pos[8];
    size_t sta_sefd_count = 0;
    size_t sta_band_count;
    for(size_t i = 0, j, k; i < NET->count; ++i) {
        strncpy(sta.name, CAT->antenna_list[i].name, 8);
        sta.axes = CAT->antenna_list[i].axes;
        sta.axes_limits[0] = CAT->antenna_list[i].axes_limits[0];
        sta.axes_limits[1] = CAT->antenna_list[i].axes_limits[1];
        // SEFD
        sta_band_count = 0;
        for(j = 0; j < BAND_OTHER; ++j) sta.band[j] = false;
        for(j = 0; j < len_eqp; ++j) {
            if(cat_name_eq(sta.name, CAT->equip_list[j].name_ant)) {
                for(k = 0; k < 2; ++k) {
                    if(CAT->equip_list[j].bands[k] == BAND_OTHER) continue;
                    if(sta.band[CAT->equip_list[j].bands[k]]) {
                        HH_DBG("Encountered duplicate SEFD reading for %.*s's %c band.",
                            (int) cat_name_len(sta.name), sta.name, BAND_CODES[CAT->equip_list[j].bands[k]]);
                        // skip the new flux entry if the current one has more steps
                        if(CAT->equip_list[j].sefd[k] < sta.sefd[CAT->equip_list[j].bands[k]]) continue;
                    } else sta_band_count++;
                    sta.band[CAT->equip_list[j].bands[k]] = true;
                    sta.sefd[CAT->equip_list[j].bands[k]] = CAT->equip_list[j].sefd[k];
                }
            }
        }
        if(sta_band_count > 0) sta_sefd_count++;
        // use stations.cat as a fallback
        for(j = 0; j < len_sta; ++j) {
            if(cat_name_eq(sta.name, CAT->station_list[j].name_ant)) {
                memcpy(sta_name_pos, CAT->station_list[j].name_pos, 8);
                break;
            }
        }
        for(j = 0; j < len_pos; ++j) {
            if(cat_name_eq(CAT->position_list[j].name, sta.name)) {
                memcpy(sta.id, CAT->position_list[j].id, 2);
                sta.x = CAT->position_list[j].x;
                sta.y = CAT->position_list[j].y;
                sta.z = CAT->position_list[j].z;
                sta.lat = CAT->position_list[j].lat;
                sta.lon = CAT->position_list[j].lon;
                goto net_init_add_sta;
            }
        }
        for(j = 0; j < len_pos; ++j) {
            if(cat_name_eq(CAT->position_list[j].name, sta_name_pos)) {
                memcpy(sta.id, CAT->position_list[j].id, 2);
                sta.x = CAT->position_list[j].x;
                sta.y = CAT->position_list[j].y;
                sta.z = CAT->position_list[j].z;
                sta.lat = CAT->position_list[j].lat;
                sta.lon = CAT->position_list[j].lon;
                goto net_init_add_sta;
            }
        }
        HH_ERR("Failed to find position entry for %.*s. Skipping.", (int) cat_name_len(sta.name), sta.name);
        continue;
net_init_add_sta:
        // TODO: Consider handling of stations without SEFD readings (sta_band_count == 0)
        net_add(sta);
    }
    HH_MSG("Found SEFD readings for %zu out of %zu antennas.", sta_sefd_count, NET->count);
}

void
net_free(void) {
    free(NET->entries);
}

void
net_dump(void) {
    for(size_t i = 0; i < NET->count; ++i) {
        if(NET->entries[i].used) Station_dump(&(NET->entries[i].station));
    }
#if 0
    for(size_t i = 0, j; i < NET->count; ++i) {
        if(NET->entries[i].used) {
            printf("%c%c: ", NET->entries[i].station.id[0], NET->entries[i].station.id[1]);
            for(j = 0; j < BAND_OTHER; ++j)
                printf("%c ", NET->entries[i].station.band[j] ? BAND_CODES[j] : ' ');
            printf("\n");
        }
    }
#endif
}

bool
net_xml_parse(struct xml_node* root) {
    struct xml_node* general = xml_node_find(root, "general");
    if(general == NULL) return false;
    struct xml_node* stations = xml_node_find(general, "stations");
    if(stations == NULL) return false;
    struct xml_node* child;
    struct xml_string* name;
    const Station* sta;
    bool* active;
    for(size_t i = 0; i < xml_node_children(stations); ++i) {
        child = xml_node_child(stations, i);
        if(xml_node_name_equals(child, "station")) {
            name = xml_node_content(child);
            net_it(sta) {
                active = net_sta_active(sta->id);
                if(memcmp(sta->name, name->buffer, cat_name_len(sta->name)) == 0) {
                    HH_MSG("Added station: %.*s", (int) name->length, name->buffer);
                    *active = true;
                    break;
                } else active = NULL;
            }
            if(active == NULL)  HH_MSG("Failed to add station: %.*s", (int) name->length, name->buffer);
        }
    }
    return true;
}

//
// helper functions
//

size_t
NET_H__net_it(size_t i, const Station** sta, bool only_active) {
    (*sta) = NULL;
    while(i < NET->count) {
        (*sta) = &(NET->entries[i++].station);
        if(NET->entries[i].used) {
            if(!only_active || NET->entries[i - 1].active) return i;
        }
    }
    return SIZE_MAX;
}
