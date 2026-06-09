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

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if defined(__APPLE__) || defined(__linux__) || defined(__unix__)
#include <pthread.h>
#endif

int gc_platform_get_stack_bounds(uintptr_t *lo, uintptr_t *hi) {
    if (!lo || !hi) {
        return 0;
    }

#if defined(_WIN32)
    {
        HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
        if (kernel32) {
            typedef VOID(WINAPI *Fn)(PULONG_PTR, PULONG_PTR);
            Fn p = (Fn)(void *)GetProcAddress(kernel32, "GetCurrentThreadStackLimits");
            if (p) {
                ULONG_PTR low = 0;
                ULONG_PTR high = 0;
                p(&low, &high);
                if (high > low) {
                    *lo = (uintptr_t)low;
                    *hi = (uintptr_t)high;
                    return 1;
                }
            }
        }

        uintptr_t marker = 0;
        MEMORY_BASIC_INFORMATION mbi;
        SIZE_T n = VirtualQuery((void *)&marker, &mbi, sizeof(mbi));
        if (n == 0 || !mbi.AllocationBase) {
            return 0;
        }

        uintptr_t base = (uintptr_t)mbi.AllocationBase;
        uintptr_t end = base;
        uintptr_t p = base;
        for (;;) {
            n = VirtualQuery((void *)p, &mbi, sizeof(mbi));
            if (n == 0) {
                break;
            }
            if ((uintptr_t)mbi.AllocationBase != base) {
                break;
            }
            uintptr_t region_end = (uintptr_t)mbi.BaseAddress + (uintptr_t)mbi.RegionSize;
            if (region_end > end) {
                end = region_end;
            }
            if (region_end <= p) {
                break;
            }
            p = region_end;
        }

        if (end > base) {
            *lo = base;
            *hi = end;
            return 1;
        }
        return 0;
    }
#elif defined(__APPLE__)
    {
        pthread_t self = pthread_self();
        void *stackaddr = pthread_get_stackaddr_np(self);
        size_t stacksize = pthread_get_stacksize_np(self);
        if (!stackaddr || stacksize == 0) {
            return 0;
        }
        *hi = (uintptr_t)stackaddr;
        *lo = *hi - (uintptr_t)stacksize;
        return *hi > *lo;
    }
#elif defined(__linux__) || defined(__unix__)
    {
        pthread_attr_t attr;
        if (pthread_getattr_np(pthread_self(), &attr) != 0) {
            return 0;
        }
        void *stackaddr = NULL;
        size_t stacksize = 0;
        int rc = pthread_attr_getstack(&attr, &stackaddr, &stacksize);
        pthread_attr_destroy(&attr);
        if (rc != 0 || !stackaddr || stacksize == 0) {
            return 0;
        }
        *lo = (uintptr_t)stackaddr;
        *hi = *lo + (uintptr_t)stacksize;
        return *hi > *lo;
    }
#else
    (void)lo;
    (void)hi;
    return 0;
#endif
}
