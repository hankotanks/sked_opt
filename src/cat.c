#include "cat.h"

static struct cat_t CAT_H__cat = {
#define X(type_) .type_##_list = NULL,
    CAT_LIST
#undef X
}; struct cat_t* cat = &CAT_H__cat;

void cat_parse(const char* path) {
    FILE* file;
    char* path_file;
    char* line = NULL;
    const char* line_temp;
    size_t len;
#define X(type_) \
    do { \
        path_file = hh_path(path); \
        hh_path_join(path_file, CAT_H__##type_##_file); \
        file = fopen(path_file, "r"); \
        HH_ASSERT_MSG(file, "Failed to open catalog [%s].", path_file); \
        hh_arradd(cat->type_##_list, 1); \
        while(hh_getline(&line, &len, file) != -1) { \
            line_temp = hh_skip_whitespace(line); \
            if(line_temp[0] == '*' || line_temp[0] == '\0') continue; \
            if(CAT_H__##type_##_parse(line, &hh_arrlast(cat->type_##_list))) hh_arradd(cat->type_##_list, 1); \
        } \
        HH_MSG("Parsed %zu entries from [%s].", \
            hh_arrlen(cat->type_##_list), path_file); \
        fclose(file); \
        hh_arrfree(path_file); \
    } while(0);
    CAT_LIST
#undef X
}
