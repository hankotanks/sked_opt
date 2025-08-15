#include "cat.h"

static struct cat_t CAT_H__cat = {
#define X(type_) .type_##_list = NULL,
    CAT_LIST
#undef X
}; struct cat_t* cat = &CAT_H__cat;

void cat_init(const char* path) {
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
        HH_ASSERT(file != NULL, "Failed to open catalog [%s].", path_file); \
        hh_arradd(cat->type_##_list, 1); \
        while(hh_getline(&line, &len, file) != -1) { \
            line_temp = hh_skip_whitespace(line); \
            if(line_temp[0] == '*' || line_temp[0] == '\0') continue; \
            if(CAT_H__##type_##_parse(line, &hh_arrlast(cat->type_##_list))) hh_arradd(cat->type_##_list, 1); \
        } \
        if(hh_arrlen(cat->type_##_list) > 0) hh_arrpop(cat->type_##_list); \
        HH_MSG("Parsed %zu entries from [%s].", \
            hh_arrlen(cat->type_##_list), path_file); \
        fclose(file); \
        hh_arrfree(path_file); \
    } while(0);
    CAT_LIST
#undef X
    if(line != NULL) free(line);
}

void cat_free(void) {
#define X(type_) hh_arrfree(cat->type_##_list);
    CAT_LIST
#undef X
}

//
// helper functions
//

size_t
cat_name_len(const char name[static 8]) {
    return memchr(name, '\0', 8) ? strlen(name) : 8;
}

bool
cat_name_eq(const char fst[static 8], const char snd[static 8]) {
    size_t len = cat_name_len(fst);
    if(len != cat_name_len(snd)) return false;
    return memcmp(fst, snd, len) == 0;
}

void
cat_name_print(const char name[static 8]) {
    printf("%.*s", (int) cat_name_len(name), name);
}

bool
cat_name_contains(const char name[8], const char* sub) {
    size_t len_name = 8;
    for(size_t i = 0; i < 8; ++i) {
        if(name[i] == '\0') {
            len_name = i;
            break;
        }
    }
    size_t len_sub = strlen(sub);
    if(len_sub == 0) return true;
    for(size_t i = 0; i + len_sub <= len_name; ++i) 
        if(memcmp(name + i, sub, len_sub) == 0) return true;
    return false;
}
