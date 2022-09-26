#pragma once
#include "param.h"
#include "util.h"

int myfuse_getattr(const char* path, struct stat* stbuf,
                   struct fuse_file_info* fi);

int myfuse_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info* fi,
                   enum fuse_readdir_flags flags);