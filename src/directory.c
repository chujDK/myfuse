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

// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char* skipelem(char* path, char* name) {
  char* s;
  int len;

  while (*path == '/') path++;
  if (*path == 0) return 0;
  s = path;
  while (*path != '/' && *path != 0) path++;
  len = path - s;
  if (len >= DIRSIZE)
    memmove(name, s, DIRSIZE);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  while (*path == '/') path++;
  return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode* namex(char* path, int nameiparent, char* name) {
  struct inode *ip, *next;

  if (*path == '/')
    ip = iget(ROOTINO);
  else
    ip = idup(MYFUSE_STATE->cwd);

  while ((path = skipelem(path, name)) != 0) {
    ilock(ip);
    if (ip->type != T_DIR) {
      iunlockput(ip);
      return 0;
    }
    if (nameiparent && *path == '\0') {
      // Stop one level early.
      iunlock(ip);
      return ip;
    }
    if ((next = dirlookup(ip, name, 0)) == 0) {
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  if (nameiparent) {
    iput(ip);
    return 0;
  }
  return ip;
}

struct inode* path2inode(char* path) {
  char name[DIRSIZE];
  return namex(path, 0, name);
}

struct inode* path2parentinode(char* path, char* name) {
  return namex(path, 1, name);
}
