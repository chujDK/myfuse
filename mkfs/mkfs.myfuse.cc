#include <cstdio>
#include <cmath>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <cassert>
#include <sys/stat.h>
#include <algorithm>
#include <string>
#include <iostream>

// this give us some utilities
#include "mkfs.myfuse-util.h"

// Disk layout
// [ boot block (skip) | super block | log | inode blocks |
//                                           free bit map | data blocks]

int main(int argc, char* argv[]) {
  std::string user_decide;

  if (argc != 2) {
    err_exit(
        "Usage: mkfs /dev/<disk name>\n"
        "\tNote: the disk has to have a sector size of 512\n");
  }

  assert(BSIZE % sizeof(struct dinode) == 0);
  assert(BSIZE % sizeof(struct dirent) == 0);

  fprintf(stderr, "This program will format the disk %s\n", argv[1]);
  fprintf(stderr, "Continue? yes/no >");
  std::cin >> user_decide;
  std::transform(user_decide.begin(), user_decide.end(), user_decide.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (user_decide.substr(0, 3) == "yes") {
    err_exit("canceled");
  }

  struct stat st;
  stat(argv[1], &st);
  return 0;
}
