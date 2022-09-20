#include "util.h"
#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 31
#endif
#include "fuse.h"

void err_exit(const char* msg, ...) {
  char buf[128] = "\033[1;91m[-]\033[0m ";
  va_list arg;
  va_start(arg, msg);
  strncat(buf, msg, 128);
  strncat(buf, "\n", 128);
  vfprintf(stderr, buf, arg);
  va_end(arg);
  exit(1);
}

void myfuse_log(const char* msg, ...) {
#ifdef VERBOSE
  char buf[128] = "\033[1;92m[+]\033[0m ";
  va_list arg;
  va_start(arg, msg);
  strncat(buf, msg, 128);
  strncat(buf, "\n", 128);
  vfprintf(stdout, buf, arg);
  va_end(arg);
#endif
}

struct myfuse_state* get_myfuse_state() {
  return (struct myfuse_state*)fuse_get_context()->private_data;
}