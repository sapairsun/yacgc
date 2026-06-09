#include "../include/gc.h"

#include <stdint.h>

typedef struct Node {
    struct Node *next;
    uintptr_t value;
} Node;

static Node *build_list(size_t n) {
    Node *head = NULL;
    for (size_t i = 0; i < n; i++) {
        Node *node = (Node *)gc_malloc(sizeof(Node));
        node->next = head;
        node->value = (uintptr_t)i;
        head = node;
    }
    return head;
}

int main(void) {
    for (uintptr_t i = 0; i < 2000; i++) {
        Node *head = build_list((i % 64) + 1);
        if (head && head->next) {
            head->value ^= head->next->value;
        }
    }
    return 0;
}
