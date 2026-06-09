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

#include <setjmp.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#endif

int g_lock_initialized = 0;
#if !defined(__STDC_NO_ATOMICS__)
atomic_flag g_lock = ATOMIC_FLAG_INIT;
#endif

static int g_threads_inited = 0;
gc_thread *g_threads = NULL;
size_t g_threads_count = 0;

static _Thread_local gc_thread *t_thread = NULL;

#if !defined(__STDC_NO_ATOMICS__)
static _Atomic int g_world_stopping = 0;
static _Atomic int g_world_parked = 0;
#else
static volatile int g_world_stopping = 0;
static volatile int g_world_parked = 0;
#endif

static void gc_cpu_relax(void) {
#if defined(_WIN32)
    YieldProcessor();
#elif defined(__i386__) || defined(__x86_64__)
    __asm__ __volatile__("pause");
#else
    ;
#endif
}

void gc_lock(void) {
    if (!g_lock_initialized) {
        g_lock_initialized = 1;
    }
#if !defined(__STDC_NO_ATOMICS__)
    while (atomic_flag_test_and_set_explicit(&g_lock, memory_order_acquire)) {
        gc_cpu_relax();
    }
#endif
}

void gc_unlock(void) {
#if !defined(__STDC_NO_ATOMICS__)
    atomic_flag_clear_explicit(&g_lock, memory_order_release);
#endif
}

gc_thread *gc_threads_current(void) {
    return t_thread;
}

#if defined(_WIN32)
static unsigned long gc_os_thread_id(void) {
    return (unsigned long)GetCurrentThreadId();
}
#endif

#if !defined(_WIN32)
static void gc_sig_handler(int sig) {
    (void)sig;
    gc_thread *th = t_thread;
    if (!th) {
        while (
#if !defined(__STDC_NO_ATOMICS__)
            atomic_load_explicit(&g_world_stopping, memory_order_acquire)
#else
            g_world_stopping
#endif
        ) {
            gc_cpu_relax();
        }
        return;
    }

    uintptr_t sp_marker = 0;
    jmp_buf env;
    memset(&env, 0, sizeof(env));
    setjmp(env);

    th->parked_sp = (uintptr_t)&sp_marker;
    th->parked_regs_lo = (uintptr_t)&env;
    th->parked_regs_hi = th->parked_regs_lo + (uintptr_t)sizeof(env) - 1;
    th->parked = 1;

#if !defined(__STDC_NO_ATOMICS__)
    atomic_fetch_add_explicit(&g_world_parked, 1, memory_order_acq_rel);
    while (atomic_load_explicit(&g_world_stopping, memory_order_acquire)) {
        gc_cpu_relax();
    }
#else
    g_world_parked += 1;
    while (g_world_stopping) {
        gc_cpu_relax();
    }
#endif
    th->parked = 0;
}
#endif

void gc_threads_init_once(void) {
    if (g_threads_inited) {
        return;
    }
    g_threads_inited = 1;

#if !defined(_WIN32)
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = gc_sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);
#endif
}

void gc_threads_attach_current(void) {
    if (t_thread && t_thread->attached) {
        return;
    }
    gc_threads_init_once();

    gc_thread *th = (gc_thread *)calloc(1, sizeof(*th));
    if (!th) {
        abort();
    }

#if defined(_WIN32)
    th->os_thread_id = gc_os_thread_id();
    th->os_thread = (void *)GetCurrentThread();
#else
    th->os_thread = pthread_self();
#endif

    uintptr_t lo = 0;
    uintptr_t hi = 0;
    if (gc_platform_get_stack_bounds(&lo, &hi)) {
        th->stack_lo = lo;
        th->stack_hi = hi;
    }
    th->attached = 1;

    gc_lock();
    th->next = g_threads;
    g_threads = th;
    g_threads_count += 1;
    gc_unlock();

    t_thread = th;
}

void gc_threads_set_shadow_top(uintptr_t shadow_top) {
    gc_thread *th = t_thread;
    if (!th) {
        return;
    }
    th->shadow_top = shadow_top;
}

static void gc_capture_current_thread_parked_state(void) {
    gc_thread *th = t_thread;
    if (!th) {
        return;
    }
    uintptr_t sp_marker = 0;
    jmp_buf env;
    memset(&env, 0, sizeof(env));
    setjmp(env);
    th->parked_sp = (uintptr_t)&sp_marker;
    th->parked_regs_lo = (uintptr_t)&env;
    th->parked_regs_hi = th->parked_regs_lo + (uintptr_t)sizeof(env) - 1;
    th->parked = 1;
}

int gc_threads_stop_the_world(void) {
    gc_threads_attach_current();

#if !defined(__STDC_NO_ATOMICS__)
    atomic_store_explicit(&g_world_stopping, 1, memory_order_release);
    atomic_store_explicit(&g_world_parked, 0, memory_order_release);
#else
    g_world_stopping = 1;
    g_world_parked = 0;
#endif

    gc_capture_current_thread_parked_state();

    size_t expected = 0;

#if defined(_WIN32)
    for (gc_thread *th = g_threads; th; th = th->next) {
        if (th == t_thread || !th->attached) {
            continue;
        }
        HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, (DWORD)th->os_thread_id);
        if (!h) {
            continue;
        }
        SuspendThread(h);
        CONTEXT ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        if (GetThreadContext(h, &ctx)) {
#if defined(_M_X64) || defined(__x86_64__)
            th->parked_sp = (uintptr_t)ctx.Rsp;
#elif defined(_M_IX86) || defined(__i386__)
            th->parked_sp = (uintptr_t)ctx.Esp;
#else
            th->parked_sp = 0;
#endif
            th->parked_regs_lo = (uintptr_t)&ctx;
            th->parked_regs_hi = th->parked_regs_lo + (uintptr_t)sizeof(ctx) - 1;
        }
        th->parked = 1;
#if !defined(__STDC_NO_ATOMICS__)
        atomic_fetch_add_explicit(&g_world_parked, 1, memory_order_acq_rel);
#else
        g_world_parked += 1;
#endif
        expected += 1;
        CloseHandle(h);
    }
#else
    for (gc_thread *th = g_threads; th; th = th->next) {
        if (th == t_thread || !th->attached) {
            continue;
        }
        int rc = pthread_kill(th->os_thread, SIGUSR1);
        if (rc == 0) {
            expected += 1;
        } else if (rc == ESRCH) {
            th->attached = 0;
        }
    }
#endif

    size_t spins = 0;
    for (;;) {
#if !defined(__STDC_NO_ATOMICS__)
        int parked = atomic_load_explicit(&g_world_parked, memory_order_acquire);
#else
        int parked = g_world_parked;
#endif
        if ((size_t)parked >= expected) {
            break;
        }
        spins += 1;
        if (spins > 5000000) {
            return 0;
        }
        gc_cpu_relax();
    }

    return 1;
}

void gc_threads_start_the_world(void) {
#if defined(_WIN32)
    for (gc_thread *th = g_threads; th; th = th->next) {
        if (th == t_thread || !th->attached || !th->parked) {
            continue;
        }
        HANDLE h = OpenThread(THREAD_SUSPEND_RESUME, FALSE, (DWORD)th->os_thread_id);
        if (!h) {
            continue;
        }
        ResumeThread(h);
        CloseHandle(h);
        th->parked = 0;
    }
#endif

#if !defined(__STDC_NO_ATOMICS__)
    atomic_store_explicit(&g_world_stopping, 0, memory_order_release);
#else
    g_world_stopping = 0;
#endif

    if (t_thread) {
        t_thread->parked = 0;
    }
}
