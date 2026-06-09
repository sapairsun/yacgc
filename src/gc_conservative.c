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
