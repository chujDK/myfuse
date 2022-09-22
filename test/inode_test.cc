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
  for (uint i = range->start; i < range->end; i++) {
    inode_write_nbytes(single_inode, &big_file_content[i * BSIZE], BSIZE,
                       i * BSIZE);
  }
  return nullptr;
}

void* block_aligned_read_worker(void*) {
  std::array<char, BSIZE> read_buf;
  for (int i = 0; i < nblock_reader_check; i++) {
    int blockno = std::rand() % big_file_block;
    inode_read_nbytes(single_inode, read_buf.data(), BSIZE, blockno * BSIZE);
    int eq = memcmp(read_buf.data(), &big_file_content[blockno * BSIZE], BSIZE);
    EXPECT_EQ(eq, 0);
  }
  return nullptr;
}

int write_max_size = MAXOPBLOCKS * BSIZE;
int read_max_size  = MAXOPBLOCKS * BSIZE;

void* unaligned_random_write_worker(void* _range) {
  auto range = (struct start_to_end*)_range;
  for (uint i = range->start; i < range->end;) {
    uint size = rand() % write_max_size;
    size      = std::min(range->end - i, size);

    inode_write_nbytes(single_inode, &big_file_content[i], size, i);
    i += size;
  }
  return nullptr;
}

void* unaligned_random_read_worker(void* _range) {
  auto range     = (struct start_to_end*)_range;
  char* read_buf = new char[read_max_size];
  for (uint i = range->start; i < range->end;) {
    uint size = rand() % write_max_size;
    size      = std::min(range->end - i, size);

    inode_read_nbytes(single_inode, read_buf, size, i);

    int eq = memcmp(read_buf, &big_file_content[i], size);
    EXPECT_EQ(eq, 0);
    i += size;
  }
  delete[] read_buf;
  return nullptr;
}

TEST(inode, unaligned_random_read_write_test) {
  write_max_size = MAXOPBLOCKS * BSIZE;
  read_max_size  = MAXOPBLOCKS * BSIZE;
  begin_op();
  single_inode = ialloc(T_FILE);
  end_op();

  for (char& c : big_file_content) {
    c = std::rand();
  }

  start_worker(unaligned_random_write_worker, 1, big_file_size);

  start_worker(unaligned_random_read_worker, 1, big_file_size);

  begin_op();
  iput(single_inode);
  end_op();
}

TEST(inode, big_unaligned_random_read_write_test) {
  write_max_size = MAXOPBLOCKS * 10 * BSIZE;
  read_max_size  = MAXOPBLOCKS * 10 * BSIZE;
  begin_op();
  single_inode = ialloc(T_FILE);
  end_op();

  for (char& c : big_file_content) {
    c = std::rand();
  }

  start_worker(unaligned_random_write_worker, 1, big_file_size);

  start_worker(unaligned_random_read_worker, 1, big_file_size);

  begin_op();
  iput(single_inode);
  end_op();
}

TEST(inode, parallel_big_unaligned_random_read_write_test) {
  write_max_size = MAXOPBLOCKS * 10 * BSIZE;
  read_max_size  = MAXOPBLOCKS * 10 * BSIZE;

  begin_op();
  single_inode = ialloc(T_FILE);
  end_op();

  for (char& c : big_file_content) {
    c = std::rand();
  }

  start_worker(unaligned_random_write_worker, 10, big_file_size);

  myfuse_log("write finished");

  start_worker(unaligned_random_read_worker, 10, big_file_size);

  begin_op();
  iput(single_inode);
  end_op();
}

TEST(inode, parrallel_block_aligned_read_write_test) {
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
    inode_read_nbytes(single_inode, read_buf.data(), BSIZE, i * BSIZE);
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