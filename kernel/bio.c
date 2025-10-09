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
#define mod 13
struct buf_buc {
  struct spinlock lock;
  struct buf head;
};
struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf_buc buckets[mod];
} bcache;

uint hash(int blockno) {
  return blockno%mod;
}
static char buclock_name[16];
void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
  for(int i = 0 ;i < mod ;++i) {
    snprintf(buclock_name,sizeof(buclock_name),"bcache_bucket%02d",i);
    initlock(&bcache.buckets[i].lock, buclock_name);
    bcache.buckets[i].head.next = &(bcache.buckets[i].head);
    bcache.buckets[i].head.prev = &(bcache.buckets[i].head);
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock,"buffer");
     b->refcnt = 0;
     b->valid = 0;
     b->dev = -1;
     b->blockno = 0;
    struct buf_buc *buc = &(bcache.buckets[0]);
    //开局直接全部丢到0桶
    b->next = (buc->head).next;
    b->prev = &(buc->head);
    ((buc->head).next)->prev = b;
    (buc->head).next = b;
  }
}

// helper functions 
static void 
remove_from_bucket(struct buf *b) {
  b->next->prev = b->prev;
  b->prev->next = b->next;
  b->prev = b->next = 0;
}
static void 
insert_into_bucket(struct buf *b,int buc_id) {
  struct buf_buc *buc = &(bcache.buckets[buc_id]);
  b->next = buc->head.next;
  b->prev = &buc->head;
  buc->head.next->prev = b;
  buc->head.next = b;
}
// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int id = hash(blockno);
  struct buf_buc *buc = &(bcache.buckets[id]);
  acquire(&(buc->lock));

  // Is the block already cached?
  for(b = (buc->head).next; b != &(buc->head); b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&(buc->lock));
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&(buc->lock));

  acquire(&bcache.lock);
  for(b = bcache.buf; b < bcache.buf + NBUF; b++){
    if(b->refcnt == 0) {
      int oth_id = hash(b->blockno);
      int mn = oth_id < id ? oth_id : id;
      int mx = oth_id < id ? id : oth_id; 

      //NOTICE!!! avoid deadlock
      acquire(&bcache.buckets[mn].lock);
      if (mn != mx)
        acquire(&bcache.buckets[mx].lock);

      if(b->refcnt == 0){
        //maybe useless if
        if(b->next || b->prev){
          remove_from_bucket(b);
        }
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        insert_into_bucket(b, id);

        if (mn != mx)
        release(&bcache.buckets[mx].lock);
        release(&bcache.buckets[mn].lock);


        release(&bcache.lock);


        acquiresleep(&b->lock);
        return b;
      }

      if (mn != mx){
        release(&bcache.buckets[mx].lock);
        release(&bcache.buckets[mn].lock);
      }
    }
  }
  release(&bcache.lock);
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

  int buc_id = hash(b->blockno);
  struct buf_buc *buc = &(bcache.buckets[buc_id]);

  acquire(&(buc->lock));
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    if(b->next || b->prev){
      remove_from_bucket(b);
    }
      insert_into_bucket(b, buc_id);
  }
  release(&(buc->lock));
}

void
bpin(struct buf *b) {
  int buc_id = hash(b->blockno);
  acquire(&bcache.buckets[buc_id].lock);
  b->refcnt++;
  release(&bcache.buckets[buc_id].lock);
}

void
bunpin(struct buf *b) {
  int buc_id = hash(b->blockno);
  acquire(&bcache.buckets[buc_id].lock);
  b->refcnt--;
  release(&bcache.buckets[buc_id].lock);
}


