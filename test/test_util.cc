#include "random"
#include <algorithm>
#include "test_def.h"

// map blockno to it's expected content
std::map<int, const u_char*> contents;
// this array contains [0, MAX_BLOCK_NO), and then shuffled
std::array<int, MAX_BLOCK_NO> total_blockno;
// this array contains {content_sum} uniq random nums in range [0, MAX_BLOCK_NO)
std::array<int, content_sum> content_blockno;

int nmeta_blocks = 0;

void generate_block_test_data() {
  // init the global contents
  for (int i = 0; i < nmeta_blocks; i++) {
    // write 0 in test is safe (fs do not use the boot section)
    total_blockno[i] = 0;
  }
  for (int i = nmeta_blocks; i < MAX_BLOCK_NO; i++) {
    total_blockno[i] = i;
  }

  static_assert(content_sum <= MAX_BLOCK_NO,
                "content_sum must lower then MAX_BLOCK_NO");

  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(total_blockno.begin(), total_blockno.end(), g);
  std::copy(total_blockno.begin(),
            total_blockno.begin() + content_blockno.size(),
            content_blockno.begin());

  for (int& i : content_blockno) {
    auto buf = new u_char[BSIZE];
    for (int i = 0; i < BSIZE; i++) {
      buf[i] = std::rand() % 0x100;
    }
    contents[i] = buf;
  }
}

void start_worker(void* (*pthread_worker)(void*), uint MAX_WORKER,
                  uint64_t end) {
  auto workers = new pthread_t[MAX_WORKER];
  auto ranges  = new struct start_to_end[MAX_WORKER];
  for (uint i = 0; i < MAX_WORKER; i++) {
    auto range   = &ranges[i];
    range->start = i * (end / MAX_WORKER);
    range->end   = (i + 1) * (end / MAX_WORKER);
    pthread_create(&workers[i], nullptr, pthread_worker, range);
  }
  for (uint i = 0; i < MAX_WORKER; i++) {
    pthread_join(workers[i], nullptr);
  }

  delete[] workers;
  delete[] ranges;
}

void init_super_block() {
  auto sb = &MYFUSE_STATE->sb;

  const uint disk_size     = MAX_BLOCK_NO;
  const uint nlog          = NLOG;
  const uint nbitmap       = disk_size / (BSIZE * 8) + 1;
  const uint ninode_blocks = ceil(disk_size / 100) + 1;
  const uint ninodes       = ninode_blocks * IPB;
  const uint nmeta_blocks  = 2 + nlog + ninode_blocks + nbitmap;
  const uint nblocks       = disk_size - nmeta_blocks;
  sb->magic                = FSMAGIC;
  sb->ninodes              = ninodes;
  sb->size                 = disk_size;
  sb->nblocks              = nblocks;
  sb->nlog                 = nlog;
  sb->logstart             = 2;
  sb->inodestart           = 2 + nlog;
  sb->bmapstart            = 2 + nlog + ninode_blocks;
}

struct myfuse_state* get_myfuse_state() {
  static struct myfuse_state state;
  return &state;
}