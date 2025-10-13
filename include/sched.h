#ifndef SCHED_H__
#define SCHED_H__

#include <X11/Xlib.h>
#include <stdbool.h>
#include <stddef.h>

#include "source.h"
#include "station.h"
#include "map.h"

// tags of implemented schedules
#define SCHED_TYPES \
    X(SCHED_DEFAULT)

// define schedule tags as enum values
enum sched_type {
#define X(ty_) ty_,
    SCHED_TYPES
#undef X
    SCHED_COUNT
};

// enum representing station states at each segment
enum station_state {
    STATE_IDLE = 0,
    STATE_SLEW,
    STATE_SCAN,
    STATE_FAIL
};

// incomplete types
typedef struct SCHED_H__Sched Sched;

// TODO: Consider making this incomplete again
typedef struct {
    size_t* sta, target;
} Scan;

// macros
#define SCHED_IMPL(ty_) bool SCHED_H__load_##ty_(Sched* const out)

// interface for Sched
Sched*
Sched_init(enum sched_type ty);
void
Sched_free(Sched* out);
void
Sched_push_begin(Sched* const out, size_t seg, const Source* const target);
void
Sched_push(Sched* const out, const Station* const sta);
void
Sched_push_end(Sched* const out);
void
Sched_dump(const Sched* const out); 
// returns the number of scans in this segment
size_t
Sched_get(const Sched* const out, size_t seg, Scan** const scans);
const enum station_state*
Sched_get_activity(const Sched* const out, const Station* const sta);
const struct map*
Sched_map(const Sched* const out);


// forward declaration of scheduling functions
#define X(ty_) SCHED_IMPL(ty_);
    SCHED_TYPES
#undef X

#endif // SCHED_H__
