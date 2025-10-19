#ifndef STATION_H__
#define STATION_H__

#include <stdbool.h>
#include <stdint.h>

#include "cat.h"
#include "source.h"

// station struct
typedef struct {
    char id[2];
    char name[8];
    double x, y, z;
    double lon, lat;
    enum dish_axes axes;
    struct dish_limits axes_limits[2];
    size_t mask_count;
    double mask[81]; // deg
    double mask_min; // deg
    bool band[BAND_OTHER];
    double sefd[BAND_OTHER];
} Station;

// interface
void
Station_dump(const Station* const sta);
bool
Station_src_visible(const Station* const sta, const Source* const src, unsigned int seconds);
unsigned int
Station_slew_time(const Station* const sta, const Source* const src[2], unsigned int seconds[2]);

// sky coverage indexer template
typedef size_t (*Station_sky_cov_idx)(const Station* const, const Source* const, unsigned int);

// sky coverage cell indexers
size_t
Station_sky_cov_idx_13v1(const Station* const sta, const Source* const src, unsigned int seconds);
size_t
Station_sky_cov_idx_13v2(const Station* const sta, const Source* const src, unsigned int seconds);
size_t
Station_sky_cov_idx_25v1(const Station* const sta, const Source* const src, unsigned int seconds);
size_t
Station_sky_cov_idx_25v2(const Station* const sta, const Source* const src, unsigned int seconds);
size_t
Station_sky_cov_idx_37v1(const Station* const sta, const Source* const src, unsigned int seconds);
size_t
Station_sky_cov_idx_37v2(const Station* const sta, const Source* const src, unsigned int seconds);

#endif // STATION_H__
