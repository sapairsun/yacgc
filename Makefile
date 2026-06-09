CC ?= cc
AR ?= ar

CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
CFLAGS += -pthread
ifeq ($(OS),Windows_NT)
else
CFLAGS += -fPIC
endif
LDFLAGS += -pthread
CPPFLAGS += -Iinclude

BUILD_DIR := build
SRC_DIR := src
TEST_DIR := test

LIB := $(BUILD_DIR)/libyacgc.a
ifeq ($(OS),Windows_NT)
SHARED_LIB := $(BUILD_DIR)/libyacgc.dll
else ifeq ($(shell uname -s),Darwin)
SHARED_LIB := $(BUILD_DIR)/libyacgc.dylib
else
SHARED_LIB := $(BUILD_DIR)/libyacgc.so
endif
TEST_BINS := $(BUILD_DIR)/example_conservative $(BUILD_DIR)/example_precise $(BUILD_DIR)/example_service $(BUILD_DIR)/example_multithread
EXAMPLE_BIN := $(BUILD_DIR)/example_service

OBJS := $(BUILD_DIR)/gc_core.o $(BUILD_DIR)/gc_conservative.o $(BUILD_DIR)/gc_precise.o $(BUILD_DIR)/gc_stack.o $(BUILD_DIR)/gc_threads.o

.PHONY: all test example clean

all: $(LIB) $(SHARED_LIB) $(TEST_BINS)

test: $(TEST_BINS)
	$(BUILD_DIR)/example_conservative
	$(BUILD_DIR)/example_precise
	$(BUILD_DIR)/example_service
	$(BUILD_DIR)/example_multithread

example: $(EXAMPLE_BIN)

$(BUILD_DIR)/gc_core.o: $(SRC_DIR)/gc_core.c $(SRC_DIR)/gc_internal.h include/gc.h include/gc_precise.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(SRC_DIR)/gc_core.c -o $@

$(BUILD_DIR)/gc_conservative.o: $(SRC_DIR)/gc_conservative.c $(SRC_DIR)/gc_internal.h include/gc.h include/gc_conservative.h include/gc_precise.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(SRC_DIR)/gc_conservative.c -o $@

$(BUILD_DIR)/gc_precise.o: $(SRC_DIR)/gc_precise.c $(SRC_DIR)/gc_internal.h include/gc.h include/gc_precise.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(SRC_DIR)/gc_precise.c -o $@

$(BUILD_DIR)/gc_stack.o: $(SRC_DIR)/gc_stack.c $(SRC_DIR)/gc_internal.h include/gc.h include/gc_precise.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(SRC_DIR)/gc_stack.c -o $@

$(BUILD_DIR)/gc_threads.o: $(SRC_DIR)/gc_threads.c $(SRC_DIR)/gc_internal.h include/gc.h include/gc_precise.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(SRC_DIR)/gc_threads.c -o $@

$(LIB): $(OBJS) | $(BUILD_DIR)
	$(AR) rcs $@ $(OBJS)

$(SHARED_LIB): $(OBJS) | $(BUILD_DIR)
ifeq ($(OS),Windows_NT)
	$(CC) -shared $(LDFLAGS) $(OBJS) -o $@
else ifeq ($(shell uname -s),Darwin)
	$(CC) -dynamiclib $(LDFLAGS) $(OBJS) -o $@
else
	$(CC) -shared $(LDFLAGS) $(OBJS) -o $@
endif

$(BUILD_DIR)/example_conservative.o: $(TEST_DIR)/example_conservative.c include/gc.h include/gc_conservative.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(TEST_DIR)/example_conservative.c -o $@

$(BUILD_DIR)/example_conservative: $(BUILD_DIR)/example_conservative.o $(LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(BUILD_DIR)/example_conservative.o $(LIB) -o $@

$(BUILD_DIR)/example_precise.o: $(TEST_DIR)/example_precise.c include/gc.h include/gc_precise.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(TEST_DIR)/example_precise.c -o $@

$(BUILD_DIR)/example_precise: $(BUILD_DIR)/example_precise.o $(LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(BUILD_DIR)/example_precise.o $(LIB) -o $@

$(BUILD_DIR)/example_service.o: $(TEST_DIR)/example_service.c include/gc.h include/gc_precise.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(TEST_DIR)/example_service.c -o $@

$(EXAMPLE_BIN): $(BUILD_DIR)/example_service.o $(LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(BUILD_DIR)/example_service.o $(LIB) -o $@

$(BUILD_DIR)/example_multithread.o: $(TEST_DIR)/example_multithread.c include/gc.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $(TEST_DIR)/example_multithread.c -o $@

$(BUILD_DIR)/example_multithread: $(BUILD_DIR)/example_multithread.o $(LIB) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(BUILD_DIR)/example_multithread.o $(LIB) -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
