#include <pthread.h>

#include "buf_cache.h"

void binit(struct bcache* cache) {
  struct bcache_buf* b;
  pthread_spin_init(&cache->lock, 0);

  for (int i = 0; i < BCACHE_HASH_SIZE; i++) {
    pthread_spin_init(&cache->lock, 0);
    cache->hash[i].head.prev = &cache->hash[i].head;
    cache->hash[i].head.next = &cache->hash[i].head;
  }

  // add all buffers to hash[0]
  for (b = cache->buf; b < cache->buf + NCACHE_BUF; b++) {
    b->next = cache->hash[0].head.next;
    b->prev = &cache->hash[0].head;
    pthread_mutex_init(&b->lock, NULL);
    cache->hash[0].head.next->prev = b;
    cache->hash[0].head.next       = b;
  }
}