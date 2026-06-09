/*
MIT License

Copyright (c) 2026 yacgc contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef C_GC_GC_INTERNAL_H
#define C_GC_GC_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "../include/gc.h"
#include "../include/gc_precise.h"

#if !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>
#endif

#if !defined(_WIN32)
#include <pthread.h>
#endif

typedef struct gc_block {
    struct gc_block *next;
    size_t size;
    unsigned char marked;
} gc_block;

typedef struct gc_mark_stack {
    gc_block **items;
    size_t len;
    size_t cap;
} gc_mark_stack;

extern gc_block *g_blocks;
extern uintptr_t g_stack_lo;
extern uintptr_t g_stack_hi;
extern int g_stack_bounds_ready;
extern int g_stack_grows_down;
int gc_platform_get_stack_bounds(uintptr_t *lo, uintptr_t *hi);

extern void ***g_roots;
extern size_t g_roots_len;
extern size_t g_roots_cap;

typedef struct gc_thread {
    struct gc_thread *next;
    uintptr_t stack_lo;
    uintptr_t stack_hi;
    uintptr_t parked_sp;
    uintptr_t parked_regs_lo;
    uintptr_t parked_regs_hi;
    uintptr_t shadow_top;
#if defined(_WIN32)
    void *os_thread;
    unsigned long os_thread_id;
#else
    pthread_t os_thread;
#endif
    int attached;
    int parked;
} gc_thread;

extern gc_thread *g_threads;
extern size_t g_threads_count;

gc_thread *gc_threads_current(void);
void gc_threads_init_once(void);
void gc_threads_attach_current(void);
void gc_threads_set_shadow_top(uintptr_t shadow_top);
int gc_threads_stop_the_world(void);
void gc_threads_start_the_world(void);

extern size_t g_allocated_bytes;
extern size_t g_allocated_objects;
extern size_t g_last_collected_bytes;
extern size_t g_last_collected_objects;

extern int g_auto_collect_enabled;
extern size_t g_collect_threshold;
extern int g_collect_threshold_user_set;
extern int g_in_collect;

extern gc_mode g_mode;

extern int g_conservative_stack_scanning_enabled;

extern int g_lock_initialized;
#if !defined(__STDC_NO_ATOMICS__)
extern atomic_flag g_lock;
#endif
void gc_lock(void);
void gc_unlock(void);

void gc_mark_stack_push(gc_mark_stack *st, gc_block *b);
gc_block *gc_find_block_containing(void *p);
void gc_mark_ptr(gc_mark_stack *st, void *p);
void gc_mark_range(gc_mark_stack *st, uintptr_t lo, uintptr_t hi);
void gc_mark_object(gc_mark_stack *st, gc_block *b);

void gc_sweep(void);

void gc_mode_conservative_mark_roots(gc_mark_stack *st);
void gc_mode_precise_mark_roots(gc_mark_stack *st);

#endif
