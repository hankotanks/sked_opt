#include "map.h"

#include "hh.h"

#include "station.h"
#include "source.h"
#include "sky.h"
#include "network.h"
#include "time_sys.h"

void
map_init(struct map* const map) {
    // build network map
    map->count_sta = 0;
    const Station* sta;
    net_it_active(sta) ++(map->count_sta);
    HH_CALLOC(map->map_sta, map->count_sta * 2 + 1);
    size_t i = 0;
    net_it_active(sta) {
        map->map_sta[i++] = sta->id[0];
        map->map_sta[i++] = sta->id[1];
    }
    // build sky map
    map->count_src = 0;
    const Source* src;
    sky_it_active(src) (map->count_src)++;
    HH_CALLOC(map->map_src, map->count_src * 8 + 1);
    size_t j = 0, k;
    sky_it_active(src) {
        k = hh_strnlen(src->name, 8);
        memcpy(map->map_src + j * 8, src->name, k);
        for(; k < 8; ++k) map->map_src[j * 8 + k] = ' ';
        j++;
    }
    // segments
    map->count_seg = TIME_SYS->duration / TIME_SYS->scan_length;
}

void
map_free(struct map* map) {
    free(map->map_sta);
    free(map->map_src);
}

size_t
map_sta_idx(const struct map* const map, const Station* sta) {
    HH_ASSERT(sta != NULL, "Unreachable!");
    for(size_t i = 0; i < map->count_sta; ++i) {
        if(map_sta_get(map, i) == sta) return i;
    }
    return SIZE_MAX;
}

size_t
map_src_idx(const struct map* const map, const Source* src) {
    HH_ASSERT(src != NULL, "Unreachable!");
    for(size_t i = 0; i < map->count_src; ++i) {
        if(map_src_get(map, i) == src) return i;
    }
    return SIZE_MAX;
}

const Station*
map_sta_get(const struct map* const map, size_t idx) {
    return net_sta(&(map->map_sta[idx * 2]));
}

const Source*
map_src_get(const struct map* const map, size_t idx) {
    char buf[8];
    memcpy(buf, &(map->map_src[idx * 8]), 8);
    for(size_t j = 8; j-- > 0;) {
        if(buf[j] == ' ') buf[j] = '\0';
        else break;
    }
    return sky_src(buf);
}

size_t
MAP_H__map_src_it_helper(const struct map* const map, size_t i, const Source** src) {
    if(i >= map->count_src) {
        (*src) = NULL;
        return SIZE_MAX;
    }
    char buf[8];
    memcpy(buf, &(map->map_src[8 * i++]), 8);
    for(size_t j = 8; j-- > 0;) {
        if(buf[j] == ' ') buf[j] = '\0';
        else break;
    }
    if(((*src) = sky_src(buf)) == NULL) return SIZE_MAX;
    return i - 1;
}
