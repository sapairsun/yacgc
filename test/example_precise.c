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

