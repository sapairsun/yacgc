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

#include "gc_internal.h"

#include <stdlib.h>
#include <string.h>

static _Thread_local gc_shadow_frame *t_shadow_top = NULL;

void gc_shadow_enter(gc_shadow_frame *frame) {
    if (!frame) {
        return;
    }
    frame->prev = t_shadow_top;
    frame->len = 0;
    frame->cap = GC_SHADOW_INLINE_CAP;
    frame->slots = (void ***)frame->inline_slots;
    frame->heap_slots = NULL;
    t_shadow_top = frame;
    gc_threads_set_shadow_top((uintptr_t)t_shadow_top);
}

void gc_shadow_leave(gc_shadow_frame *frame) {
    if (!frame) {
        return;
    }
    if (t_shadow_top == frame) {
        t_shadow_top = frame->prev;
    } else {
        for (gc_shadow_frame *cur = t_shadow_top; cur; cur = cur->prev) {
            if (cur->prev == frame) {
                cur->prev = frame->prev;
                break;
            }
        }
    }
    free(frame->heap_slots);
    frame->heap_slots = NULL;
    frame->slots = NULL;
    frame->len = 0;
    frame->cap = 0;
    frame->prev = NULL;
    gc_threads_set_shadow_top((uintptr_t)t_shadow_top);
}

void gc_shadow_root(gc_shadow_frame *frame, void **root_addr) {
    if (!frame || !root_addr) {
        return;
    }
    for (size_t i = 0; i < frame->len; i++) {
        if (frame->slots[i] == root_addr) {
            return;
        }
    }

    if (frame->len == frame->cap) {
        size_t new_cap = frame->cap ? (frame->cap * 2) : GC_SHADOW_INLINE_CAP;
        void ***new_slots = NULL;

        if (frame->heap_slots) {
            new_slots = (void ***)realloc(frame->heap_slots, new_cap * sizeof(*new_slots));
            if (!new_slots) {
                abort();
            }
            frame->heap_slots = new_slots;
            frame->slots = new_slots;
        } else {
            new_slots = (void ***)malloc(new_cap * sizeof(*new_slots));
            if (!new_slots) {
                abort();
            }
            memcpy(new_slots, frame->inline_slots, frame->len * sizeof(*new_slots));
            frame->heap_slots = new_slots;
            frame->slots = new_slots;
        }
        frame->cap = new_cap;
    }

    frame->slots[frame->len++] = root_addr;
}

void gc_mode_precise_mark_roots(gc_mark_stack *st) {
    for (gc_thread *th = g_threads; th; th = th->next) {
        if (!th->attached) {
            continue;
        }
        for (gc_shadow_frame *fr = (gc_shadow_frame *)th->shadow_top; fr; fr = fr->prev) {
            for (size_t i = 0; i < fr->len; i++) {
                void *p = *fr->slots[i];
                gc_mark_ptr(st, p);
            }
        }
    }
}
