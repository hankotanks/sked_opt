#ifndef SCHED_H__
#define SCHED_H__

#include <stdbool.h>

#include "source.h"
#include "station.h"

#define SCHED_TYPES \
    X(SCHED_DEMO) \
    X(SCHED_DEFAULT)

enum sched_type {
#define X(ty_) ty_,
    SCHED_TYPES
#undef X
    SCHED_COUNT
};

#define SCHED_DECL(ty_) SCHED_H__load_##ty_
#define SCHED_IMPL(ty_) bool SCHED_DECL(ty_)(Output* const out)

typedef struct {
    const Source* target;
    uintptr_t* sta;
} Scan;

void
Scan_init(Scan* const scan, const Source* target);
void
Scan_free(Scan* scan);
void
Scan_add(Scan* const scan, const Station* sta);
void
Scan_dump(const Scan* const scan);

typedef struct { Scan** scans; } Output;

void
Output_init(Output* const out);
void
Output_free(Output* out);
void
Output_add(Output* const out, size_t seg, Scan scan);
void
Output_dump(const Output* const out);

void
sched_start(enum sched_type ty);

#define X(ty_) SCHED_IMPL(ty_);
    SCHED_TYPES
#undef X

#endif // SCHED_H__
