#ifndef NETWORK_H__
#define NETWORK_H__

#include <stddef.h>

#include "xml.h"
#include "station.h"

// globals
extern struct NETWORK_H__NET* NET;

// macros
#define net_it(it_)        net_it_filter(it_, false)
#define net_it_active(it_) net_it_filter(it_, true)
#define net_count          (*((size_t*) NET))

// interface
const Station*
net_sta(const char id[static 2]);
bool*
net_sta_active(const char id[static 2]);
void
net_init(void);
void
net_free(void);
void
net_dump(void);
bool
net_xml_parse(struct xml_node* root);

// macro internals
#define net_it_filter(it_, only_active_) \
    for(size_t it_##_idx = NET_H__net_it(0, &it_, only_active_); \
        it_##_idx != SIZE_MAX; \
        it_##_idx = NET_H__net_it(it_##_idx, &it_, only_active_))
size_t NET_H__net_it(size_t, const Station**, bool);

#endif // NETWORK_H__
