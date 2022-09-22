#include "inode.h"
#include <pthread.h>
#include <malloc.h>
#include <assert.h>
#include "log.h"

struct {
  pthread_spinlock_t lock;
  size_t ninode;
  struct inode* inode;
} itable;

pthread_spinlock_t block_alloc_lock;

int inode_init() {
  pthread_spin_init(&itable.lock, 1);
#define NINODE_INIT 30
  itable.ninode = NINODE_INIT;
  itable.inode  = malloc(sizeof(struct inode) * NINODE_INIT);
  for (int i = 0; i < NINODE_INIT; i++) {
    pthread_mutex_init(&itable.inode[i].lock, NULL);
  }

  pthread_spin_init(&block_alloc_lock, 1);
  return 0;
}

static void itable_grow() {
  if (!pthread_spin_trylock(&itable.lock)) {
    err_exit("itable_grow called no within itable locked");
  }
  itable.ninode *= 2;
  itable.inode = realloc(itable.inode, sizeof(struct inode) * itable.ninode);
  for (int i = itable.ninode / 2; i < itable.ninode; i++) {
    pthread_mutex_init(&itable.inode[i].lock, NULL);
  }
}

// return the {inum}-th inode's in memory copy
// the inode isn't locked and haven't read from disk
static struct inode* iget(uint inum) {
  struct inode* empty_victim = 0;
  pthread_spin_lock(&itable.lock);
  for (struct inode* ip = itable.inode; ip < itable.inode + itable.ninode;
       ip++) {
    if (ip->inum == inum && ip->ref > 0) {
      ip->ref++;
      pthread_spin_unlock(&itable.lock);
      return ip;
    }
    if (ip->ref == 0 && empty_victim == 0) {
      empty_victim = ip;
    }
  }

  if (empty_victim != 0) {
    empty_victim->inum  = inum;
    empty_victim->ref   = 1;
    empty_victim->valid = 0;
    pthread_spin_unlock(&itable.lock);
    return empty_victim;
  }
  // reach here, means the inode cache is too small
  itable_grow();
  pthread_spin_unlock(&itable.lock);
  return iget(inum);
}

// currently only type FILE is allowed
struct inode* ialloc(short type) {
  struct bcache_buf* bp;
  struct dinode* dip;

  for (int inum = 1; inum < MYFUSE_STATE->sb.ninodes; inum++) {
    bp  = logged_read(IBLOCK(inum));
    dip = (struct dinode*)bp->data + inum % IPB;
    if (dip->type == T_UNUSE) {
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      logged_write(bp);
      logged_relse(bp);
      return iget(inum);
    }
    logged_relse(bp);
  }

