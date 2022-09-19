#pragma once
#include "param.h"
#include "util.h"

void *myfuse_init(struct fuse_conn_info *conn, struct fuse_config *config);