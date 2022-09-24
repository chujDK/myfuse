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
#include "inode.h"
#include "block_allocator.h"
#ifdef __cplusplus
}
#include <unistd.h>
#include <cstdlib>
#include <array>
#endif

#define MAX_SECTOR_BLOCK_NO 400000
const int sector_size      = 512;
const int sector_per_block = BSIZE / sector_size;
static_assert(MAX_SECTOR_BLOCK_NO % sector_per_block == 0,
              "test disk unaligned!");
#define MAX_BLOCK_NO MAX_SECTOR_BLOCK_NO / sector_per_block
extern int nmeta_blocks;

struct myfuse_state* get_myfuse_state();

void init_super_block();

class TestEnvironment : public ::testing::Environment {
 public:
  void SetUp() override {
    srand(time(nullptr));

#ifndef DISK_IMG_PATH
#define DISK_IMG_PATH "./disk.img"
#endif
    block_device_init(DISK_IMG_PATH);

    init_super_block();

    nmeta_blocks = MYFUSE_STATE->sb.size - MYFUSE_STATE->sb.nblocks;
    std::array<u_char, BSIZE> zeros;
    zeros.fill(0);
    for (int i = 0; i < nmeta_blocks; i++) {
      write_block_raw(i, zeros.data());
    }
    memmove(zeros.data(), &MYFUSE_STATE->sb, sizeof(MYFUSE_STATE->sb));
    write_block_raw(1, zeros.data());

    bcache_init();
    log_init(&MYFUSE_STATE->sb);

    inode_init();
    init_meta_blocks_bmap();
    free(bmap_cache.cache_buf);
    free(bmap_cache.first_unalloced);
    free(bmap_cache.n_alloced);
    block_allocator_init();
  }
};

const int content_sum = 1000;
const int MAX_WORKER  = 10;

// map blockno to it's expected content
extern std::map<int, const u_char*> contents;
// this array contains {content_sum} uniq random nums in range [0,
// MAX_BLOCK_NO)
extern std::array<int, content_sum> content_blockno;
void generate_block_test_data();

void start_worker(void* (*pthread_worker)(void*), uint MAXWORKER = MAX_WORKER,
                  uint64_t end = content_sum);

// indicate the range the worker need to workon
// don't free it by the callee
struct start_to_end {
  uint start;
  uint end;
};