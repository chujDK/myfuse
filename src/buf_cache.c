#include <pthread.h>

#include "buf_cache.h"

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

static struct bcache bcache;

void binit() {
  struct bcache_buf* b;
  pthread_spin_init(&bcache.lock, 0);

  for (int i = 0; i < BCACHE_HASH_SIZE; i++) {
    pthread_spin_init(&bcache.lock, 0);
    bcache.hash[i].head.prev = &bcache.hash[i].head;
    bcache.hash[i].head.next = &bcache.hash[i].head;
  }

  // add all buffers to hash[0]
  for (b = bcache.buf; b < bcache.buf + NCACHE_BUF; b++) {
    b->next = bcache.hash[0].head.next;
    b->prev = &bcache.hash[0].head;
    pthread_mutex_init(&b->lock, NULL);
    bcache.hash[0].head.next->prev = b;
    bcache.hash[0].head.next       = b;
  }
}

struct bcache_buf* bget() {
  return NULL;
}