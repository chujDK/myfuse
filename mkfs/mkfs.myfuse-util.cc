#include <cmath>
#include "mkfs.myfuse-util.h"

void init_super_block(uint disk_size_in_sector_block) {
  auto sb = &MYFUSE_STATE->sb;

  const uint disk_size     = disk_size_in_sector_block;
  const uint nlog          = NLOG;
  const uint nbitmap       = disk_size / BPB + 1;
  const uint ninode_blocks = ceil(disk_size / 100) + 1;
  const uint ninodes       = ninode_blocks * IPB;
  const uint nmeta_blocks  = 2 + nlog + ninode_blocks + nbitmap;
  const uint nblocks       = disk_size - nmeta_blocks;
  sb->magic                = FSMAGIC;
  sb->ninodes              = ninodes;
  sb->size                 = disk_size;
  sb->nblocks              = nblocks;
  sb->nlog                 = nlog;
  sb->logstart             = 2;
  sb->inodestart           = 2 + nlog;
  sb->bmapstart            = 2 + nlog + ninode_blocks;
}

struct myfuse_state* get_myfuse_state() {
  static struct myfuse_state state;
  return &state;
}
