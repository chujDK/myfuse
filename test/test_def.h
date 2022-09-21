#pragma once
#include <gtest/gtest.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "block_device.h"
#include "buf_cache.h"
#include "util.h"
#include "log.h"
#include "param.h"
#ifdef __cplusplus
}
#include <unistd.h>
#include <cstdlib>
#endif

#define MAX_BLOCK_NO 400000

struct myfuse_state* get_myfuse_state();

void init_super_block();

class TestEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    block_device_init("./disk.img");

    init_super_block();

    bcache_init();
    log_init(&MYFUSE_STATE->sb);
  }
};

const int content_sum = 1000;
const int MAX_WORKER  = MAXOPBLOCKS;

// map blockno to it's expected content
extern std::map<int, const u_char*> contents;
// this array contains {content_sum} uniq random nums in range [0,
// MAX_BLOCK_NO)
extern std::array<int, content_sum> content_blockno;
void generate_test_data();

void start_worker(void* (*pthread_worker)(void*), int MAXWORKER = MAX_WORKER);

// indicate the range the worker need to workon
// don't free it by the callee
struct start_to_end {
  int start;
  int end;
};