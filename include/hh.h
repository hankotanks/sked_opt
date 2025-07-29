#ifndef HH_H__
#define HH_H__

#ifndef _WIN32
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif // not _WIN32

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

//
// MISC
//

#define HH_MAX(x, y) (((x) > (y)) ? (x) : (y))
#define HH_MIN(x, y) (((x) < (y)) ? (x) : (y))


//
// LOGGING
//

enum {
    HH_LOG_ERR = 1 << 0,
    HH_LOG_MSG = 1 << 1,
    HH_LOG_DBG = 1 << 2
};

#ifdef HH_LOG

#define HH_DBG(...) do { \
    if(HH_LOG >= HH_LOG_DBG) { \
        printf("DEBUG [%s:%d]: ", __FILE__, __LINE__); \
        printf(__VA_ARGS__); \
        printf("\n"); \
    } \
} while(0)

#define HH_MSG(...) do { \
    if(HH_LOG >= HH_LOG_MSG) { \
        printf("INFO [%s:%d]: ", __FILE__, __LINE__); \
        printf(__VA_ARGS__); \
        printf("\n"); \
    } \
} while(0)

#define HH_ERR(...) do { \
    if(HH_LOG >= HH_LOG_ERR) { \
        fprintf(stderr, "ERROR [%s:%d]: ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } \
} while(0)

#else
#define HH_DBG(...)
#define HH_MSG(...)
#define HH_ERR(...)
#endif

//
// MISC
//

#define HH_ASSERT(cond, ...) do { for(; !(cond); assert(cond)) HH_ERR(__VA_ARGS__); } while(0)

#define HH_CHECK_STREAM(stream, cond, ...) if(!(cond)) { \
		fclose((stream)); \
		HH_ERR(__VA_ARGS__); \
	} \
	if(!(cond))

#define HH_UNREACHABLE HH_ASSERT(false, "Unreachable!")

#define HH_MALLOC(var, size) do { \
		(var) = malloc(size); \
		HH_ASSERT(var != NULL, "Failed to allocate [%s].", #var); \
	} while(0);

#define HH_CALLOC(var, size) do { \
		(var) = calloc(1, size); \
		HH_ASSERT(var != NULL, "Failed to allocate [%s].", #var); \
	} while(0);

//
// DYNAMIC ARRAYS
//

// Adapted from...
// stb_ds.h - v0.67 - public domain data structures - Sean Barrett 2019

// internal array components
typedef struct { size_t length, capacity; } hh_arrheader_t;
void* 
hh_arrgrow_impl(void*, size_t, size_t, size_t);

#define hh_arrheader(arr)    ((hh_arrheader_t*) (arr) - 1)
#define hh_arrgrow(arr, n)   ((arr) = hh_arrgrow_impl((arr), sizeof *(arr), (n), 0))
#define hh_arrcheck(arr, n)  ((!(arr) || hh_arrheader(arr)->length + (n) > hh_arrheader(arr)->capacity) ? (hh_arrgrow(arr, n), 0) : 0)
#define hh_arrcap(arr)       ((arr) ? hh_arrheader(arr)->capacity : 0)
#define hh_arrcapset(arr, n) ((arr) = hh_arrgrow_impl((arr), sizeof *(arr), 0, (n))) 
#define hh_arrlen(arr)       ((arr) ? hh_arrheader(arr)->length : 0)
#define hh_arrlenset(arr, n) ((hh_arrcap(arr) < (size_t) (n) ? hh_arrcapset((arr), (size_t) (n)), 0 : 0), (arr) ? hh_arrheader(arr)->length = (size_t) (n) : 0)
#define hh_arrput(arr, val)  (hh_arrcheck(arr, 1), (arr)[(hh_arrheader(arr)->length)++] = (val))
#define hh_arrpop(arr)       ((hh_arrheader(arr)->length)--, (arr)[hh_arrheader(arr)->length])
// add n zero-initialized elements to array and return the index of the first added element
#define hh_arradd(arr, n)    ((void) (hh_arrcheck(arr, n)), (n) ? (memset((arr) + hh_arrheader(arr)->length, 0, sizeof *(arr) * (n)), hh_arrheader(arr)->length += (n), hh_arrheader(arr)->length - (n)) : hh_arrlen(arr))
#define hh_arrlast(arr)      ((arr)[hh_arrheader(arr)->length - 1])
#define hh_arrfree(arr)      ((void) ((arr) ? free(hh_arrheader(arr)) : (void) 0), (arr) = NULL)

//
// STRINGS
//

// NOTE: Assumes that the end of the array is currently \0
#define hh_strput(arr, str) do { \
    if(hh_arrlen(arr) == 0) hh_arrput(arr, '\0'); \
    else if(hh_arrlast(arr) != '\0') hh_arrput(arr, '\0'); \
    size_t _off = hh_arradd(arr, strlen((str))); \
    strcpy((arr) + _off - 1, (str)); \
} while(0)

