#include <pthread.h>

#include "buf_cache.h"
#include "util.h"
#include "block_device.h"
#include "sys/time.h"

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
    pthread_spin_init(&bcache.hash[i].lock, 0);
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

static uint hash(uint blockno) { return blockno % BCACHE_HASH_SIZE; }

static struct bcache_buf* bget(uint blockno) {
  struct bcache_buf* b;

  int hashid                    = hash(blockno);
  struct bcache_hashtbl* bucket = &bcache.hash[hashid];
  pthread_spin_lock(&bucket->lock);

  for (b = bucket->head.next; b != &bucket->head; b = b->next) {
    if (b->blockno == blockno) {
      b->refcnt++;
      pthread_spin_unlock(&bucket->lock);
      pthread_mutex_lock(&b->lock);
      return b;
    }
  }
  pthread_spin_unlock(&bucket->lock);

  pthread_spin_lock(&bcache.lock);

  // Not cached.
  // Recycle all hashtbl and get the least recent buf
  uint64_t least_recent_time_stamp = 0;
  least_recent_time_stamp--;
  struct bcache_buf* least_recent_buf = 0;
  for (int hidx = 0; hidx < BCACHE_HASH_SIZE; hidx++) {
    pthread_spin_lock(&bcache.hash[hidx].lock);
    for (b = bcache.hash[hidx].head.prev; b != &bcache.hash[hidx].head;
         b = b->prev) {
      if (b->refcnt == 0) {
        if (b->timestamp <= least_recent_time_stamp) {
          least_recent_buf        = b;
          least_recent_time_stamp = b->timestamp;
        }
      }
    }
  }

  if (least_recent_buf == 0) {
    err_exit("bget: no buffers");
  }

  b          = least_recent_buf;
  b->blockno = blockno;
  b->valid   = 0;
  b->refcnt  = 1;
  // unlink
  b->next->prev = b->prev;
  b->prev->next = b->next;

  // link
  b->next                 = bucket->head.next;
  b->prev                 = &bucket->head;
  bucket->head.next->prev = b;
  bucket->head.next       = b;
  for (int hidx = 0; hidx < BCACHE_HASH_SIZE; hidx++) {
    pthread_spin_unlock(&bcache.hash[hidx].lock);
  }

  pthread_spin_unlock(&bcache.lock);
  pthread_mutex_lock(&b->lock);

  return b;
}

struct bcache_buf* bread(uint blockno) {
  struct bcache_buf* b;

  b = bget(blockno);
  if (!b->valid) {
    read_block_raw(blockno, b->data);
    b->valid = 1;
  }
  return b;
}

void bwrite(struct bcache_buf* b) {
  if (!pthread_mutex_trylock(&b->lock)) {
    err_exit("bwrite called with unlocked buf");
  }

  write_block_raw(b->blockno, b->data);
}

static inline uint64_t current_timestamp() {
  struct timeval te;
  gettimeofday(&te, NULL);  // get current time
  long long milliseconds =
      te.tv_sec * 100LL + te.tv_usec / 100;  // calculate milliseconds
  return milliseconds;
}

void brelse(struct bcache_buf* b) {
  if (!pthread_mutex_trylock(&b->lock)) {
    err_exit("brelse called with unlocked buf");
  }

  pthread_mutex_unlock(&b->lock);
  int hashid                    = hash(b->blockno);
  struct bcache_hashtbl* bucket = &bcache.hash[hashid];
  pthread_spin_lock(&bucket->lock);

  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // update timestamp
    b->timestamp = current_timestamp();
  }

  pthread_spin_unlock(&bucket->lock);
}

void bpin(struct bcache_buf* b) {
  pthread_spin_lock(&bcache.lock);
  b->refcnt++;
  pthread_spin_unlock(&bcache.lock);
}

void bunpin(struct bcache_buf* b) {
  pthread_spin_lock(&bcache.lock);
  b->refcnt--;
  pthread_spin_unlock(&bcache.lock);
}