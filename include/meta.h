#ifndef META_H__
#define META_H__

#include <stdbool.h>

struct META_H__META {
    char* path_parent;
    char* name;
    bool generate_statistics;
};

extern struct META_H__META* META;

void
meta_init(void);
void
meta_free(void);
const char*
meta_file(void);
const char*
meta_file_stat(void);

#endif // META_H__
