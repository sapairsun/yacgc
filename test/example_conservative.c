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
