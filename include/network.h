#ifndef NETWORK_H__
#define NETWORK_H__

#include <stddef.h>

#include "xml.h"
#include "station.h"

typedef struct NETWORK_H__StationEntry StationEntry;
typedef struct {
    size_t count;
    StationEntry* entries;
} Network;

extern Network* net;

void
net_add_sta(const Station sta);
bool* // Returns NULL if station not found, otherwise, returns pointer to station toggle
net_get_sta(const char id[static 2], Station* out);
bool*
net_get_sta_by_idx(const size_t idx, Station* out);
void
net_init(void);
void
net_free(void);
void
net_dump(void);
void
net_xml_parse(struct xml_node* root);

#endif // NETWORK_H__
