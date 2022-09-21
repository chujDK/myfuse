#include <gtest/gtest.h>
#include "test_def.h"

TestEnvironment* env;

std::array<bool, MAX_BLOCK_NO> wrote;

void* test_write_worker(void* _range) {
  auto range = (struct start_to_end*)_range;
  for (int i = range->start; i < range->end; i++) {
    int blockno = content_blockno[i];
    myfuse_log("writeing #%d", i);
    begin_op();
    auto b = logged_read(blockno);
    memcpy(b->data, contents[blockno], BSIZE);
    logged_write(b);
    EXPECT_EQ(0, memcmp(b->data, contents[blockno], BSIZE));
    EXPECT_EQ(blockno, b->blockno);
    logged_relse(b);
    end_op();
    EXPECT_EQ(wrote[blockno], false);
    wrote[blockno] = true;
  }
  return nullptr;
}

TEST(log_test, parallel_read_write_test) {
  generate_test_data();
  start_worker(test_write_worker, 2);

  int failed = 0;
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