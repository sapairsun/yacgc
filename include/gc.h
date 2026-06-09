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

#ifndef C_GC_GC_H
#define C_GC_GC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gc_stats {
    size_t allocated_bytes;
    size_t allocated_objects;
    size_t last_collected_bytes;
    size_t last_collected_objects;
} gc_stats;

typedef enum gc_mode {
    GC_MODE_CONSERVATIVE = 0,
    GC_MODE_PRECISE = 1
} gc_mode;

void gc_init(void *stack_base);
void gc_shutdown(void);

#define GC_INIT()                          \
    do {                                  \
        void *gc__stack_base = &gc__stack_base; \
        gc_init(gc__stack_base);          \
    } while (0)

void gc_set_mode(gc_mode mode);
gc_mode gc_get_mode(void);

void gc_set_auto_collect(int enabled);
void gc_set_collect_threshold(size_t bytes);

void *gc_malloc(size_t size);
void *gc_calloc(size_t count, size_t size);
void *gc_realloc(void *ptr, size_t new_size);
void gc_free(void *ptr);

void gc_add_root(void **root_addr);
void gc_remove_root(void **root_addr);

void gc_collect(void);
gc_stats gc_get_stats(void);

#ifdef __cplusplus
}
#endif

#endif
