#include "xml_util.h"

#include "hh.h"
#include "xml.h"
#include <stdbool.h>

struct xml_document* 
xml_parse_document_skip_preamble(char* const contents) {
    if(contents == NULL) return NULL;
    char* offset = contents;
    char* temp;
    while(offset[0] != '\0') {
        if(strncmp(offset, "<?xml", 5) != 0) break;
        temp = strchr(offset, '\n');
        if(temp == NULL) return NULL;
        offset = temp + 1;
    }
    size_t contents_size = hh_arrlen(contents) - (size_t) (offset - contents);
    return xml_parse_document((uint8_t*) offset, contents_size);
}

bool
xml_string_equals(struct xml_string* str, const char* val) {
    if(str->length != strlen(val)) return false;
    return strncmp((char*) str->buffer, val, str->length) == 0;
}

bool
xml_node_name_equals(struct xml_node* node, const char* name) {
    return xml_string_equals(xml_node_name(node), name);
}

struct xml_node* 
xml_node_find(struct xml_node* parent, const char* name) {
    struct xml_node* child = NULL;
    for(size_t i = 0; i < xml_node_children(parent); ++i) {
        child = xml_node_child(parent, i);
        if(xml_node_name_equals(child, name)) return child;
    }
    return NULL;
}

struct xml_node*
xml_node_find_with_attr(struct xml_node* parent, const char* name, const char* attr, const char* val) {
    struct xml_node* child = NULL;
    for(size_t i = 0, j; i < xml_node_children(parent); ++i) {
        child = xml_node_child(parent, i);
        if(!xml_node_name_equals(child, name)) continue;
        for(j = 0; j < xml_node_attributes(child); ++j) {
            if(xml_string_equals(xml_node_attribute_name(child, j), attr))
                if(xml_string_equals(xml_node_attribute_content(child, j), val)) return child;
        }
    }
    return NULL;
}

