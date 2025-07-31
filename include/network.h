#ifndef NETWORK_H__
#define NETWORK_H__

#include <stddef.h>

#include "station.h"

typedef struct NETWORK_H__StationEntry StationEntry;
typedef struct {
    size_t count;
    StationEntry* entries;
} Network;

extern Network* net;

void
Network_add_sta(const Station sta);
bool* // Returns NULL if station not found, otherwise, returns pointer to station toggle
Network_get_sta(const char id[static 2], Station* out);
bool*
Network_get_sta_by_idx(const size_t idx, Station* out);
void
Network_init();
void
Network_free();
void
Network_dump();

#endif // NETWORK_H__
