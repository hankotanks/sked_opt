#ifndef XML_UTIL_H__
#define XML_UTIL_H__

#include "xml.h"

struct xml_string {
	uint8_t const* buffer;
	size_t length;
};

bool
xml_node_name_equals(struct xml_node* node, const char* name);
struct xml_node* 
xml_node_find(struct xml_node* parent, const char* name);

#endif // XML_UTIL_H__
