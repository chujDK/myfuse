#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "fs.h"

// Disk layout
// [ boot block (skip) | super block | log | inode blocks |
//                                           free bit map | data blocks]
int nbitmap;
int ninode_blocks;  // blocks that stores the on-disk inodes
int ninodes;        // maximum number of inodes
const int nlog = NLOG;
int nmeta_blocks;
int nblocks;     // sum of the data blocks
uint disk_size;  // sum of the disk's blocks
static struct superblock sb;

const int sector_size             = 512;
const int sector_per_block        = BSIZE / sector_size;
const static char zero_buf[BSIZE] = {0};
static char data_buf[BSIZE];

void err_exit(const char* msg, ...) {
  char buf[128] = "\033[1;91m[-]\033[0m ";
  va_list arg;
  va_start(arg, msg);
  strncat(buf, msg, 127);
  strncat(buf, "\n", 127);
  vfprintf(stderr, buf, arg);
  va_end(arg);
  exit(1);
}

void mkfs_log(const char* msg, ...) {
#ifdef VERBOSE
  char buf[128] = "\033[1;92m[+]\033[0m ";
  va_list arg;
  va_start(arg, msg);
  strncat(buf, msg, 127);
  strncat(buf, "\n", 127);
  vfprintf(stdout, buf, arg);
  va_end(arg);
#endif
}

void write_block_raw(int fsfd, uint index, const char* buf) {
  if (lseek(fsfd, index * BSIZE, 0) != index * BSIZE) {
    err_exit("lseek error seeking %u", index);
  }
  if (write(fsfd, buf, BSIZE) != BSIZE) {
    err_exit("write error writing %u", index);
  }
}

int main(int argc, char* argv[]) {
  char disk_name[16];
  char disk_attr[32];
  char user_decide[8];
  FILE* attr_file;

  if (argc != 2) {
    err_exit(
        "Usage: mkfs /dev/<disk name>\n"
        "\tNote: the disk has to have a sector size of 512\n");
  }

  assert(BSIZE % sizeof(struct dinode) == 0);
  assert(BSIZE % sizeof(struct dirent) == 0);
  assert(memcmp(argv[1], "/dev/", 5) == 0);

  int fsfd = open(argv[1], O_RDWR, 0666);
  if (fsfd < 0) {
    err_exit("Failed to open disk");
  }

  sscanf(argv[1], "/dev/%15s", disk_name);
  mkfs_log("Disk name: %s", disk_name);

  fprintf(stderr, "This program will format the disk %s\n", disk_name);
  fprintf(stderr, "Continue? yes/no >");
  scanf("%7s", user_decide);
  if (strcmp(user_decide, "yes") != 0) {
    err_exit("canceled");
  }

  // adjust the disk layout
  // Get the section's sum
  sprintf(disk_attr, "/sys/block/%s/size", disk_name);
  attr_file = fopen(disk_attr, "r");
  if (attr_file == NULL) {
    err_exit("open %s failed", disk_attr);
  }
  fscanf(attr_file, "%u", &disk_size);
  assert((disk_size % sector_per_block) == 0);
  disk_size /= sector_per_block;
  mkfs_log("disk has %u blocks", disk_size);

  nbitmap       = disk_size / (BSIZE * 8) + 1;
  ninode_blocks = ceil(disk_size / 100) + 1;
  ninodes       = ninode_blocks * IPB;
  nmeta_blocks  = 2 + nlog + ninode_blocks + nbitmap;
  nblocks       = disk_size - nmeta_blocks;
  mkfs_log("this disk can have about %.2lf GiB storage",
           ((nblocks * 1024.0) / 1024.0 / 1024.0 / 1024.0) * 0.98);
  printf(
      "nmeta %d (boot, super, log blocks %u inode blocks %u, bitmap blocks %u) "
      "blocks %d total %d\n",
      nmeta_blocks, nlog, ninode_blocks, nbitmap, nblocks, disk_size);
  printf("%d bytes per-block\n", BSIZE);

  sb.magic      = FSMAGIC;
  sb.ninodes    = ninodes;
  sb.size       = disk_size;
  sb.nblocks    = nblocks;
  sb.nlog       = nlog;
  sb.logstart   = 2;
  sb.inodestart = 2 + nlog;
  sb.bmapstart  = 2 + nlog + ninode_blocks;

  // write super block
  memset(data_buf, 0, sizeof(data_buf));
  memmove(data_buf, &sb, sizeof(sb));
  write_block_raw(fsfd, 1, data_buf);

  for (int i = 2; i < nmeta_blocks; i++) {
    write_block_raw(fsfd, i, zero_buf);
  }

  // write bitmap
  uint nmeta_bitmap_block     = nmeta_blocks / BPB;
  uint nmeta_bitmap_bit       = nmeta_blocks % BPB;
  uint nmeta_bitmap_byte      = nmeta_bitmap_bit / 8;
  uint nmeta_bitmap_left_bits = nmeta_bitmap_bit % 8;
  uint meta_bitmap_left_byte  = 0;
  for (int i = 0; i < nmeta_bitmap_left_bits; i++) {
    meta_bitmap_left_byte |= (1 << i);
  }
  for (int i = sb.bmapstart; i < sb.bmapstart + nmeta_bitmap_block; i++) {
    memset(data_buf, 0xFF, sizeof(data_buf));
    write_block_raw(fsfd, i, data_buf);
  }
  memset(data_buf, 0xFF, nmeta_bitmap_byte);
  data_buf[nmeta_bitmap_byte] = meta_bitmap_left_byte;
  write_block_raw(fsfd, nmeta_bitmap_block + sb.bmapstart, data_buf);

  return 0;
}
