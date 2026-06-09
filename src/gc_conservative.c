#include "gc_internal.h"

#include "../include/gc_conservative.h"

#include <setjmp.h>
#include <string.h>

int g_conservative_stack_scanning_enabled = 1;

void gc_conservative_set_stack_scanning(int enabled) {
    g_conservative_stack_scanning_enabled = enabled ? 1 : 0;
}

void gc_mode_conservative_mark_roots(gc_mark_stack *st) {
    if (!g_conservative_stack_scanning_enabled) {
        return;
    }
    for (gc_thread *th = g_threads; th; th = th->next) {
        if (!th->attached || !th->parked) {
            continue;
        }
        if (th->parked_regs_hi > th->parked_regs_lo) {
            gc_mark_range(st, th->parked_regs_lo, th->parked_regs_hi);
        }
        if (g_stack_grows_down) {
            if (th->parked_sp && th->stack_hi) {
                gc_mark_range(st, th->parked_sp, th->stack_hi);
            }
        } else {
            if (th->parked_sp && th->stack_lo) {
                gc_mark_range(st, th->stack_lo, th->parked_sp);
            }
        }
    }
}
