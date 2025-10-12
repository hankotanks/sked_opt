#ifndef SCHED_H__
#define SCHED_H__

#include <stdbool.h>

#include "meta.h"
#include "source.h"
#include "station.h"

// tags of implemented schedules
#define SCHED_TYPES \
    X(SCHED_DEFAULT) \
    X(SCHED_EXPR)

// define schedule tags as enum values
enum sched_type {
#define X(ty_) ty_,
    SCHED_TYPES
#undef X
    SCHED_COUNT
};

// macros
#define SCHED_IMPL(ty_) bool SCHED_H__load_##ty_(Sched* const out)

// start the requested schedule
void
start(enum sched_type ty);

// interface
typedef struct SCHED_H__Sched Sched;
void
Sched_init(Sched* const out);
void
Sched_push_begin(Sched* const out, size_t seg, const Source* const target);
void
Sched_push(Sched* const out, const Station* const sta);
void
Sched_push_end(Sched* const out);
void
Sched_dump(const Sched* const out);
void
Sched_dump_activity(const Sched* const out);

// forward declaration of scheduling functions
#define X(ty_) SCHED_IMPL(ty_);
    SCHED_TYPES
#undef X

#endif // SCHED_H__
