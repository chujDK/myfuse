#pragma once
#include <gtest/gtest.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "block_device.h"
#include "util.h"
#ifdef __cplusplus
}
#include <unistd.h>
#include <cstdlib>
#endif

#define MAX_BLOCK_NO 400000

struct myfuse_state* get_myfuse_state();

class TestEnvironment : public ::testing::Environment {
 public:
  void SetUp() override { block_init("./disk.img"); }
};