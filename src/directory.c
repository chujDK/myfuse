#include "directory.h"
#include <pthread.h>
#include <malloc.h>
#include <inode.h>

static int dirnamecmp(char* a, char* b) { return strncmp(a, b, DIRSIZE); }

struct inode* dirlookup(struct inode* dp, char* name, uint* poff) {
  struct dirent* de;
  char* buf = malloc(dp->size);

#ifdef DEBUG
  if (dp->type != T_DIR) {
    free(buf);
    err_exit("dirlookup: file is not a dir");
  }
#endif

  if (inode_read_nbytes_unlocked(dp, buf, dp->size, 0) != dp->size) {
    free(buf);
    err_exit("dirlookup: failed to read dir");
  }

  for (int off = 0; off < dp->size; off += sizeof(*de)) {
    de = (struct dirent*)&buf[off];
    if (de->inum == 0) {
      continue;
    }
    if (dirnamecmp(de->name, name) == 0) {
      return iget(de->inum);
    }
  }

  free(buf);
  return NULL;
}

int dirlink(struct inode* dp, char* name, uint inum) {
  int off;
  struct dirent* de;
  struct inode* ip;

#ifdef DEBUG
  // pthread_mutex_trylock: return 0 if not locked
  if (!pthread_mutex_trylock(&dp->lock)) {
    err_exit("dirlink called with unlocked inode");
  }

  // test the inode is a directory
  if (dp->type != T_DIR) {
    err_exit("dirlink called with non-directory inode");
  }

  // test the directory size
  if (dp->size % sizeof(struct dirent) != 0) {
    err_exit("dirlink called with directory inode with invalid size");
  }
#endif

  // Check that name is not present.
  if ((ip = dirlookup(dp, name, 0)) != 0) {
    iput(ip);
    return -1;
  }

  // Look for an empty dirent.
  // this is a userland program, we can do this to save time
  char* buf = malloc(dp->size);
  if (inode_read_nbytes_locked(ip, buf, dp->size, 0) != dp->size) {
    free(buf);
    err_exit("dirlink: read failed");
  }
  for (off = 0; off < dp->size; off += sizeof(*de)) {
    de = (struct dirent*)&buf[off];
    if (de->inum == 0) break;
  }

  strncpy(de->name, name, DIRSIZE);
  de->inum = inum;
  if (inode_write_nbytes_locked(dp, (char*)&de, sizeof(de), off) !=
      sizeof(de)) {
    free(buf);
    err_exit("dirlink: write entry failed");
  }

  free(buf);

  return 0;
}
