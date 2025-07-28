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
Station*
Network_get_sta(const Network* const net, const char id[static 2]);
void
Network_init(Network* const net);
void
Network_free(const Network* const net);
void
Network_dump(const Network* const net);

#endif // NETWORK_H__
