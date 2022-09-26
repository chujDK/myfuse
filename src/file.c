#include "file.h"
#include "inode.h"
#include "directory.h"
#include <stddef.h>
#include <fcntl.h>
#include <errno.h>
#include "log.h"

int myfuse_getattr(const char *path, struct stat *stbuf,
                   struct fuse_file_info *fi) {
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
  begin_op();
  struct inode *dp = path2inode(path);
  if (dp == NULL) {
    myfuse_log("`%s' is not exist", path);
    end_op();
    return -ENOENT;
  }

  ilock(dp);

  struct dirent *de;
  char *dirbuf = malloc(dp->size);
  char file_name[DIRSIZE + 1];
  struct stat st;

#ifdef DEBUG
  if (dp->type != T_DIR_INODE_MYFUSE) {
    free(dirbuf);
    iunlockput(dp);
    end_op();
    err_exit("dirlookup: file is not a dir");
  }
#endif

  if (inode_read_nbytes_locked(dp, dirbuf, dp->size, 0) != dp->size) {
    free(dirbuf);
    iunlockput(dp);
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
      filler(buf, file_name, &st, 0, 0);
    }
  }

  iunlockput(dp);
  free(dirbuf);
  end_op();

  return 0;
}