# yacgc: Yet Another C Garbage Collector

[English](README.md) | [中文](README-CN.md)

A garbage collector library implemented from scratch in C (no external third-party dependencies). The goal is to offer a Go-like GC experience even without a VM: application code primarily allocates via `gc_malloc`, and the GC takes care of reclamation.

This project supports two ways to obtain the root set:
- Conservative: scan each thread’s register snapshot and stack memory, treating word values as potential pointers.
- Precise: use a per-thread shadow stack (precise roots) to avoid false retention, suitable for long-running services that require stable memory behavior.

It also provides multi-thread stop-the-world (STW) support: during GC, other threads are paused, their stacks/registers/precise roots are scanned, and then they are resumed.

## Layout

- include/
  - gc.h: public API (allocation/collection/stats/mode switching)
  - gc_conservative.h: conservative-mode specific API
  - gc_precise.h: precise-mode (shadow stack) API
- src/
  - gc_core.c: allocation + mark/sweep core (dispatches root scanning by mode)
  - gc_conservative.c: conservative root scanning
  - gc_precise.c: shadow stack implementation + precise root scanning
  - gc_stack.c: cross-platform stack bounds discovery
  - gc_threads.c: thread attachment, global lock, STW (POSIX/Windows)
- test/
  - example_conservative.c: conservative example (only `gc_malloc`)
  - example_precise.c: precise example (shadow stack)
  - example_service.c: long-running service style example (precise + periodic GC)
  - example_multithread.c: multi-thread concurrent allocation example

## Build & Run

- Build the static library, shared library, and all example executables:

```bash
make all
```

- Run examples (as test cases):

```bash
make test
```

Artifacts are placed under `build/`:
- Static library: `libyacgc.a`
- Shared library (platform-specific):
  - macOS: `libyacgc.dylib`
  - Linux: `libyacgc.so`
  - Windows: `libyacgc.dll`

## Usage

### 1) Default (Conservative) — application only uses gc_malloc

Conservative mode is enabled by default. On the first `gc_malloc` call, the current thread is automatically attached and initialization is performed lazily. Typical application code only needs to allocate via `gc_malloc`:

```c
#include "gc.h"

typedef struct Node { struct Node *next; int v; } Node;

static Node *build(int n) {
    Node *h = 0;
    for (int i = 0; i < n; i++) {
        Node *x = (Node *)gc_malloc(sizeof(Node));
        x->next = h;
        x->v = i;
        h = x;
    }
    return h;
}

int main(void) {
    for (int i = 0; i < 10000; i++) {
        Node *h = build((i % 64) + 1);
        if (h && h->next) h->v ^= h->next->v;
    }
    gc_shutdown();
    return 0;
}
```

Note: conservative scanning can cause false retention. In long-running processes, this may show up as slow growth that looks like a leak. If you need provable memory stability, use precise mode.

### 2) Precise mode (shadow stack) — avoid false retention

```c
#include "gc.h"
#include "gc_precise.h"

int main(void) {
    gc_set_mode(GC_MODE_PRECISE);
    gc_set_auto_collect(0);

    {
        gc_shadow_frame fr;
        gc_shadow_enter(&fr);

        void *p = gc_malloc(1024);
        GC_SHADOW_ROOT(&fr, p);

        gc_collect();
        gc_shadow_leave(&fr);
    }

    gc_collect();
    gc_shutdown();
    return 0;
}
```

The key idea is that the GC only scans roots you explicitly register (shadow stack), so stack garbage will not accidentally keep objects alive.

## Technical Design

### 1) Memory model: non-moving

This GC is non-moving:
- object addresses are stable (works naturally with C pointers)
- no compaction, simpler and robust

Trade-off: fragmentation is possible (depends on the underlying `malloc` strategy), and there is no locality improvement from moving objects.

### 2) Allocation: gc_malloc/gc_free

Each allocation creates a block with a small header:
- the header stores `size/marked/next`
- the pointer returned to the user points to the payload (right after the header)

All blocks are tracked in a singly linked list `g_blocks`. During sweep, unreachable blocks are unlinked and freed.

### 3) Mark & Sweep

A GC cycle:
1. clear all `marked` flags
2. scan roots and push reachable objects onto the mark stack
3. repeatedly pop objects and scan their payload memory, treating machine words as potential pointers and marking newly found objects
4. sweep: free unmarked objects

Object scanning is conservative: the payload is scanned word-by-word, interpreting each word as a candidate pointer.

### 4) Two root acquisition modes

#### A. Conservative mode

Root sources:
- each thread’s register snapshot (via `setjmp`, making registers scannable)
- each thread’s stack range (from SP to stack top, or stack bottom to SP depending on stack growth)

Cross-platform stack bounds:
- macOS: `pthread_get_stackaddr_np/pthread_get_stacksize_np`
- Linux: `pthread_getattr_np + pthread_attr_getstack`
- Windows: prefer `GetCurrentThreadStackLimits`, otherwise estimate via `VirtualQuery`

#### B. Precise mode

Root sources:
- shadow stack (a per-thread TLS-linked list of frames)

Application code registers roots that must survive a GC via `GC_SHADOW_ROOT(&fr, p)`. The GC scans shadow roots from all threads.

### 5) Multithreading and stop-the-world (STW)

This project uses STW to ensure safe reachability analysis:
- on entering `gc_collect()`, request other threads to pause
- scan other threads’ register snapshots and stacks (conservative) / shadow roots (precise)
- resume other threads after sweep

Implementation:
- POSIX: use `pthread_kill(SIGUSR1)` to trigger a signal handler that parks the thread and records SP/register snapshots; the GC thread waits until all target threads are parked.
- Windows: use `SuspendThread/GetThreadContext` to stop and snapshot thread context, then `ResumeThread`.

### 6) Portability boundaries and limitations

To provide a Go-like GC experience in pure C, some boundaries are inevitable:
- conservative mode can retain objects spuriously (probabilistic), good for “ease of use first”
- precise mode requires explicit root registration to guarantee stable memory behavior
- POSIX STW relies on signals; be cautious in latency-sensitive environments (though this approach is standard)
