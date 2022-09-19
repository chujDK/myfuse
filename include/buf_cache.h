#pragma once
#include "param.h"
#include "bcache_def.h"
#include "pthread.h"

// buffed read and write
// Return a locked buf with the contents of the indicated block
struct buf* bread(uint block_idx);

// Write back block to disk
void* bwrite(uint block_idx);

// Release a locked buffer
void brelse(struct buf* b);

void bpin(struct buf* b);
void bunpin(struct buf* b);

void binit(struct bcache* cache);
