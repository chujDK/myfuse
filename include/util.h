#pragma once
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void err_exit(const char* msg, ...);

void myfuse_log(const char* msg, ...);

void myfuse_nonfatal(const char* msg, ...);

struct myfuse_state* get_myfuse_state()
#ifdef DEBUG
    // weak symbol to make the test suite can rewrite the get_myfuse_state
    // to provide state with no need to start a fuse runtime
    __attribute__((weak))
#endif
    ;

#define MYFUSE_STATE (get_myfuse_state())