// 
// PATH
//

char*
hh_path(const char* raw); // allocates a new path
bool
hh_path_exists(const char* path);
bool
hh_path_is_file(const char* path);
char*
hh_path_join(char* path, const char* sub); // extends the given path
const char*
hh_path_name(const char* path); // returns the pointer to the filename part of the path
char*
hh_path_parent(const char* path); // returns a new allocated path
char*
hh_path_parent_in_place(char* path); // returns truthy if path has a parent

//
// EDITION
//

#define HH_EDITION_89 0L
#define HH_EDITION_90 1L
#define HH_EDITION_94 199409L
#define HH_EDITION_99 199901L
#define HH_EDITION_11 201112L
#define HH_EDITION_17 201710L
#define HH_EDITION_23 202311L

#ifdef __STDC__
#define HH_EDITION HH_EDITION_89
#ifdef __STDC_VERSION__
#ifdef HH_EDITION
#undef HH_EDITION
#endif // HH_EDITION
#define HH_EDITION HH_EDITION_90
#if(__STDC_VERSION__ >= HH_EDITION_94)
#ifdef HH_EDITION
#undef HH_EDITION
#endif // HH_EDITION
#define HH_EDITION HH_EDITION_94
#endif // 199409L
#if(__STDC_VERSION__ >= HH_EDITION_99)
#ifdef HH_EDITION
#undef HH_EDITION
#endif // HH_EDITION
#define HH_EDITION HH_EDITION_99
#endif // 199901L
#if(__STDC_VERSION__ >= HH_EDITION_11)
#ifdef HH_EDITION
#undef HH_EDITION
#endif // HH_EDITION
#define HH_EDITION HH_EDITION_11
#endif // 201112L
#if(__STDC_VERSION__ >= HH_EDITION_17)
#ifdef HH_EDITION
#undef HH_EDITION
#endif // HH_EDITION
#define HH_EDITION HH_EDITION_17
#endif // 201710L
#if(__STDC_VERSION__ >= HH_EDITION_23)
#ifdef HH_EDITION
#undef HH_EDITION
#endif // HH_EDITION
#define HH_EDITION HH_EDITION_23
#endif // 202311L
#endif // __STDC_VERSION__
#endif // __STD__

#define HH_EDITION_SUPPORTED(x) (HH_EDITION >= (x))

//
// NetBSD: getline.c,v 1.2 2014/09/16 17:23:50 christos Exp
//

ptrdiff_t
hh_getdelim(char** buf, size_t* bufsiz, int delimiter, FILE* fp);
ptrdiff_t
hh_getline(char** buf, size_t* bufsiz, FILE* fp);

//
// PARSING
//

typedef struct {
	const char* ptr;
	size_t len;
} hh_span_t;

bool
hh_span_next(hh_span_t* span);
bool
hh_span_double(const hh_span_t span, double* out);
bool
hh_span_long(const hh_span_t span, long* out);
bool
hh_span_size_t(const hh_span_t span, size_t* out);
bool
hh_span_equals(const hh_span_t span, const char* other);
const char*
hh_skip_whitespace(const char* ptr);
bool
hh_starts_with(const char* ptr, const char* prefix);

#endif // HH_H__

//
//
//

#ifdef HH_IMPL

#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#endif // _WIN32

