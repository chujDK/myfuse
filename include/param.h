#pragma once
#define FUSE_USE_VERSION 31

#include "fuse.h"
#include "fs.h"
#include "pthread.h"

struct myfuse_state {
  int fsfd;
  struct superblock sb;
};

struct options {
  const char* device_path;
  int show_help;
};

#define MYFUSE_STATE ((struct myfuse_state*)fuse_get_context()->private_data)
