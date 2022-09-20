#pragma once
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void err_exit(const char* msg, ...);

void myfuse_log(const char* msg, ...);

void myfuse_nonfatal(const char* msg, ...);

// weak symbol to make the test suite can rewrite the get_myfuse_state
// to provide state with no need to start a fuse runtime
struct myfuse_state* get_myfuse_state() __attribute__((weak));

#define MYFUSE_STATE (get_myfuse_state())