void*
hh_arrgrow_impl(void* arr, size_t elemsize, size_t addlen, size_t min_cap) {
  hh_arrheader_t temp = {0}; 
  void *b;
  size_t min_len = hh_arrlen(arr) + addlen;
  (void) sizeof(temp);
  if(min_len > min_cap) min_cap = min_len;
  if(min_cap <= hh_arrcap(arr)) return arr;
  if(min_cap < 2 * hh_arrcap(arr)) min_cap = 2 * hh_arrcap(arr);
  else if(min_cap < 4) min_cap = 4;
  b = realloc((arr) ? hh_arrheader(arr) : 0, elemsize * min_cap + sizeof(hh_arrheader_t));
  b = (char*) b + sizeof(hh_arrheader_t);
  if(arr == NULL) hh_arrheader(b)->length = 0;
  hh_arrheader(b)->capacity = min_cap;
  return b;
}

char*
hh_path_impl(const char* raw) {
	char* path = NULL;
	hh_strput(path, raw);
	if(path == NULL) return NULL;
	hh_arrpop(path);
	for(char* curr = path; *curr != '\0'; ++curr) if(*curr == '\\') *curr = '/';
	hh_arrput(path, '\0');
	return path;
}

char*
hh_path(const char *raw) {
	char* raw_abs = NULL;
	char* path = NULL;
#ifdef _WIN32
    DWORD len = GetFullPathNameA(raw, 0, NULL, NULL);
    if(len == 0) return NULL;
    raw_abs = malloc(len);
    if(!raw_abs) return NULL;
    if(GetFullPathNameA(raw, len, raw_abs, NULL) == 0) {
        free(raw_abs);
        return NULL;
    }
	path = hh_path_impl(raw_abs);
	if(path && path[0] >= 'a' && path[0] <= 'z') path[0] -= ('a' - 'A');
#else
	if(raw[0] == '.' && (raw[1] == '/' || raw[1] == '\\')) {
		raw_abs = realpath(".", NULL);
		path = hh_path_impl(raw_abs);
		hh_strput(path, raw + 1);
	} else if(raw[0] == '.' && raw[1] == '.' && (raw[2] == '/' || raw[2] == '\\')) {
		raw_abs = realpath("..", NULL);
		path = hh_path_impl(raw_abs);
		hh_strput(path, raw + 2);
	} else {
		raw_abs = realpath(raw, NULL);
		path = hh_path_impl(raw_abs);
	}
#endif
	if(raw_abs) free(raw_abs);
	if(path == NULL) return NULL;
	for(char* curr = path; *curr != '\0'; ++curr) if(*curr == '\\') *curr = '/';
	hh_arrpop(path);
	if(hh_arrlast(path) == '/' && hh_arrlen(path) > 2) hh_arrpop(path);
	hh_arrput(path, '\0');
	return path;
}

bool
hh_path_exists(const char* path) {
#ifdef _WIN32
	return _access(path, 0) == 0;
#else
	return access(path, 0) == 0;
#endif
}

bool
hh_path_is_file(const char* path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
#endif
}

char*
hh_path_join(char* path, const char* sub) {
	if(sub[0] == '/' || sub[0] == '\\') ++sub;
	hh_arrpop(path);
	if(hh_arrlast(path) != '/') hh_strput(path, "/");
	hh_strput(path, sub);
	hh_arrpop(path);
	if(hh_arrlast(path) == '/' && hh_arrlen(path) > 2) hh_arrpop(path);
	hh_arrput(path, '\0');
	return path;
}

const char*
hh_path_name(const char* path) {
	const char *prev = path;
    for(const char *p = path; *p; ++p) if(*p == '/') prev = p + 1;
	if(prev[0] == '\0') return NULL;
    return prev;
}

char*
hh_path_parent(const char* path) {
	char* path_parent = NULL;
	hh_strput(path_parent, path);
	while(hh_arrlast(path_parent) != '/') hh_arrpop(path_parent);
#ifdef _WIN32
	if(hh_arrlen(path_parent) == 3 && path_parent[0] >= 'A' && path_parent[0] <= 'Z' && path_parent[1] == ':' && path_parent[2] == '/') {
		if(hh_arrlen(path) == 4) hh_arrfree(path_parent);
		else hh_arrput(path_parent, '\0');
	} else {
		hh_arrpop(path_parent);
		hh_arrput(path_parent, '\0');
	}
#else
	if(hh_arrlen(path_parent) == 1 && path_parent[0] == '/') {
		if(hh_arrlen(path) == 2) hh_arrfree(path_parent);
		else hh_arrput(path_parent, '\0');
	} else {
		hh_arrpop(path_parent);
		hh_arrput(path_parent, '\0');
	}
#endif
	return path_parent;
}

