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

// bucket size is prime
#define BUCKET_SIZE 13

struct bucket {
  struct buf head;
  struct spinlock lock;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct bucket buckets[BUCKET_SIZE];
} bcache;



// simple hash for bucket index
static uint hash(uint key) {
  return key % BUCKET_SIZE;
}

// head <-> buf + 29 <-> buf + 28 <-> ... <-> buf + 1 <-> buf
// buf <-> head
// double linked list
void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  char lock_name[16];
  
  for (int i = 0; i < BUCKET_SIZE; i++) {
    snprintf(lock_name, 16, "bcache_%d", i);
    initlock(&bcache.buckets[i].lock, lock_name);
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
  }

  for (int i = 0; i < NBUF; i++) {
    b = &bcache.buf[i];
    int idx = hash(i);
    b->prev = &bcache.buckets[idx].head;
    b->next = bcache.buckets[idx].head.next;
    bcache.buckets[idx].head.next->prev = b;
    bcache.buckets[idx].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b = 0;

  int idx = hash(blockno);

  acquire(&bcache.buckets[idx].lock);
  
  // is buf cached ? 
  for (b = bcache.buckets[idx].head.next; b != &bcache.buckets[idx].head; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.buckets[idx].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // is bucket available ?
  for (b = bcache.buckets[idx].head.next; b != &bcache.buckets[idx].head; b = b->next) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.buckets[idx].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.buckets[idx].lock);

  // borrow from other bucket
  
  // acquire giant lock in case of deadlock
  acquire(&bcache.lock);
  
  for (int i = idx + 1; i != idx; i++) {
    if (i == BUCKET_SIZE) i -= BUCKET_SIZE;
    acquire(&bcache.buckets[i].lock);   
    for (b = bcache.buckets[i].head.next; b != &bcache.buckets[i].head; b = b->next) {
      if (b->refcnt == 0) {
          acquire(&bcache.buckets[idx].lock);
          b->prev->next = b->next;
          b->next->prev = b->prev;
          b->next = bcache.buckets[idx].head.next;
          b->prev = &bcache.buckets[idx].head;
          bcache.buckets[idx].head.next->prev = b;
          bcache.buckets[idx].head.next = b;
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;
          release(&bcache.buckets[i].lock);
          release(&bcache.buckets[idx].lock);
          break;
      }
    }
  }

  release(&bcache.lock);
  if (b) acquiresleep(&b->lock);
  else panic("bget: no buffers");
  return b;
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

  int idx = hash(b->blockno);

  acquire(&bcache.buckets[idx].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.buckets[idx].head.next;
    b->prev = &bcache.buckets[idx].head;
    bcache.buckets[idx].head.next->prev = b;
    bcache.buckets[idx].head.next = b;
  }
  release(&bcache.buckets[idx].lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


