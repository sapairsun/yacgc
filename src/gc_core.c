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

gc_block *g_blocks = NULL;
uintptr_t g_stack_lo = 0;
uintptr_t g_stack_hi = 0;
int g_stack_bounds_ready = 0;
int g_stack_grows_down = 1;
static int g_initialized = 0;

void ***g_roots = NULL;
size_t g_roots_len = 0;
size_t g_roots_cap = 0;

size_t g_allocated_bytes = 0;
size_t g_allocated_objects = 0;
size_t g_last_collected_bytes = 0;
size_t g_last_collected_objects = 0;

int g_auto_collect_enabled = 1;
size_t g_collect_threshold = 1024 * 1024;
int g_collect_threshold_user_set = 0;
int g_in_collect = 0;

gc_mode g_mode = GC_MODE_CONSERVATIVE;

static int gc_stack_dir_helper(uintptr_t first) {
    uintptr_t second = 0;
    return (uintptr_t)&second < first;
}

static void gc_ensure_init(void) {
    if (g_initialized) {
        return;
    }
    gc_threads_init_once();
    g_stack_grows_down = gc_stack_dir_helper((uintptr_t)&g_stack_grows_down) ? 1 : 0;
    gc_init(NULL);
    gc_threads_attach_current();
}

void gc_set_mode(gc_mode mode) {
    gc_lock();
    g_mode = mode;
    gc_unlock();
}

gc_mode gc_get_mode(void) {
    return g_mode;
}

void gc_mark_stack_push(gc_mark_stack *st, gc_block *b) {
    if (st->len == st->cap) {
        size_t new_cap = st->cap ? (st->cap * 2) : 256;
        gc_block **new_items = (gc_block **)realloc(st->items, new_cap * sizeof(*new_items));
        if (!new_items) {
            abort();
        }
        st->items = new_items;
        st->cap = new_cap;
    }
    st->items[st->len++] = b;
}

gc_block *gc_find_block_containing(void *p) {
    if (!p) {
        return NULL;
    }
    uintptr_t addr = (uintptr_t)p;
    for (gc_block *b = g_blocks; b; b = b->next) {
        uintptr_t start = (uintptr_t)(b + 1);
        uintptr_t end = start + b->size;
        if (addr >= start && addr < end) {
            return b;
        }
    }
    return NULL;
}

void gc_mark_ptr(gc_mark_stack *st, void *p) {
    gc_block *b = gc_find_block_containing(p);
    if (!b || b->marked) {
        return;
    }
    b->marked = 1;
    gc_mark_stack_push(st, b);
}

void gc_mark_range(gc_mark_stack *st, uintptr_t lo, uintptr_t hi) {
    uintptr_t word = sizeof(uintptr_t);
    uintptr_t start = lo < hi ? lo : hi;
    uintptr_t end = lo < hi ? hi : lo;

    start &= ~(word - 1);
    end &= ~(word - 1);

    for (uintptr_t cur = start; cur <= end; cur += word) {
        uintptr_t v = *(uintptr_t *)cur;
        gc_mark_ptr(st, (void *)v);
    }
}

void gc_mark_object(gc_mark_stack *st, gc_block *b) {
    uintptr_t start = (uintptr_t)(b + 1);
    uintptr_t end = start + b->size;
    if (end <= start) {
        return;
    }
    gc_mark_range(st, start, end - 1);
}

void gc_sweep(void) {
    gc_block *prev = NULL;
    gc_block *cur = g_blocks;
    g_last_collected_bytes = 0;
    g_last_collected_objects = 0;

    while (cur) {
        if (cur->marked) {
            prev = cur;
            cur = cur->next;
            continue;
        }

        gc_block *dead = cur;
        cur = cur->next;
        if (prev) {
            prev->next = cur;
        } else {
            g_blocks = cur;
        }

        g_last_collected_bytes += dead->size;
        g_last_collected_objects += 1;
        g_allocated_bytes -= dead->size;
        g_allocated_objects -= 1;
        free(dead);
    }
}

void gc_init(void *stack_base) {
    g_initialized = 1;
    g_stack_bounds_ready = 0;
    gc_threads_init_once();

    if (stack_base) {
        g_stack_hi = (uintptr_t)stack_base;
        g_stack_lo = 0;
        g_stack_bounds_ready = 1;
    } else {
        uintptr_t lo = 0;
        uintptr_t hi = 0;
        if (gc_platform_get_stack_bounds(&lo, &hi)) {
            g_stack_lo = lo;
            g_stack_hi = hi;
            g_stack_bounds_ready = 1;
        }
    }
    if (!g_collect_threshold_user_set) {
        g_collect_threshold = 1024 * 1024;
    }

    gc_threads_attach_current();
}

void gc_shutdown(void) {
    gc_lock();
    g_in_collect = 1;
    g_initialized = 0;
    g_stack_lo = 0;
    g_stack_hi = 0;
    g_stack_bounds_ready = 0;

    gc_block *b = g_blocks;
    while (b) {
        gc_block *next = b->next;
        free(b);
        b = next;
    }
    g_blocks = NULL;

    free(g_roots);
    g_roots = NULL;
    g_roots_len = 0;
    g_roots_cap = 0;

    g_allocated_bytes = 0;
    g_allocated_objects = 0;
    g_last_collected_bytes = 0;
    g_last_collected_objects = 0;

    g_in_collect = 0;
    gc_unlock();
}

