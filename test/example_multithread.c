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

#include <stdint.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

typedef struct Payload {
    struct Payload *next;
    uintptr_t v;
} Payload;

static Payload *build_chain(uintptr_t n) {
    Payload *head = NULL;
    for (uintptr_t i = 0; i < n; i++) {
        Payload *p = (Payload *)gc_malloc(sizeof(Payload));
        p->next = head;
        p->v = i;
        head = p;
    }
    return head;
}

static uintptr_t worker_body(uintptr_t id) {
    uintptr_t acc = id;
    for (uintptr_t i = 0; i < 2000; i++) {
        Payload *head = build_chain((i % 16) + 1);
        if (head && head->next) {
            acc ^= head->v + head->next->v;
        }
    }
    return acc;
}

#if defined(_WIN32)
static DWORD WINAPI worker_thread(LPVOID p) {
    uintptr_t id = (uintptr_t)p;
    (void)worker_body(id);
    return 0;
}
#else
static void *worker_thread(void *p) {
    uintptr_t id = (uintptr_t)p;
    (void)worker_body(id);
    return NULL;
}
#endif

int main(void) {
    const uintptr_t kThreads = 4;
    (void)gc_malloc(1);
    gc_set_auto_collect(0);

#if defined(_WIN32)
    HANDLE threads[kThreads];
    for (uintptr_t i = 0; i < kThreads; i++) {
        threads[i] = CreateThread(NULL, 0, worker_thread, (LPVOID)i, 0, NULL);
        if (!threads[i]) {
            return 1;
        }
    }
    WaitForMultipleObjects((DWORD)kThreads, threads, TRUE, INFINITE);
    for (uintptr_t i = 0; i < kThreads; i++) {
        CloseHandle(threads[i]);
    }
#else
    pthread_t threads[kThreads];
    for (uintptr_t i = 0; i < kThreads; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread, (void *)i) != 0) {
            return 1;
        }
    }
    for (uintptr_t i = 0; i < kThreads; i++) {
        pthread_join(threads[i], NULL);
    }
#endif

    gc_collect();
    gc_shutdown();
    return 0;
}
