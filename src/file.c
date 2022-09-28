#include "file.h"
#include "inode.h"
#include "directory.h"
#include <stddef.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include "log.h"

#define NFILE_INIT 50

struct ftable {
  pthread_spinlock_t lock;
  struct file **files;
  uint nfile;
} ftable;

void ftable_grow() {
  ftable.files =
      realloc(ftable.files, 2 * ftable.nfile * sizeof(struct file *));
  ftable.nfile *= 2;
  for (int i = ftable.nfile / 2; i < ftable.nfile; i++) {
    ftable.files[i] = calloc(1, sizeof(struct file));
  }
}

void file_init() {
  ftable.files = malloc(sizeof(struct file *) * NFILE_INIT);
  ftable.nfile = NFILE_INIT;
  pthread_spin_init(&ftable.lock, PTHREAD_PROCESS_SHARED);
  for (int i = 0; i < ftable.nfile; i++) {
    ftable.files[i] = calloc(1, sizeof(struct file));
  }
}

struct file *filealloc() {
  struct file *f;
  pthread_spin_lock(&ftable.lock);
  for (int i = 0; i < ftable.nfile; i++) {
    if (ftable.files[i]->ref == 0) {
      f      = ftable.files[i];
      f->ref = 1;
      pthread_spin_unlock(&ftable.lock);
      return f;
    }
  }
  ftable_grow();
  f      = ftable.files[ftable.nfile / 2];
  f->ref = 1;
  pthread_spin_unlock(&ftable.lock);
  return f;
}

struct file *filedup(struct file *f) {
  pthread_spin_lock(&ftable.lock);
  if (f->ref < 1) {
    err_exit("filedup: ref < 1");
  };
  f->ref++;
  pthread_spin_unlock(&ftable.lock);
  return f;
}

void fileclose(struct file *f) {
  pthread_spin_lock(&ftable.lock);

  if (f->ref < 1) {
    err_exit("fileclose: ref < 1");
  }

  if (--f->ref > 0) {
    pthread_spin_unlock(&ftable.lock);
    return;
  }

  struct inode *ip = f->ip;
  f->ref           = 0;
  pthread_spin_unlock(&ftable.lock);

  begin_op();
  iput(ip);
  end_op();
}

int filestat(struct file *f, struct stat *stbuf) {
  if (f->type == FD_INODE) {
    begin_op();
    ilock(f->ip);
    int res = stat_inode(f->ip, stbuf);
    iunlock(f->ip);
    end_op();
    return res;
  } else {
    err_exit("filestat: unknown file type");
    // unreachable
    return -1;
  }
}

int fileread(struct file *f, char *dst, size_t n) {
  int nread = 0;

  if (f->readable == 0) {
    return -1;
  }

  if (f->ip->type == T_FILE_INODE_MYFUSE) {
    return -EISDIR;
  }

  if (f->type == FD_INODE) {
    nread = inode_read_nbytes_unlocked(f->ip, dst, f->off, n);
    f->off += nread;
  } else {
    myfuse_log("fileread: unknown file type");
    // unreachable
    return -1;
  }

  return nread;
}

int filewrite(struct file *f, const char *src, size_t n) {
  int nwrite = 0;

  if (f->writable == 0) {
    return -1;
  }

  if (f->ip->type == T_FILE_INODE_MYFUSE) {
    return -EISDIR;
  }

  if (f->type == FD_INODE) {
    nwrite = inode_write_nbytes_unlocked(f->ip, src, f->off, n);
    f->off += nwrite;
  }

  return nwrite;
}

int myfuse_getattr(const char *path, struct stat *stbuf,
                   struct fuse_file_info *fi) {
  (void)fi;
  begin_op();
  struct inode *ip = path2inode(path);
  int res          = 0;

  if (ip == NULL) {
    myfuse_log("`%s' is not exist", path);
    end_op();
    res = -ENOENT;
    return res;
  }
  ilock(ip);
  res = stat_inode(ip, stbuf);
  iunlockput(ip);
  end_op();

  return res;
}

int myfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info *fi,
                   enum fuse_readdir_flags flags) {
  if (fi == NULL) {
    return -ENOENT;
  }
  begin_op();
  struct inode *dp = ((struct file *)fi->fh)->ip;
  if (dp == NULL) {
    myfuse_log("`%s' is not exist", path);
    end_op();
    return -ENOENT;
  }

  DEBUG_TEST(if (dp->ref < 1) { err_exit("myfuse_readdir: ref < 1"); });

  ilock(dp);

  struct dirent *de;
  char *dirbuf = malloc(dp->size);
  char file_name[DIRSIZE + 1];
  struct stat st;

  DEBUG_TEST(if (dp->type != T_DIR_INODE_MYFUSE) {
    free(dirbuf);
    iunlock(dp);
    end_op();
    return -EBADF;
  });

  if (inode_read_nbytes_locked(dp, dirbuf, dp->size, 0) != dp->size) {
    free(dirbuf);
    iunlock(dp);
    end_op();
    err_exit("dirlookup: failed to read dir");
  }

  for (int off = 0; off < dp->size; off += sizeof(*de)) {
    de = (struct dirent *)&dirbuf[off];
    if (de->inum == 0) {
      continue;
    }
    dirnamecpy(file_name, de->name);

    int stat_res = 0;
    if (de->inum != dp->inum) {
      stat_res = stat_inum(de->inum, &st);
    } else {
      stat_res = stat_inode(dp, &st);
    }
    if (stat_res == 0) {
      if (filler(buf, file_name, &st, 0, 0) != 0) {
        break;
      }
    }
  }

  iunlock(dp);
  free(dirbuf);
  end_op();

  return 0;
}

int myfuse_opendir(const char *path, struct fuse_file_info *fi) {
  begin_op();
  struct inode *dir_inode = path2inode(path);
  if (dir_inode == NULL) {
    end_op();
    return -ENOENT;
  }
  ilock(dir_inode);
  if (dir_inode->type != T_DIR_INODE_MYFUSE) {
    iunlockput(dir_inode);
    end_op();
    return -ENOTDIR;
  }
  struct file *file = filealloc();
  file->type        = FD_INODE;
  file->ip          = dir_inode;
  fi->fh            = (size_t)file;
  DEBUG_TEST(if (file->ip->ref < 1) { err_exit("myfuse_opendir: ref < 1"); });
  iunlock(dir_inode);
  end_op();
  return 0;
}

int myfuse_open(const char *path, struct fuse_file_info *fi) {
  begin_op();
  struct inode *file_inode = path2inode(path);
  if (file_inode == NULL) {
    if ((fi->flags & O_CREAT) == 0) {
      return -ENOENT;
    } else {
      // create the file
      char filename[DIRSIZE];
      struct inode *dir_inode = path2parentinode(path, filename);
      file_inode              = ialloc(T_FILE_INODE_MYFUSE);
      file_inode->nlink++;
      iupdate(dir_inode);
      ilock(dir_inode);
      dirlink(dir_inode, filename, file_inode->inum);
      iunlockput(dir_inode);
    }
  }
  ilock(file_inode);
  if (file_inode->type == T_DIR_INODE_MYFUSE) {
    iunlockput(file_inode);
    return -EISDIR;
  }
  struct file *file = filealloc();
  file->type        = FD_INODE;
  file->ip          = file_inode;
  if (fi->flags & O_RDONLY) {
    file->readable = 1;
    file->writable = 0;
  } else if (fi->flags & O_WRONLY) {
    file->readable = 0;
    file->writable = 1;
  } else if (fi->flags & O_RDWR) {
    file->readable = 1;
    file->writable = 1;
  } else {
    // FIXME: use errcode instead of err_exit
    iunlockput(file_inode);
    end_op();
    err_exit("myfuse_open: unknown flag");
  }

  fi->fh = (size_t)file;
  iunlock(file_inode);
  end_op();
  return 0;
}

static void release_internal(struct fuse_file_info *fi) {
  if (fi == NULL) {
    return;
  }
  struct file *file = (struct file *)fi->fh;
  if (file == NULL) {
    return;
  }
  fi->fh = 0;
  fileclose(file);
}

int myfuse_release(const char *path, struct fuse_file_info *fi) {
  release_internal(fi);
  return 0;
}

int myfuse_releasedir(const char *path, struct fuse_file_info *fi) {
  release_internal(fi);
  return 0;
}

