//
// created by chuj 2022/9/19
//

// fs layers
// |       names/fds     |
// |        inode        |
// |     inode cache     |
// |         log         |
// |      buf cache      |
// | disk (block device) |
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>

#include "param.h"
#include "names_fds.h"

struct options options;

#define OPTION(t, p) \
  { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
    OPTION("--device_path=%s", device_path), OPTION("-h", show_help),
    OPTION("--help", show_help), FUSE_OPT_END};

static void show_help(const char* progname) {
  printf("usage: %s [options] <mountpoint>\n\n", progname);
  printf(
      "File-system specific options:\n"
      "    --device_path=<s>          Path to the disk device\n"
      "                               example: /dev/sdb\n"
      "\n");
}

static const struct fuse_operations myfuse_oper = {
    .init = myfuse_init,
};

#ifdef VERBOSE
void SIGSEVG_handler(int sig) {
  void* array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}
#else
void SIGSEVG_handler(int){};
#endif

int main(int argc, char* argv[]) {
  int ret;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  signal(SIGSEGV, SIGSEVG_handler);

  if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1) {
    return 1;
  }

  if (options.show_help) {
    show_help(argv[0]);
    assert(fuse_opt_add_arg(&args, "--help") == 0);
    args.argv[0][0] = '\0';
  }

  ret = fuse_main(args.argc, args.argv, &myfuse_oper, NULL);
  fuse_opt_free_args(&args);
  return ret;
}