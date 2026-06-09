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
