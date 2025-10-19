#ifndef MAP_H__
#define MAP_H__

#include <stddef.h>

#include "source.h"
#include "sky.h"
#include "station.h"
#include "network.h"

struct map {
    size_t count_sta, count_src, count_seg;
    char* map_sta;
    char* map_src;
};

#define map_seg_it(map_, it_) \
    for(it_ = 0; it_ < map_->count_seg; ++it_)

#define map_src_it(map_, it_) \
    for(size_t it_##_idx = MAP_H__map_src_it_helper(map_, 0, &it_); it_##_idx != SIZE_MAX; it_##_idx = MAP_H__map_src_it_helper(map_, it_##_idx + 1, &it_))

#define map_sta_it(map_, it_) \
    it_ = net_sta((map_)->map_sta); \
    for(size_t it_##_idx = 0; (it_##_idx < (map_)->count_sta) && (it_ = net_sta(&((map_)->map_sta[it_##_idx * 2]))); ++it_##_idx)

#define map_bln_it(map_, it_fst_, it_snd_) \
     it_fst_ = net_sta((map_)->map_sta); \
     for(size_t it_fst_##_idx = 0, it_snd_##_idx; (it_fst_##_idx < (map_)->count_sta) && (it_fst_ = net_sta(&((map_)->map_sta[it_fst_##_idx * 2]))); ++it_fst_##_idx) \
        for(it_snd_##_idx = 0, it_snd_ = net_sta((map_)->map_sta); (it_snd_##_idx < (map_)->count_sta) && (it_snd_ = net_sta(&((map_)->map_sta[it_snd_##_idx * 2]))); ++it_snd_##_idx) \
            if(it_fst_##_idx < it_snd_##_idx)

void
map_init(struct map* const map);
void
map_free(struct map* map);
size_t
map_sta_idx(const struct map* const map, const Station* sta);
size_t
map_src_idx(const struct map* const map, const Source* src);
const Station*
map_sta_get(const struct map* const map, size_t idx);
const Source*
map_src_get(const struct map* const map, size_t idx);

// internal
size_t
MAP_H__map_src_it_helper(const struct map* const map, size_t i, const Source** src);

#endif // MAP_H__
