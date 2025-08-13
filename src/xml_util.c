#include "xml_util.h"

#include "hh.h"

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
xml_node_name_equals(struct xml_node* node, const char* name) {
    size_t length = strlen(name);
    struct xml_string* node_name = xml_node_name(node);
    if(node_name->length != length) return false;
    return strncmp((char*) node_name->buffer, name, node_name->length) == 0;
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
