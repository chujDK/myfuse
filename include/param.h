#pragma once
#define FUSE_USE_VERSION 31

#include "fuse.h"
#include "fs.h"
#include "pthread.h"
#include "util.h"
#define BCACHE_HASH_SIZE 31

struct myfuse_state {
  struct superblock sb;
  struct inode* cwd;  // current working directory
};

struct options {
  const char* device_path;
  int show_help;
};
