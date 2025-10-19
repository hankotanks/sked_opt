#ifndef SOURCE_H__
#define SOURCE_H__

#include "cat.h"

typedef struct {
    char name[8];
    double raan, raan_rad, decl, decl_rad;
    double epoch;
    double crs[3];
    bool band[BAND_OTHER];
    struct flux_step* flux[BAND_OTHER]; // NOTE: Source does not own these flux arrays
} Source;

void
Source_dump(const Source* const src);

#endif // SOURCE_H__
