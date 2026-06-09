#include "../include/gc.h"
#include "../include/gc_precise.h"

#include <stdint.h>

typedef struct Request {
    uintptr_t id;
    uintptr_t n;
} Request;

typedef struct Item {
    struct Item *next;
    uintptr_t v;
} Item;

static Item *build_items(uintptr_t n) {
    Item *head = NULL;
    for (uintptr_t i = 0; i < n; i++) {
        Item *it = (Item *)gc_malloc(sizeof(Item));
        it->next = head;
        it->v = i;
        head = it;
    }
    return head;
}

static void handle_request(const Request *req) {
    gc_shadow_frame fr;
    gc_shadow_enter(&fr);

    Item *items = build_items(req->n);
    GC_SHADOW_ROOT(&fr, items);

    if (items && items->next) {
        items->v ^= items->next->v;
    }

    gc_shadow_leave(&fr);
}

int main(void) {
    GC_INIT();
    gc_set_mode(GC_MODE_PRECISE);
    gc_set_auto_collect(0);

    for (uintptr_t i = 0; i < 100000; i++) {
        Request req;
        req.id = i;
        req.n = (i % 64) + 1;
        handle_request(&req);
        if ((i % 128) == 0) {
            gc_collect();
        }
    }

    gc_collect();
    gc_shutdown();
    return 0;
}
