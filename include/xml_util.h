#ifndef XML_UTIL_H__
#define XML_UTIL_H__

#include "xml.h"

struct xml_string {
	uint8_t const* buffer;
	size_t length;
};

struct xml_document* 
xml_parse_document_skip_preamble(char* const contents);

bool
xml_string_equals(struct xml_string* str, const char* val);
bool
xml_node_name_equals(struct xml_node* node, const char* name);
struct xml_node* 
xml_node_find(struct xml_node* parent, const char* name);
struct xml_node*
xml_node_find_with_attr(struct xml_node* parent, const char* name, const char* attr, const char* val);

#endif // XML_UTIL_H__
