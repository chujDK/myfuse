#include "block_device.h"
#include <fcntl.h>
#include <unistd.h>

int write_block_raw(uint block_id, const char *buf) {
  return write_block_raw_byfd(MYFUSE_STATE->fsfd, block_id, buf);
}

int write_block_raw_byfd(int fd, uint block_id, const char *buf) {
  if (lseek(fd, block_id * BSIZE, SEEK_SET) != block_id * BSIZE) {
    return -1;
  }
  return write(fd, buf, BSIZE);
}

int read_block_raw_byfd(int fd, uint block_id, char *buf) {
  if (lseek(fd, block_id * BSIZE, SEEK_SET) != block_id * BSIZE) {
    return -1;
  }
  return read(fd, buf, BSIZE);
}

int read_block_raw(uint block_id, char *buf) {
  return read_block_raw_byfd(MYFUSE_STATE->fsfd, block_id, buf);
}

int read_block_raw_nbytes_byfd(int fd, uint block_id, char *buf, uint nbytes) {
  if (lseek(fd, block_id * BSIZE, SEEK_SET) != block_id * BSIZE) {
    return -1;
  }
  if (nbytes > BSIZE) {
    nbytes = BSIZE;
  }
  return read(fd, buf, nbytes);
}

int read_block_raw_nbytes(uint block_id, char *buf, uint nbytes) {
  return read_block_raw_nbytes_byfd(MYFUSE_STATE->fsfd, block_id, buf, nbytes);
}