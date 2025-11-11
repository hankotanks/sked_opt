#include "meta.h"

#include "hh.h"

#include "time_sys.h"

static struct META_H__META META_H__META; 
struct META_H__META* META = &META_H__META;

static char* META_NAME_FILE = NULL;
static char* META_NAME_FILE_STAT = NULL;

void
meta_init(void) {
    META->path_parent = NULL;
    META->name = NULL;
    META->generate_statistics = false;
}

void
meta_free(void) {
    hh_arrfree(META->path_parent);
    hh_arrfree(META->name);
    hh_arrfree(META_NAME_FILE);
    hh_arrfree(META_NAME_FILE_STAT);
}

const char*
meta_file(void) {
    if(META->name == NULL) return time_sys_file();
    hh_arrclear(META_NAME_FILE);
    hh_strput(META_NAME_FILE, META->name);
    hh_strput(META_NAME_FILE, ".skd");
    return META_NAME_FILE;
}

const char*
meta_file_stat(void) {
    hh_arrclear(META_NAME_FILE_STAT);
    if(META->name == NULL) {
        hh_strput(META_NAME_FILE_STAT, time_sys_file());
        // strip extension
        for(size_t i = 5; i > 0; --i) HH_ASSERT(hh_arrpop(META_NAME_FILE_STAT) == (".skd")[i - 1], "Unreachable!");
        hh_arrput(META_NAME_FILE_STAT, '\0');    
    } else hh_strput(META_NAME_FILE_STAT, META->name);
    // add postfix and csv suffix
    hh_strput(META_NAME_FILE_STAT, "_stat.csv");
    return META_NAME_FILE_STAT;
}