  // no on disk inodes avaliable
  // too much file, for simple, this can't be recover
  // shut the file system down
  err_exit("too much file on disk!");
  return NULL;
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode* idup(struct inode* ip) {
  pthread_spin_lock(&itable.lock);
  ip->ref++;
  pthread_spin_unlock(&itable.lock);
  return ip;
}

// Lock the given inode
// Read the inode from disk if necessary
void ilock(struct inode* ip) {
  if (ip == 0 || ip->ref < 1) {
    err_exit("ilock got invalid inode");
  }

  pthread_mutex_lock(&ip->lock);

  if (!ip->valid) {
    // read from disk
    struct bcache_buf* bp = logged_read(IBLOCK(ip->inum));
    struct dinode* dip    = (struct dinode*)bp->data + ip->inum % IPB;

    ip->type  = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size  = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    logged_relse(bp);
    ip->valid = 1;
    if (ip->type == T_UNUSE) {
      err_exit("ilock: ip is unused");
    }
  }
}

void iunlock(struct inode* ip) {
  // pthread_mutex_trylock: return 0 if not locked
  if (ip == 0 || !pthread_mutex_trylock(&ip->lock) || ip->ref < 1) {
    err_exit("iunlock: got invalid inode");
  }

  pthread_mutex_unlock(&ip->lock);
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk.
// Caller must hold ip->lock.
void iupdate(struct inode* ip) {
  struct bcache_buf* bp;
  struct dinode* dip;

  bp         = logged_read(IBLOCK(ip->inum));
  dip        = (struct dinode*)bp->data + ip->inum % IPB;
  dip->type  = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size  = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  logged_write(bp);
  logged_relse(bp);
}
void itrunc(struct inode* ip);

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode table entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void iput(struct inode* ip) {
  pthread_spin_lock(&itable.lock);

  if (ip->ref == 1 && ip->valid && ip->nlink == 0) {
    // truncate and free

    pthread_mutex_lock(&ip->lock);

    pthread_spin_unlock(&itable.lock);

    itrunc(ip);
    ip->type = T_UNUSE;
    iupdate(ip);
    ip->valid = 0;
    pthread_mutex_unlock(&ip->lock);
    pthread_spin_lock(&itable.lock);
  }
  ip->ref--;
  pthread_spin_unlock(&itable.lock);
}

void iunlockput(struct inode* ip) {
  iunlock(ip);
  iput(ip);
}

static void zero_a_block(uint bno) {
  static const char zeros[BSIZE] = {0};
  struct bcache_buf* bp          = logged_read(bno);
  memmove(bp->data, zeros, BSIZE);
  logged_write(bp);
  logged_relse(bp);
}

// simple block allocator
// Allocate a zeroed block on disk
static uint block_alloc() {
  int b, bi, m;
  struct bcache_buf* bp = 0;

  static uint last_alloced = 0;
  uint nmeta               = MYFUSE_STATE->sb.size - MYFUSE_STATE->sb.nblocks;
  pthread_spin_lock(&block_alloc_lock);
  if (last_alloced == 0) {
    last_alloced = nmeta;
  }
  pthread_spin_unlock(&block_alloc_lock);
  for (b = 0; b < MYFUSE_STATE->sb.size; b += BPB) {
    pthread_spin_lock(&block_alloc_lock);
    if (b + BPB < last_alloced) {
      pthread_spin_unlock(&block_alloc_lock);
      continue;
    }
    pthread_spin_unlock(&block_alloc_lock);

    bp = logged_read(BBLOCK(b));
    for (bi = 0; bi < BPB && b + bi < MYFUSE_STATE->sb.size; bi++) {
      pthread_spin_lock(&block_alloc_lock);
      if (b + bi < last_alloced) {
        pthread_spin_unlock(&block_alloc_lock);
        continue;
      }
      pthread_spin_unlock(&block_alloc_lock);

      m = 1 << (bi % 8);
      if ((bp->data[bi / 8] & m) == 0) {  // Is block free?
        bp->data[bi / 8] |= m;            // Mark block in use.
        logged_write(bp);
        logged_relse(bp);
        zero_a_block(b + bi);

        pthread_spin_lock(&block_alloc_lock);
        last_alloced = b + bi;
        pthread_spin_unlock(&block_alloc_lock);

        return b + bi;
      }
    }
    brelse(bp);
  }
  pthread_spin_lock(&block_alloc_lock);
  last_alloced = nmeta;
  pthread_spin_unlock(&block_alloc_lock);
  return block_alloc();
}

static void block_free(uint b) {
  struct bcache_buf* bp;
  int bi, m;

  bp = logged_read(BBLOCK(b));
  bi = b % BPB;
  m  = 1 << (bi % 8);
  if ((bp->data[bi / 8] & m) == 0) {
    err_exit("freeing free block");
  }
  bp->data[bi / 8] &= ~m;
  logged_write(bp);
  logged_relse(bp);
}

static inline void assert_no_need_to_restart_op_in_imap2blockno() {
  if (n_log_wrote >= MAXOPBLOCKS - 1 - 3) {
    err_exit("too many blocks in one op");
  }
}

// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT]...
// there is total 3 indirect blocks

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
uint imap2blockno(struct inode* ip, uint bn) {
  assert_no_need_to_restart_op_in_imap2blockno();

  uint addr, *a;
  struct bcache_buf* bp;

  if (bn < NDIRECT) {
    if ((addr = ip->addrs[bn]) == 0) {
      ip->addrs[bn] = addr = block_alloc();
    }
    return addr;
  }
  bn -= NDIRECT;

  // 1st indirect block
  // [d d .. d i i i]
  //           |
  //           +---> [d d .. d]
  if (bn < NINDIRECT1) {
    // Load indirect block, allocating if necessary.
    if ((addr = ip->addrs[NDIRECT]) == 0) {
      ip->addrs[NDIRECT] = addr = block_alloc();
    }
    bp = logged_read(addr);
    a  = (uint*)bp->data;
    if ((addr = a[bn]) == 0) {
      a[bn] = addr = block_alloc();
      logged_write(bp);
    }
    logged_relse(bp);
    return addr;
  }
  bn -= NINDIRECT1;

  // 2st indirect block
  // [d d .. d i i i]
  //             |
  //             +---> [i i .. i]
  //                    | |
  //     [d d .. d] <---+ +---> [d d .. d]
  if (bn < NINDIRECT2) {
    uint entry  = bn / NINDIRECT1;
    uint offset = bn % NINDIRECT1;
    // Load indirect entry, allocating if necessary
    if ((addr = ip->addrs[NDIRECT + 1]) == 0) {
      ip->addrs[NDIRECT + 1] = addr = block_alloc();
    }
    bp = logged_read(addr);
    a  = (uint*)bp->data;
    assert(entry * sizeof(uint) < BSIZE);
    if ((addr = a[entry]) == 0) {
      a[entry] = addr = block_alloc();
      logged_write(bp);
    }
    logged_relse(bp);

    bp = logged_read(addr);
    a  = (uint*)bp->data;
    if ((addr = a[offset]) == 0) {
      a[offset] = addr = block_alloc();
      logged_write(bp);
    }
    logged_relse(bp);
    return addr;
  }
  bn -= NINDIRECT2;

  // 3st indirect block
  // [d d .. d i i i]
  //             |
  //             +---> [i i .. i]
  //                    | |
  //     [i i .. i] <---+ +---> [i i .. i]
  //      |
  //      +---> [d d .. d]
  if (bn < NINDIRECT3) {
    uint entryl1  = bn / NINDIRECT2;
    uint offsetl1 = bn % NINDIRECT2;
    uint entryl2  = offsetl1 / NINDIRECT1;
    uint offsetl2 = offsetl1 % NINDIRECT1;
    // Load indirect entry, allocating if necessary
    if ((addr = ip->addrs[NDIRECT + 2]) == 0) {
      ip->addrs[NDIRECT + 2] = addr = block_alloc();
    }
    bp = logged_read(addr);
    a  = (uint*)bp->data;
    assert(entryl1 * sizeof(uint) < BSIZE);
    if ((addr = a[entryl1]) == 0) {
      a[entryl1] = addr = block_alloc();
      logged_write(bp);
    }
    logged_relse(bp);

    bp = logged_read(addr);
    a  = (uint*)bp->data;
    assert(entryl2 * sizeof(uint) < BSIZE);
    if ((addr = a[entryl2]) == 0) {
      a[entryl2] = addr = block_alloc();
      logged_write(bp);
    }
    logged_relse(bp);

    bp = logged_read(addr);
    a  = (uint*)bp->data;
    if ((addr = a[offsetl2]) == 0) {
      a[offsetl2] = addr = block_alloc();
      logged_write(bp);
    }
    logged_relse(bp);
    return addr;
  }

  err_exit("imap2blockno: inode too big");
  return -1;
}

void itrunc(struct inode* ip) {
  for (int i = 0; i < NDIRECT; i++) {
    if (ip->addrs[i]) {
      block_free(ip->addrs[i]);
    }
  }

  if (ip->addrs[NDIRECT]) {
    struct bcache_buf* bp = logged_read(ip->addrs[NDIRECT]);
    uint* a               = (uint*)bp->data;
    for (int i = 0; i < NINDIRECT1; i++) {
      if (a[i]) {
        block_free(a[i]);
      }
    }
    logged_relse(bp);
    block_free(ip->addrs[NDIRECT]);
  }

  if (ip->addrs[NDIRECT + 1]) {
    struct bcache_buf* bp = logged_read(ip->addrs[NDIRECT + 1]);
    uint* a               = (uint*)bp->data;
    for (int i = 0; i < NINDIRECT1; i++) {
      if (a[i]) {
        struct bcache_buf* bp2 = logged_read(a[i]);
        uint* a2               = (uint*)bp2->data;
        for (int j = 0; j < NINDIRECT1; j++) {
          if (a2[j]) {
            block_free(a2[j]);
          }
        }
        logged_relse(bp2);
        block_free(a[i]);
      }
    }
    logged_relse(bp);
    block_free(ip->addrs[NDIRECT + 1]);
  }

  if (ip->addrs[NDIRECT + 2]) {
    struct bcache_buf* bp = logged_read(ip->addrs[NDIRECT + 2]);
    uint* a               = (uint*)bp->data;
    for (int i = 0; i < NINDIRECT1; i++) {
      if (a[i]) {
        struct bcache_buf* bp2 = logged_read(a[i]);
        uint* a2               = (uint*)bp2->data;
        for (int j = 0; j < NINDIRECT1; j++) {
          if (a2[j]) {
            struct bcache_buf* bp3 = logged_read(a2[j]);
            uint* a3               = (uint*)bp3->data;
            for (int k = 0; k < NINDIRECT1; k++) {
              if (a3[k]) {
                block_free(a3[k]);
              }
            }
            logged_relse(bp3);
            block_free(a2[j]);
          }
        }
        logged_relse(bp2);
        block_free(a[i]);
      }
    }
    logged_relse(bp);
    block_free(ip->addrs[NDIRECT + 2]);
  }

  // TODO: this is unneccessary?
  memset(ip->addrs, 0, sizeof(ip->addrs));
}

static size_t min(size_t a, size_t b) { return a < b ? a : b; }

static void restart_op_on(struct inode* ip, uint nwrote) {
  if (n_log_wrote >= nwrote) {
    iunlock(ip);
    end_op();
    begin_op();
    ilock(ip);
  }
}

void inode_write_nbytes(struct inode* ip, const char* data, size_t nbytes,
                        size_t off) {
  begin_op();
  ilock(ip);
  uint inode_block_start = ((size_t)(off / BSIZE));
  size_t from_start      = off % BSIZE;
  size_t n_left          = BSIZE - from_start;
  restart_op_on(ip, MAXOPBLOCKS - 1 - 3 - 1);
  struct bcache_buf* bp = logged_read(imap2blockno(ip, inode_block_start));
  memmove(bp->data + from_start, data, min(n_left, nbytes));
  logged_write(bp);
  logged_relse(bp);
  if (nbytes <= n_left) {
    iunlock(ip);
    end_op();
    return;
  }
  nbytes -= n_left;
  data += n_left;

  // start block write
  uint inode_blockno = inode_block_start + 1;
  for (; nbytes > BSIZE; nbytes -= BSIZE) {
    // 3 is the max imap2blockno will write
    // 1 is the followed write
    restart_op_on(ip, MAXOPBLOCKS - 1 - 3 - 1);

    bp = logged_read(imap2blockno(ip, inode_blockno));
    memmove(bp->data, data, BSIZE);
    logged_write(bp);
    logged_relse(bp);

    data += BSIZE;
    inode_blockno++;
  }

  // 3 is the max imap2blockno will write
  // 1 is the followed write
  restart_op_on(ip, MAXOPBLOCKS - 3 - 1);

  // write last block
  if (nbytes) {
    bp = logged_read(imap2blockno(ip, inode_blockno));
    memmove(bp->data, data, nbytes);
    logged_write(bp);
    logged_relse(bp);
  }
  iunlock(ip);
  end_op();
}

void inode_read_nbytes(struct inode* ip, char* data, size_t nbytes,
                       size_t off) {
  // read op won't cause log, so just begin and end
  begin_op();
  ilock(ip);
  uint inode_block_start = ((size_t)(off / BSIZE));
  size_t from_start      = off % BSIZE;
  size_t n_left          = BSIZE - from_start;
  struct bcache_buf* bp  = logged_read(imap2blockno(ip, inode_block_start));
  memmove(data, bp->data + from_start, min(n_left, nbytes));
  logged_relse(bp);
  if (nbytes <= n_left) {
    iunlock(ip);
    end_op();
    return;
  }
  nbytes -= n_left;
  data += n_left;

  // start block write
  uint inode_blockno = inode_block_start + 1;
  for (; nbytes > BSIZE; nbytes -= BSIZE) {
    // 3 is the max imap2blockno will write
    restart_op_on(ip, MAXOPBLOCKS - 1 - 3);

    bp = logged_read(imap2blockno(ip, inode_blockno));
    memmove(data, bp->data, BSIZE);
    logged_relse(bp);
    data += BSIZE;
    inode_blockno++;
  }

  // write last block
  if (nbytes) {
    bp = logged_read(imap2blockno(ip, inode_blockno));
    memmove(data, bp->data, nbytes);
    logged_relse(bp);
  }
  iunlock(ip);
  end_op();
}
