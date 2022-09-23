#include "directory.h"
#include <pthread.h>

struct inode* dirlookup(struct inode* dp, char* name, uint* poff) {}

int dirlink(struct inode* dp, char* name, uint inum) {
  int off;
  struct dirent de;
  struct inode* ip;

#ifdef DEBUG
  // pthread_mutex_trylock: return 0 if not locked
  if (!pthread_mutex_trylock(&dp->lock)) {
    err_exit("dirlink called with unlocked inode");
  }
#endif

  // Check that name is not present.
  if ((ip = dirlookup(dp, name, 0)) != 0) {
    iput(ip);
    return -1;
  }

  // Look for an empty dirent.
  // this is a userland program, we can do this to save time
  char buf[BSIZE];
  if (inode_read_nbytes_unlocked(ip, buf, dp->size, 0) != dp->size) {
    err_exit("dirlink: read failed");
  }
  for (off = 0; off < dp->size; off += sizeof(de)) {
    if (inode_read_nbytes_unlocked(dp, (char*)&de, sizeof(de), off) !=
        sizeof(de)) {
    }
    if (de.inum == 0) break;
  }

  strncpy(de.name, name, DIRSIZE);
  de.inum = inum;
  if (inode_write_nbytes_unlocked(dp, (char*)&de, sizeof(de), off) !=
      sizeof(de)) {
    err_exit("dirlink: write entry failed");
  }

  return 0;
}
