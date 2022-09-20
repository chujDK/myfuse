#pragma once
#include "param.h"

// return: n bytes write
int write_block_raw(uint block_id, const char* buf);

// return: n bytes read
int read_block_raw(uint block_id, char* buf);
int read_block_raw_nbytes(uint block_id, char* buf, uint nbytes);

void block_init(const char* path_to_device);