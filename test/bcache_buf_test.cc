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
std::map<int, std::array<u_char, BSIZE>*> contents;
const int content_sum = 1000;
// this array contains [0, MAX_BLOCK_NO), and then shuffled
std::array<int, MAX_BLOCK_NO> total_blockno;
// this array denotes {blockno}-th block has been wrote
std::array<bool, MAX_BLOCK_NO> wrote;
// this array contains {content_sum} uniq random nums in range [0, MAX_BLOCK_NO)
std::array<int, content_sum> content_blockno;

void test_write_worker(int start, int end) {
  myfuse_log("write work from %d to %d started", start, end);
  for (int i = start; i < end; i++) {
    int blockno = content_blockno[i];
    auto b      = bread(blockno);
    memcpy(b->data, contents[blockno]->data(), contents[blockno]->size());
    bwrite(b);
    brelse(b);
    wrote[blockno] = true;
    // myfuse_log("index: %d blockno: %d writed", i, blockno);
  }
}

// test:
// random read write block with mutiple threads
// this test is not `unit' at all...
TEST(bcache_buf, read_write_test) {
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
    auto buf = new std::array<u_char, BSIZE>;
    for (u_char& c : *buf) {
      c = std::rand() % 0x100;
    }
    contents[i] = buf;
  }
  myfuse_log("contents inited");

  // start {MAXOPBLOCKS} workers to write
  std::array<std::thread*, MAXOPBLOCKS> workers;
  for (int i = 0; i < MAXOPBLOCKS; i++) {
    workers[i] =
        new std::thread(test_write_worker, i * (content_sum / MAXOPBLOCKS),
                        (i + 1) * (content_sum / MAXOPBLOCKS));
  }
  for (auto thread : workers) {
    thread->join();
  }

  // check the write
  for (int& i : content_blockno) {
    ASSERT_TRUE(wrote[i]);
    auto b = bread(i);
    ASSERT_EQ(i, b->blockno);
    auto expected_data = contents[i]->data();
    int eq             = memcmp(b->data, expected_data, contents[i]->size());
    brelse(b);
    if (eq != 0) {
      myfuse_nonfatal("read odd data in block %d", i);
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