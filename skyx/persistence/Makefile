OPENSSL_ROOT ?= /opt/homebrew/opt/openssl
SQLITE3 ?= sqlite3
TIMING_REPORT ?= hash_timing_report.txt
BENCHMARK_REPORT ?= hash_benchmark_report.txt
BUILD_DIR := .test_db_build
BLAKE3_DIR := ../../ext/blake3/c
BLAKE3_OBJECTS := $(BUILD_DIR)/blake3.o $(BUILD_DIR)/blake3_dispatch.o $(BUILD_DIR)/blake3_portable.o
TARGET_TRIPLE := $(shell $(CC) -dumpmachine)
BLAKE3_BACKEND := portable
BLAKE3_EXPECT_SIMD := 0
BLAKE3_FLAGS := -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 -DBLAKE3_NO_AVX512 -DBLAKE3_USE_NEON=0

ifneq (,$(filter arm64% aarch64%,$(TARGET_TRIPLE)))
BLAKE3_OBJECTS += $(BUILD_DIR)/blake3_neon.o
BLAKE3_FLAGS := -DBLAKE3_USE_NEON=1
BLAKE3_BACKEND := NEON
BLAKE3_EXPECT_SIMD := 1
else ifneq (,$(filter x86_64% amd64%,$(TARGET_TRIPLE)))
BLAKE3_OBJECTS += $(BUILD_DIR)/blake3_sse2.o $(BUILD_DIR)/blake3_sse41.o $(BUILD_DIR)/blake3_avx2.o $(BUILD_DIR)/blake3_avx512.o
BLAKE3_FLAGS := -DBLAKE3_USE_NEON=0
BLAKE3_BACKEND := x86-runtime-dispatch
BLAKE3_EXPECT_SIMD := 1
endif

CPPFLAGS += -I../../include -I../../ext/sqlite3 -I$(BLAKE3_DIR) -I$(OPENSSL_ROOT)/include
CXXFLAGS += -std=c++26
OPT_FLAGS ?= -O3
SANITIZER_FLAGS ?= -fsanitize=address
TEST_FLAGS := $(OPT_FLAGS) $(SANITIZER_FLAGS) -g -fno-omit-frame-pointer
REPORT_FLAGS := -DSKYX_TEST_BUILD_FLAGS='"$(TEST_FLAGS)"' -DSKYX_TEST_BLAKE3_BACKEND='"$(BLAKE3_BACKEND)"' -DSKYX_TEST_EXPECT_SIMD=$(BLAKE3_EXPECT_SIMD)
LDFLAGS += -L$(OPENSSL_ROOT)/lib
LDLIBS += -lcrypto -pthread

.PHONY: compile init-db run-test-job clean run-tests benchmark

compile: $(BUILD_DIR)/test_db

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/sqlite3.o: ../../ext/sqlite3/sqlite3.c ../../ext/sqlite3/sqlite3.h Makefile | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_FLAGS) -c $< -o $@

$(BUILD_DIR)/blake3_sse2.o: CFLAGS += -msse2
$(BUILD_DIR)/blake3_sse41.o: CFLAGS += -msse4.1
$(BUILD_DIR)/blake3_avx2.o: CFLAGS += -mavx2
$(BUILD_DIR)/blake3_avx512.o: CFLAGS += -mavx512f -mavx512vl

$(BUILD_DIR)/%.o: $(BLAKE3_DIR)/%.c $(BLAKE3_DIR)/blake3.h $(BLAKE3_DIR)/blake3_impl.h Makefile | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_FLAGS) $(BLAKE3_FLAGS) -c $< -o $@

$(BUILD_DIR)/test_db: test_db.cpp db.cpp ../../include/persistence/db.h $(BLAKE3_DIR)/blake3.h $(BUILD_DIR)/sqlite3.o $(BLAKE3_OBJECTS) Makefile
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(TEST_FLAGS) $(REPORT_FLAGS) test_db.cpp db.cpp $(BUILD_DIR)/sqlite3.o $(BLAKE3_OBJECTS) -o $@ $(LDFLAGS) $(LDLIBS)

# The production connection opens index.db relative to its working directory.
init-db: | $(BUILD_DIR)
	rm -f $(BUILD_DIR)/index.db $(BUILD_DIR)/index.db-journal $(BUILD_DIR)/index.db-wal $(BUILD_DIR)/index.db-shm
	$(SQLITE3) $(BUILD_DIR)/index.db 'CREATE TABLE skyx_index (modified INTEGER NOT NULL, path TEXT NOT NULL, path_key TEXT PRIMARY KEY NOT NULL, hash TEXT NOT NULL);'

run-test-job: compile init-db
	cd $(BUILD_DIR) && ./test_db "$(abspath $(TIMING_REPORT))"

clean:
	rm -rf $(BUILD_DIR)

# Use a trap so failed builds/assertions also clean up, preserving the failure status.
run-tests:
	@set -eu; trap '$(MAKE) clean' EXIT; $(MAKE) run-test-job

# Separate artifacts prevent reusing sanitizer-instrumented objects in benchmarks.
benchmark:
	$(MAKE) run-tests BUILD_DIR=.benchmark_db_build SANITIZER_FLAGS= TIMING_REPORT="$(BENCHMARK_REPORT)"
