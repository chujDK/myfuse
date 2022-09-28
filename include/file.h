#pragma once
#include "param.h"
#include "util.h"

int myfuse_getattr(const char* path, struct stat* stbuf,
                   struct fuse_file_info* fi);

int myfuse_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info* fi,
                   enum fuse_readdir_flags flags);

int myfuse_open(const char* path, struct fuse_file_info* fi);

int myfuse_mkdir(const char* path, mode_t mode);

int myfuse_unlink(const char* path);

int myfuse_opendir(const char* path, struct fuse_file_info* fi);

int myfuse_release(const char* path, struct fuse_file_info* fi);

int myfuse_releasedir(const char* path, struct fuse_file_info* fi);

int myfuse_read(const char* path, char* buf, size_t size, off_t offset,
                struct fuse_file_info* fi);

int myfuse_write(const char* path, const char* buf, size_t size, off_t offset,
                 struct fuse_file_info* fi);

int myfuse_rmdir(const char* path);

void file_init();

enum FD_TYPE {
  FD_NONE = 0,
  FD_INODE,
  FD_DEVICE,
};
struct file {
  enum FD_TYPE type;
  int ref;
  int readable;
  int writable;
  struct inode* ip;
  uint off;
};
struct file;
struct file* filealloc();
struct file* filedup(struct file* f);
void fileclose(struct file* f);
int filestat(struct file* f, struct stat* stbuf);
int fileread(struct file* f, char* dst, size_t n);
int filewrite(struct file* f, const char* src, size_t n);