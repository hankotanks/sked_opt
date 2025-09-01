#ifndef SOURCE_H__
#define SOURCE_H__

#include "cat.h"

typedef struct {
    char name[8];
    double raan, decl, epoch, crs[3];
    bool band[BAND_OTHER];
    struct flux_step* flux[BAND_OTHER];
} Source;

void
Source_dump(const Source* const src);

#endif // SOURCE_H__
