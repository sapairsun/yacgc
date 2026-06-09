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
