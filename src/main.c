#define FUSE_USE_VERSION 31

#include "fuse.h"

int main(int argc, char* argv[]) {
  int ret;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  return 0;
}