void gc_set_auto_collect(int enabled) {
    gc_lock();
    g_auto_collect_enabled = enabled ? 1 : 0;
    gc_unlock();
}

void gc_set_collect_threshold(size_t bytes) {
    gc_lock();
    g_collect_threshold = bytes ? bytes : 1;
    g_collect_threshold_user_set = 1;
    gc_unlock();
}

void *gc_malloc(size_t size) {
    gc_ensure_init();
    int do_collect = 0;
    gc_lock();
    if (size == 0) {
        size = 1;
    }

    gc_block *b = (gc_block *)malloc(sizeof(*b) + size);
    if (!b) {
        abort();
    }

    b->next = g_blocks;
    b->size = size;
    b->marked = 0;
    g_blocks = b;

    g_allocated_bytes += size;
    g_allocated_objects += 1;

    if (g_auto_collect_enabled && !g_in_collect) {
        if (g_allocated_bytes >= g_collect_threshold) {
            do_collect = 1;
        }
    }
    gc_unlock();
    if (do_collect) {
        gc_collect();
    }
    return (void *)(b + 1);
}

void *gc_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) {
        return gc_malloc(1);
    }
    if (SIZE_MAX / count < size) {
        abort();
    }
    size_t total = count * size;
    void *p = gc_malloc(total);
    memset(p, 0, total);
    return p;
}

void *gc_realloc(void *ptr, size_t new_size) {
    if (!ptr) {
        return gc_malloc(new_size);
    }
    if (new_size == 0) {
        gc_free(ptr);
        return NULL;
    }

    gc_block *b = (gc_block *)ptr - 1;
    size_t old_size = b->size;

    void *new_ptr = gc_malloc(new_size);
    size_t to_copy = old_size < new_size ? old_size : new_size;
    memcpy(new_ptr, ptr, to_copy);
    gc_free(ptr);
    return new_ptr;
}

void gc_free(void *ptr) {
    if (!ptr) {
        return;
    }

    gc_lock();
    gc_block *target = (gc_block *)ptr - 1;
    gc_block *prev = NULL;
    for (gc_block *b = g_blocks; b; b = b->next) {
        if (b == target) {
            if (prev) {
                prev->next = b->next;
            } else {
                g_blocks = b->next;
            }
            g_allocated_bytes -= b->size;
            g_allocated_objects -= 1;
            free(b);
            gc_unlock();
            return;
        }
        prev = b;
    }
    gc_unlock();
}

void gc_add_root(void **root_addr) {
    if (!root_addr) {
        return;
    }
    gc_lock();
    for (size_t i = 0; i < g_roots_len; i++) {
        if (g_roots[i] == root_addr) {
            gc_unlock();
            return;
        }
    }

    if (g_roots_len == g_roots_cap) {
        size_t new_cap = g_roots_cap ? (g_roots_cap * 2) : 64;
        void ***new_items = (void ***)realloc(g_roots, new_cap * sizeof(*new_items));
        if (!new_items) {
            abort();
        }
        g_roots = new_items;
        g_roots_cap = new_cap;
    }
    g_roots[g_roots_len++] = root_addr;
    gc_unlock();
}

void gc_remove_root(void **root_addr) {
    if (!root_addr) {
        return;
    }
    gc_lock();
    for (size_t i = 0; i < g_roots_len; i++) {
        if (g_roots[i] == root_addr) {
            g_roots[i] = g_roots[g_roots_len - 1];
            g_roots_len -= 1;
            gc_unlock();
            return;
        }
    }
    gc_unlock();
}

void gc_collect(void) {
    gc_ensure_init();
    gc_lock();
    g_in_collect = 1;

    if (!gc_threads_stop_the_world()) {
        g_in_collect = 0;
        gc_threads_start_the_world();
        gc_unlock();
        return;
    }

    for (gc_block *b = g_blocks; b; b = b->next) {
        b->marked = 0;
    }

    gc_mark_stack st;
    st.items = NULL;
    st.len = 0;
    st.cap = 0;

    for (size_t i = 0; i < g_roots_len; i++) {
        void *p = *g_roots[i];
        gc_mark_ptr(&st, p);
    }

    if (g_mode == GC_MODE_PRECISE) {
        gc_mode_precise_mark_roots(&st);
    } else {
        gc_mode_conservative_mark_roots(&st);
    }

    while (st.len) {
        gc_block *b = st.items[--st.len];
        gc_mark_object(&st, b);
    }

    free(st.items);
    gc_sweep();

    if (g_auto_collect_enabled && !g_collect_threshold_user_set) {
        size_t next_threshold = g_allocated_bytes ? (g_allocated_bytes * 2) : (1024 * 1024);
        if (next_threshold < 1024 * 1024) {
            next_threshold = 1024 * 1024;
        }
        g_collect_threshold = next_threshold;
    }

    g_in_collect = 0;
    gc_threads_start_the_world();
    gc_unlock();
}

gc_stats gc_get_stats(void) {
    gc_stats s;
    s.allocated_bytes = g_allocated_bytes;
    s.allocated_objects = g_allocated_objects;
    s.last_collected_bytes = g_last_collected_bytes;
    s.last_collected_objects = g_last_collected_objects;
    return s;
}
