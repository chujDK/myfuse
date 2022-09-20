#include <gtest/gtest.h>
#include "test_def.h"
#include <thread>
#include <random>
#include <algorithm>

TestEnvironment* env;

struct myfuse_state* get_myfuse_state() {
  static struct myfuse_state state;
  return &state;
}

// map blockno to it's expected content
std::map<int, const u_char*> contents;
const int content_sum = 1000;
// this array contains [0, MAX_BLOCK_NO), and then shuffled
std::array<int, MAX_BLOCK_NO> total_blockno;
// this array denotes {blockno}-th block has been wrote
std::array<bool, MAX_BLOCK_NO> wrote;
// this array contains {content_sum} uniq random nums in range [0, MAX_BLOCK_NO)
std::array<int, content_sum> content_blockno;

struct start_to_end {
  int start;
  int end;
};

void* test_write_worker(void* _range) {
  auto range = (struct start_to_end*)_range;
  for (int i = range->start; i < range->end; i++) {
    int blockno = content_blockno[i];
    auto b      = bread(blockno);
    memcpy(b->data, contents[blockno], BSIZE);
    auto n_write = bwrite(b);
    EXPECT_EQ(0, memcmp(b->data, contents[blockno], BSIZE));
    EXPECT_EQ(blockno, b->blockno);
    EXPECT_EQ(n_write, BSIZE);
    brelse(b);
    EXPECT_EQ(wrote[blockno], false);
    wrote[blockno] = true;
  }
  return nullptr;
}

// test:
// random read write block with mutiple threads
// this test is not `unit' at all...
TEST(bcache_buf, parallel_write_test) {
  // init the global contents
  for (int i = 0; i < MAX_BLOCK_NO; i++) {
    total_blockno[i] = i;
  }

  static_assert(content_sum < MAX_BLOCK_NO,
                "content_sum must lower then MAX_BLOCK_NO");

  int failed = 0;
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(total_blockno.begin(), total_blockno.end(), g);
  std::copy(total_blockno.begin(),
            total_blockno.begin() + content_blockno.size(),
            content_blockno.begin());
  wrote.fill(false);

  for (int& i : content_blockno) {
    auto buf = new u_char[BSIZE];
    for (int i = 0; i < BSIZE; i++) {
      buf[i] = std::rand() % 0x100;
    }
    contents[i] = buf;
  }

  // start {MAXOPBLOCKS} workers to write
  const int MAX_WORKER = MAXOPBLOCKS;
  std::array<pthread_t, MAX_WORKER> workers;
  for (int i = 0; i < MAX_WORKER; i++) {
    auto range   = new struct start_to_end();
    range->start = i * (content_sum / MAX_WORKER);
    range->end   = (i + 1) * (content_sum / MAX_WORKER);
    pthread_create(&workers[i], nullptr, test_write_worker, range);
  }
  for (pthread_t worker : workers) {
    pthread_join(worker, nullptr);
  }

  // check the write
  for (int& i : content_blockno) {
    ASSERT_TRUE(wrote[i]);
    auto b = bread(i);
    int eq = memcmp(b->data, contents[i], BSIZE);
    brelse(b);
    if (eq != 0) {
      failed++;
    }
  }

  myfuse_log("failed on %d blocks", failed);
  myfuse_log("%.3lf memory successfully read and write",
             (content_sum - failed) / (content_sum * 1.0));
  ASSERT_EQ(failed, 0);
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  env = reinterpret_cast<TestEnvironment*>(
      ::testing::AddGlobalTestEnvironment(new TestEnvironment()));
  if (env == nullptr) {
    err_exit("failed to init testing env!");
  }
  return RUN_ALL_TESTS();
}