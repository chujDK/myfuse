#include <gtest/gtest.h>
#include "test_def.h"
#include "pthread.h"
#include "random"
#include "algorithm"

TestEnvironment* env;

struct myfuse_state* get_myfuse_state() {
  static struct myfuse_state state;
  return &state;
}

std::map<int, std::array<char, BSIZE>*> contents;

bool test_worker() {}

// test:
// random read write block with mutiple threads
TEST(bcache_buf, read_write_test) {
  // init the global contents
  const int content_sum = 1000;
  std::array<int, MAX_BLOCK_NO> total_blockno;
  std::array<int, content_sum> content_blockno;
  for (int i = 0; i < MAX_BLOCK_NO; i++) {
    total_blockno[i] = i;
  }

  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(total_blockno.begin(), total_blockno.end(), g);
  std::copy(content_blockno.begin(), content_blockno.end(),
            total_blockno.begin());

  for (int i = 0; i < content_blockno.size(); i++) {
    auto buf = new std::array<char, BSIZE>;
    for (char& c : *buf) {
      c = std::rand() % 0x100;
    }
    contents[content_blockno[i]] = buf;
  }
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