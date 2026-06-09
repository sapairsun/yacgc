#ifndef C_GC_GC_PRECISE_H
#define C_GC_GC_PRECISE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GC_SHADOW_INLINE_CAP 16

typedef struct gc_shadow_frame {
    struct gc_shadow_frame *prev;
    size_t len;
    size_t cap;
    void ***slots;
    void ***heap_slots;
    void **inline_slots[GC_SHADOW_INLINE_CAP];
} gc_shadow_frame;

void gc_shadow_enter(gc_shadow_frame *frame);
void gc_shadow_leave(gc_shadow_frame *frame);
void gc_shadow_root(gc_shadow_frame *frame, void **root_addr);

#define GC_SHADOW_BEGIN(name) gc_shadow_frame name; gc_shadow_enter(&(name))
#define GC_SHADOW_END(name) gc_shadow_leave(&(name))
#define GC_SHADOW_ROOT(frame_ptr, ptr_lvalue) gc_shadow_root((frame_ptr), (void **)&(ptr_lvalue))

#ifdef __cplusplus
}
#endif

#endif
