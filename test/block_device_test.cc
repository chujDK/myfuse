#include "test_def.h"
#include <array>
#include <random>

TestEnvironment* env;

struct myfuse_state* get_myfuse_state() {
  static struct myfuse_state state;
  return &state;
}

// test:
// 1. read block
// 2. write block
// 3. random read write

TEST(block_device, read_write_test) {
  std::array<char, BSIZE> write_buf;
  std::array<char, BSIZE> read_buf;

  for (int i = 0; i < 1000; i++) {
    for (char& c : write_buf) {
      c = std::rand() % 0x100;
    }
    int blockno = std::rand() % MAX_BLOCK_NO;
    int n_write = write_block_raw(blockno, write_buf.data());
    EXPECT_EQ(n_write, BSIZE);
    int n_read = read_block_raw(blockno, read_buf.data());
    EXPECT_EQ(n_write, n_read);
    EXPECT_EQ(write_buf, read_buf);
  }

  (void)MYFUSE_STATE->sb;
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
