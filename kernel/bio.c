// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define PRIME 13

struct bucket {
  struct buf head;
  struct spinlock lock;
}; 

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct bucket buckets[PRIME];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for (int i = 0; i < PRIME; ++i) {
    char lockname[20];
    snprintf(lockname, 20, "bcache_bucket_%d", i);
    initlock(&bcache.buckets[i].lock, lockname);
    b = &bcache.buckets[i].head;
    b->next = b;
    b->prev = b;
  }

  struct buf *head = &bcache.buckets[0].head;
  for (b = bcache.buf; b < bcache.buf+NBUF; b++) {
    b->next = head->next;
    b->prev = head;
    initsleeplock(&b->lock, "buffer");
    head->next->prev = b;
    head->next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint offset;

  offset = blockno % PRIME;
  struct buf *head = &bcache.buckets[offset].head;
  acquire(&bcache.buckets[offset].lock);
  for (b = head->next; b != head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.buckets[offset].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  struct buf *replace = 0;
  uint mintick = 0, noffset; 
  // make sure the lock order is bcache -> bucket
  release(&bcache.buckets[offset].lock);
  acquire(&bcache.lock);
  acquire(&bcache.buckets[offset].lock);

  // run check again to make sure the block is not in the cache
  for (b = head->next; b != head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.buckets[offset].lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  for (b = bcache.buf; b < bcache.buf+NBUF; b++) {
    if (b->refcnt == 0 && (mintick == 0 || b->timestamp < mintick)) {
      replace = b;
      mintick = b->timestamp;
    }
  }
  if (replace) {
    noffset = replace->blockno % PRIME;
    replace->dev = dev;
    replace->blockno = blockno;
    replace->valid = 0;
    replace->refcnt = 1;
    replace->prev->next = replace->next;
    replace->next->prev = replace->prev;
    // add to new bucket
    if (offset != noffset)
      acquire(&bcache.buckets[noffset].lock);
    replace->next = head->next;
    replace->prev = head;
    head->next->prev = replace;
    head->next = replace;
    if (noffset != offset)
      release(&bcache.buckets[noffset].lock);  
    release(&bcache.buckets[offset].lock);
    release(&bcache.lock);
    acquiresleep(&replace->lock);
    return replace;
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  uint offset = b->blockno % PRIME;
  acquire(&bcache.buckets[offset].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    b->timestamp = ticks;
  }
  release(&bcache.buckets[offset].lock);
}

void
bpin(struct buf *b) {
  uint offset = b->blockno % PRIME;
  acquire(&bcache.buckets[offset].lock);
  b->refcnt++;
  release(&bcache.buckets[offset].lock);
}

void
bunpin(struct buf *b) {
  uint offset = b->blockno % PRIME;
  acquire(&bcache.buckets[offset].lock);
  b->refcnt--;
  release(&bcache.buckets[offset].lock);
}


