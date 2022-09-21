#pragma once
#include <gtest/gtest.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "block_device.h"
#include "buf_cache.h"
#include "util.h"
#include "param.h"
#ifdef __cplusplus
}
#include <unistd.h>
#include <cstdlib>
#endif

#define MAX_BLOCK_NO 400000

struct myfuse_state* get_myfuse_state();

class TestEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    block_device_init("./disk.img");
    bcache_init();
  }
};

// add the following two extern define to start of the testfile
//  // map blockno to it's expected content
// `extern std::map<int, const u_char*> contents;'
//  // this array contains {content_sum} uniq random nums in range [0,
//  // MAX_BLOCK_NO)
// `extern std::array<int, MAX_BLCOK_NO> content_blockno;'
void generate_test_data();

template <std::size_t MAX_WORKER = MAXOPBLOCKS>
void start_worker(void* (*pthread_worker)(void*));