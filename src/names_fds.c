#include "names_fds.h"
#include "block_device.h"
#include "buf_cache.h"
#include <malloc.h>

extern struct options options;

void *myfuse_init(struct fuse_conn_info *conn, struct fuse_config *config) {
  struct myfuse_state *state = malloc(sizeof(struct myfuse_state));
  if (state == NULL) {
    err_exit("failed to allocate memory for myfuse_state");
  }

  block_init(options.device_path);

  if (read_block_raw_nbytes(SUPERBLOCK_ID, (char *)&state->sb,
                            sizeof(struct superblock)) !=
      sizeof(struct superblock)) {
    err_exit("failed to read super block");
  }

  if (state->sb.magic != FSMAGIC) {
    err_exit("disk magic not match! read %x", state->sb.magic);
  }

  // block cache init
  binit();

  myfuse_log("fs init done; size %d", state->sb.size);
  return state;
}