int myfuse_mkdir(const char *path, mode_t mode) {
  int res = 0;
  char name[DIRSIZE];

  begin_op();

  struct inode *dp = path2parentinode(path, name);

  struct inode *ip = ialloc(T_DIR_INODE_MYFUSE);
  ip->nlink++;
  ilock(ip);

  ilock(dp);
  if (dirlink(dp, name, ip->inum) != 0) {
    res = -EEXIST;
    iunlockput(ip);
  }
  iunlockput(dp);

  ip->nlink = 1;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return res;
}

int myfuse_unlink(const char *path) {
  int res = 0;
  char name[DIRSIZE];
  struct dirent zero_de;
  uint poff;
  begin_op();

  memset(&zero_de, 0, sizeof(zero_de));

  struct inode *ip = path2inode(path);
  struct inode *dp = path2parentinode(path, name);
  if (ip == NULL) {
    myfuse_log("`%s' is not exist", path);
    end_op();
    return -ENOENT;
  }

  DEBUG_TEST(assert(dp != NULL););

  ilock(dp);
  ilock(ip);

  if (ip->nlink == 0) {
    // dangling symbolic link
    end_op();
    return -ENONET;
  }

  ip->nlink--;
  if (ip->nlink == 0) {
    if (dirlookup(dp, name, &poff) != NULL) {
      inode_write_nbytes_locked(dp, (char *)&zero_de, sizeof(zero_de), poff);
    }
  }
  iupdate(ip);
  iunlockput(ip);
  iunlockput(dp);

  end_op();

  return res;
}

int myfuse_rmdir(const char *path) {
  int res = 0;
  char name[DIRSIZE];
  struct dirent zero_de;
  uint poff;

  memset(&zero_de, 0, sizeof(zero_de));

  begin_op();
  struct inode *ip = path2inode(path);
  struct inode *dp = path2parentinode(path, name);

  if (ip == NULL) {
    myfuse_log("`%s' is not exist", path);
    end_op();
    return -ENOENT;
  }

  if (ip->size != 0) {
    char *buf = malloc(ip->size);
    ilock(ip);
    if (inode_read_nbytes_locked(ip, buf, ip->size, 0) != ip->size) {
      iunlock(ip);
      end_op();
      err_exit("myfuse_rmdir: inode_read_nbytes_unlocked failed to read");
    }

    for (int i = 0; i < ip->size; i += sizeof(struct dirent)) {
      struct dirent *de = (struct dirent *)(buf + i);
      if (de->inum != 0) {
        myfuse_log("myfuse_rmdir: `%s' is not empty", path);
        free(buf);
        iunlock(ip);
        end_op();
        return -ENOTEMPTY;
      }
    }
    iunlock(ip);
  }

  ilock(dp);
  ilock(ip);
  ip->nlink--;
  if (ip->nlink == 0) {
    if (dirlookup(dp, name, &poff) != NULL) {
      inode_write_nbytes_locked(dp, (char *)&zero_de, sizeof(zero_de), poff);
    }
  }
  iupdate(ip);

  iunlockput(ip);
  iunlockput(dp);
  end_op();

  return res;
}

int myfuse_read(const char *path, char *buf, size_t size, off_t offset,
                struct fuse_file_info *fi) {
  struct file *file = (struct file *)fi->fh;
  DEBUG_TEST(assert(file != NULL););
  int nbytes = inode_read_nbytes_unlocked(file->ip, buf, size, offset);
  return nbytes;
}

int myfuse_write(const char *path, const char *buf, size_t size, off_t offset,
                 struct fuse_file_info *fi) {
  struct file *file = (struct file *)fi->fh;
  DEBUG_TEST(assert(file != NULL););
  int nbytes = inode_write_nbytes_unlocked(file->ip, buf, size, offset);
  return nbytes;
}

int myfuse_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
  (void)fi;
  begin_op();
  struct inode *ip = path2inode(path);

  if (ip == NULL) {
    end_op();
    return -ENOENT;
  }
  if (ip->type == T_DIR_INODE_MYFUSE) {
    end_op();
    return -EISDIR;
  }

  ilock(ip);
  itrunc2size(ip, size);
  iunlockput(ip);
  end_op();
  return 0;
}

int myfuse_access(const char *path, int mask) {
  (void)mask;
  begin_op();
  struct inode *ip = path2inode(path);
  if (ip == NULL) {
    return -ENOENT;
  }
  iput(ip);
  end_op();

  return 0;
}
