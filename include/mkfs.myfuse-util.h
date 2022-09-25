#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include "param.h"
#include "block_device.h"
#include "buf_cache.h"
#include "log.h"
#include "inode.h"
#include "block_allocator.h"
#include "directory.h"
#ifdef __cplusplus
}
#endif
#include <unistd.h>
#include <cstdlib>
#include <array>

struct myfuse_state* get_myfuse_state();

void init_super_block(uint disk_size_in_sector_block);