char*
hh_path_parent_in_place(char* path) {
	while(hh_arrlast(path) != '/') hh_arrpop(path);
#ifdef _WIN32
	if(hh_arrlen(path) == 3 && path[0] >= 'A' && path[0] <= 'Z' && path[1] == ':' && path[2] == '/') {
		if(hh_arrlen(path) == 4) hh_arrfree(path);
		else hh_arrput(path, '\0');
	} else {
		hh_arrpop(path);
		hh_arrput(path, '\0');
	}
#else
	if(hh_arrlen(path) == 1 && path[0] == '/') {
		if(hh_arrlen(path) == 2) hh_arrfree(path);
		else hh_arrput(path, '\0');
	} else {
		hh_arrpop(path);
		hh_arrput(path, '\0');
	}
#endif
	return path;
}

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

ptrdiff_t
hh_getdelim(char** buf, size_t* bufsiz, int delimiter, FILE* fp) {
	char *ptr, *eptr;
	if(*buf == NULL || *bufsiz == 0) {
		*bufsiz = BUFSIZ;
		if ((*buf = (char*) malloc(*bufsiz)) == NULL) return -1;
	}
	for(ptr = *buf, eptr = *buf + *bufsiz;;) {
		int c = fgetc(fp);
		if(c == -1) {
			if(feof(fp)) {
				ptrdiff_t diff = ptr - *buf;
				if(diff != 0) {
					*ptr = '\0';
					return diff;
				}
			}
			return -1;
		}
		*ptr++ = (char) c;
		if(c == delimiter) {
			*ptr = '\0';
			return ptr - *buf;
		}
		if(ptr + 2 >= eptr) {
			char *nbuf;
			size_t nbufsiz = *bufsiz * 2;
			ptrdiff_t d = ptr - *buf;
			if((nbuf = (char*) realloc(*buf, nbufsiz)) == NULL) return -1;
			*buf = nbuf;
			*bufsiz = nbufsiz;
			eptr = nbuf + nbufsiz;
			ptr = nbuf + d;
		}
	}
}

ptrdiff_t
hh_getline(char** buf, size_t* bufsiz, FILE* fp) {
	return hh_getdelim(buf, bufsiz, '\n', fp);
}

bool
hh_span_next(hh_span_t* span) {
	span->ptr += span->len;
	span->len = 0;
	const char* ptr = span->ptr;
    while(strchr(" \t\r\n", *ptr) && (*ptr) != '\0') ++ptr;
	span->ptr = ptr;
    while(strchr(" \t\r\n", *ptr) == NULL && (*ptr) != '\0') ++ptr;
	span->len = (size_t) (ptr - span->ptr);
	return (span->len != 0);
}

bool
hh_span_double(const hh_span_t span, double* out) {
	char* endptr = NULL;
	double temp;
    temp = strtod(span.ptr, &endptr);
    if(endptr == NULL) return false;
    if(endptr != (span.ptr + (ptrdiff_t) span.len)) return false;
	*out = temp;
	return true;
}

bool
hh_span_long(const hh_span_t span, long* out) {
	char* endptr = NULL;
	long temp;
    temp = strtol(span.ptr, &endptr, 10);
    if(endptr == NULL) return false;
    if(endptr != (span.ptr + (ptrdiff_t) span.len)) return false;
	*out = temp;
	return true;
}

bool
hh_span_size_t(const hh_span_t span, size_t* out) {
	long temp;
	if(!hh_span_long(span, &temp)) return false;
	if(temp < 0) return false;
	*out = (size_t) temp;
	return true;
}

bool
hh_span_equals(const hh_span_t span, const char* other) {
	size_t len = strlen(other);
	if(span.len != len) return false;
	return strncmp(span.ptr, other, span.len) == 0;
}

const char*
hh_skip_whitespace(const char* ptr) {
	while(strchr(" \t\r\n", *ptr) && (*ptr) != '\0') ++ptr;
	return ptr;
}

bool
hh_starts_with(const char* ptr, const char* prefix) {
	return strncmp(ptr, prefix, strlen(prefix)) == 0;
}

#endif // HH_IMPL
