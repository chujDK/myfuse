// this file should be include after param.h
#pragma once

struct bcache_buf {
  int valid;  // has data read from disk?
  int disk;   // does disk own buf?
  uint blockno;
  pthread_mutex_t lock;
  uint refcnt;
  struct bcache_buf* prev;
  struct bcache_buf* next;
  uint timestamp;
  u_char data[BSIZE];
};

struct bcache_hashtbl {
  pthread_spinlock_t lock;
  struct bcache_buf head;
};

struct bcache {
  pthread_spinlock_t lock;
  struct bcache_buf buf[NCACHE_BUF];

  struct bcache_hashtbl hash[BCACHE_HASH_SIZE];
};
