#ifndef NETWORK_H__
#define NETWORK_H__

#include <stddef.h>

#include "station.h"

typedef struct NETWORK_H__StationEntry StationEntry;
typedef struct {
    size_t count;
    StationEntry* entries;
} Network;

void
Network_add_sta(const Network* const net, const Station sta);
bool
Network_get_sta(const Network* const net, const char id[static 2], Station** out);
bool
Network_get_sta_by_idx(const Network* const net, const size_t idx, Station** out);
void
Network_init(Network* const net);
void
Network_free(const Network* const net);
void
Network_dump(const Network* const net);

#endif // NETWORK_H__
