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

#include "../include/gc.h"
#include "../include/gc_precise.h"

#include <assert.h>
#include <stdint.h>

typedef struct Pair {
    uintptr_t a;
    uintptr_t b;
} Pair;

static Pair *make_pairs(size_t n) {
    Pair *p = (Pair *)gc_malloc(n * sizeof(Pair));
    for (size_t i = 0; i < n; i++) {
        p[i].a = (uintptr_t)i;
        p[i].b = (uintptr_t)(i * 3);
    }
    return p;
}

int main(void) {
    GC_INIT();
    gc_set_mode(GC_MODE_PRECISE);
    gc_set_auto_collect(0);

    {
        gc_shadow_frame fr;
        gc_shadow_enter(&fr);

        Pair *p = make_pairs(10000);
        GC_SHADOW_ROOT(&fr, p);

        gc_collect();
        gc_stats s1 = gc_get_stats();
        assert(s1.allocated_objects == 1);

        gc_shadow_leave(&fr);
    }

    gc_collect();
    gc_stats s2 = gc_get_stats();
    assert(s2.allocated_objects == 0);

    gc_shutdown();
    return 0;
}
