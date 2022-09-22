#include <gtest/gtest.h>
#include "test_def.h"
#include <cassert>

TestEnvironment* env;

struct inode* single_inode;

const int nwriter             = 10;
const int nblock_reader_check = 1000;
const uint big_file_block =
    nwriter * ((uint)((NINDIRECT2 + 1000) / nwriter) + 1);
const uint big_file_size = big_file_block * BSIZE;
std::array<char, big_file_size> big_file_content;

static_assert(big_file_block < NDIRECT + NINDIRECT);

void* block_aligned_write_worker(void* _range) {
  auto range = (start_to_end*)_range;
  for (int i = range->start; i < range->end; i++) {
    begin_op();
    inode_write_nbytes(single_inode, &big_file_content[i * BSIZE], BSIZE,
                       i * BSIZE);
    end_op();
  }
  return nullptr;
}

void* block_aligned_read_worker(void*) {
  std::array<char, BSIZE> read_buf;
  for (int i = 0; i < nblock_reader_check; i++) {
    int blockno = std::rand() % big_file_block;
    begin_op();
    inode_read_nbytes(single_inode, read_buf.data(), BSIZE, blockno * BSIZE);
    end_op();
    int eq = memcmp(read_buf.data(), &big_file_content[blockno * BSIZE], BSIZE);
    EXPECT_EQ(eq, 0);
  }
  return nullptr;
}

TEST(inode, unaligned_random_read_write_test) {
  begin_op();
  single_inode = ialloc(T_FILE);
  end_op();

  for (char& c : big_file_content) {
    c = std::rand();
  }

  for (uint i = 0; i < big_file_size;) {
    uint size = rand() % (MAXOPBLOCKS * BSIZE);
    size      = std::min(big_file_size - i, size);

    begin_op();
    inode_write_nbytes(single_inode, &big_file_content[i], size, i);
    end_op();
    i += size;
  }

  std::array<char, MAXOPBLOCKS * BSIZE> read_buf;

  for (uint i = 0; i < big_file_size;) {
    uint size = rand() % (MAXOPBLOCKS * BSIZE);
    size      = std::min(big_file_size - i, size);

    begin_op();
    inode_read_nbytes(single_inode, read_buf.data(), size, i);
    end_op();

    int eq = memcmp(read_buf.data(), &big_file_content[i], size);
    EXPECT_EQ(eq, 0);

    i += size;
  }

  begin_op();
  iput(single_inode);
  end_op();
}

TEST(inode, block_aligned_read_write_test) {
  begin_op();
  single_inode = ialloc(T_FILE);
  end_op();

  for (char& c : big_file_content) {
    c = rand() % 0x100;
  }

  start_worker(block_aligned_write_worker, nwriter, big_file_block);

  std::array<char, BSIZE> read_buf;

  int failed = 0;
  // check the write
  for (uint i = 0; i < big_file_block; i++) {
    begin_op();
    inode_read_nbytes(single_inode, read_buf.data(), BSIZE, i * BSIZE);
    end_op();
    int eq = memcmp(read_buf.data(), &big_file_content[i * BSIZE], BSIZE);
    if (eq != 0) {
      failed++;
    }
  }

  myfuse_log("failed on %d blocks", failed);
  myfuse_log("%.3lf memory successfully read and write",
             (big_file_block - failed) / (big_file_block * 1.0));
  ASSERT_EQ(failed, 0);

  start_worker(block_aligned_read_worker, nwriter, big_file_block);

  begin_op();
  iput(single_inode);
  end_op();
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