#pragma once
#define FUSE_USE_VERSION 31

#include "fuse.h"
#include "fs.h"
#include "pthread.h"
#include "util.h"
#define BCACHE_HASH_SIZE 13

struct myfuse_state {
  struct superblock sb;
};

struct options {
  const char* device_path;
  int show_help;
};
