#include "test_def.h"
#include <array>
#include <random>
#include <algorithm>
#include <random>

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
// this array contains {content_sum} uniq random nums in range [0, MAX_BLOCK_NO)
std::array<int, content_sum> content_blockno;

// test:
// 1. read block
// 2. write block
// 3. random read write
TEST(block_device, read_write_test) {
  std::array<u_char, BSIZE> write_buf;
  std::array<u_char, BSIZE> read_buf;

  for (int i = 0; i < 1000; i++) {
    for (auto& c : write_buf) {
      c = std::rand() % 0x100;
    }
    int blockno = std::rand() % MAX_BLOCK_NO;
    int n_write = write_block_raw(blockno, write_buf.data());
    EXPECT_EQ(n_write, BSIZE);
    int n_read = read_block_raw(blockno, read_buf.data());
    EXPECT_EQ(n_write, n_read);
    EXPECT_EQ(write_buf, read_buf);
  }
}

TEST(block_device, random_read_write_test) {
  // init the global contents
  for (int i = 0; i < MAX_BLOCK_NO; i++) {
    total_blockno[i] = i;
  }

  static_assert(content_sum < MAX_BLOCK_NO,
                "content_sum must lower then MAX_BLOCK_NO");

  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(total_blockno.begin(), total_blockno.end(), g);
  std::copy(total_blockno.begin(),
            total_blockno.begin() + content_blockno.size(),
            content_blockno.begin());

  for (int& i : content_blockno) {
    auto buf = new std::array<u_char, BSIZE>;
    for (u_char& c : *buf) {
      c = std::rand() % 0x100;
    }
    contents[i] = buf;
  }
  myfuse_log("contents inited");

  for (int& i : content_blockno) {
    int n_write = write_block_raw(i, contents[i]->data());
    EXPECT_EQ(n_write, BSIZE);
  }

  std::shuffle(content_blockno.begin(), content_blockno.end(), g);

  for (int& i : content_blockno) {
    std::array<u_char, BSIZE> buf;
    int n_read = read_block_raw(i, buf.data());
    EXPECT_EQ(n_read, BSIZE);
    EXPECT_EQ(buf, *contents[i]);
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
