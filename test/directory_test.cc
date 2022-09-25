#include "test_def.h"
#include <random>
#include <algorithm>

TestEnvironment* env;

std::string gen_random(const int len) {
  std::string alphanum =
      "0123456789_."
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";
  std::string tmp_s;
  tmp_s.reserve(len);

  for (int i = 0; i < len; ++i) {
    tmp_s += alphanum[rand() % (alphanum.length() - 1)];
  }

  return tmp_s;
}

TEST(directory, dir_link_lookup_test) {
  // first, get the root
  auto rootdp = path2inode("/");
  ASSERT_EQ(rootdp->inum, ROOTINO);
  iput(rootdp);

  // second, generate lots of paths
  const int name_sum       = 1000;
  const int max_paths      = 10000;
  const int max_path_depth = MAXOPBLOCKS / 6 + 5;
  std::set<std::string> names_set;
  while (names_set.size() < name_sum) {
    auto name = gen_random((rand() % (DIRSIZE - 1)) + 1);
    if (name != "." && name != "..") {
      names_set.insert(name);
    }
  }
  const std::vector<std::string> names(names_set.begin(), names_set.end());
  using Path = std::vector<std::string>;
  std::set<Path> paths_set;

  while (paths_set.size() < max_paths) {
    Path path;
    for (int j = 0; j <= std::rand() % max_path_depth; j++) {
      path.push_back(names[rand() % names.size()]);
    }
    paths_set.insert(path);
  }
  const std::vector<Path> paths(paths_set.begin(), paths_set.end());

  std::map<Path, uint> path2inum;

  for (const auto& path : paths) {
    begin_op();
    auto ip         = ialloc(T_DIR);
    std::string cwd = "/";
    uint last_inum  = 0;
    for (const auto& name : path) {
      auto dp = path2inode(cwd.c_str());
      ASSERT_NE(dp, nullptr);
      ilock(dp);
      dirlink(dp, name.c_str(), ip->inum);
      cwd += name;
      cwd += "/";
      last_inum = dp->inum;
      iunlockput(dp);
    }
    ASSERT_NE(last_inum, 0);
    path2inum[path] = last_inum;
    iput(ip);
    end_op();
  }

  // validate
  for (const Path& path : paths) {
    std::string last_dir = "";
    for (const auto& name : path) {
      last_dir += "/";
      last_dir += name;
    }

    auto dp = path2inode("/");
    uint poff;
    auto found_dp = dirlookup(dp, last_dir.c_str(), &poff);
    ASSERT_NE(found_dp, nullptr);
    EXPECT_EQ(found_dp->inum, path2inum[path]);
  }
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  env = reinterpret_cast<TestEnvironment*>(
      ::testing::AddGlobalTestEnvironment(new TestEnvironment()));
  if (env == nullptr) {
    err_exit("failed to init testing env!");
  }
  return RUN_ALL_TESTS();
}
