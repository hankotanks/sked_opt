#ifndef NETWORK_H__
#define NETWORK_H__

#if 0
#include "cat.h"

typedef struct { 
    int dummy;
} StationPos;

typedef struct {
    size_t count;
    double mask[2][30];
} StationMask;

typedef struct {
    char id[2], name[8];
    StationPos pos;
    StationMask mask;
} Station;

typedef struct {
    int dummy;
} Network;
#endif // temp

#endif // NETWORK_H__
