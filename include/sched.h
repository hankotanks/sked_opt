#ifndef SCHED_H__
#define SCHED_H__

#include <stdbool.h>

#define SCHED_TYPES \
    X(SCHED_DEMO)

enum sched_type {
#define X(ty_) ty_,
    SCHED_TYPES
#undef X
    SCHED_COUNT
};

#define SCHED_DECL(ty_) SCHED_H__load_##ty_
#define SCHED_IMPL(ty_) bool SCHED_DECL(ty_)(Output* const out)

typedef struct {
    int temp;
} Output;

void
sched_start(enum sched_type ty);

#define X(ty_) SCHED_IMPL(ty_);
    SCHED_TYPES
#undef X

#endif // SCHED_H__
