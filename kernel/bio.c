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

#define BCACHE_HASH(dev,no) (no % NBUF_BUCKET)


struct {
  struct buf buf[NBUF];
  struct{
    struct spinlock lock;
    struct buf head;
  }bucket[NBUF_BUCKET];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
  
} bcache;


void
binit(void)
{
  struct buf *b,*blast;
  static char tmp_buf[16*NBUF_BUCKET];//注意传到锁那边不会再做拷贝。
  int i;
  uint64 chunk;
  chunk = NBUF / NBUF_BUCKET;
  for(i=0;i<NBUF_BUCKET;i++){
    snprintf(tmp_buf+i*16,16,"bcache-%d",i);
    initlock(&bcache.bucket[i].lock, tmp_buf+i*16);
    // 给每个bucket平均分配
    blast =(i==NBUF_BUCKET - 1)? bcache.buf+NBUF : bcache.buf+(i+1)*chunk;
    b = bcache.buf + i*chunk;
    bcache.bucket[i].head.prev = &bcache.bucket[i].head;
    bcache.bucket[i].head.next = &bcache.bucket[i].head;
    for(;b!=blast;b++){
      // 插在最前面
      b->next = bcache.bucket[i].head.next;
      b->prev = &bcache.bucket[i].head;
      initsleeplock(&b->lock, "buffer");
      bcache.bucket[i].head.next->prev = b;
      bcache.bucket[i].head.next = b;
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int id,i;
  id = BCACHE_HASH(dev,blockno);
  acquire(&bcache.bucket[id].lock);
  // printf("get (%d %d)\n",dev,blockno);
  // Is the block already cached?
  for(b = bcache.bucket[id].head.next; b != &bcache.bucket[id].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket[id].lock);
      acquiresleep(&b->lock);
      return b; // cache
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b=bcache.bucket[id].head.prev; b!= &bcache.bucket[id].head;b = b->prev){
    if(b->refcnt == 0){
      b->dev = dev;
      b->blockno = blockno;
      b->refcnt = 1;
      release(&bcache.bucket[id].lock);
      acquiresleep(&b->lock);
      return b; // uncache
    }
  }
  // try to steal 
  for(i=0;i<NBUF_BUCKET;i++){
    if(i==id)continue;
    acquire(&bcache.bucket[i].lock);
    for(b=bcache.bucket[i].head.prev; b!= &bcache.bucket[i].head;b = b->prev){
      if(b->refcnt == 0){
        //stealing
        b->prev->next = b->next;
        b->next->prev = b->prev;
        release(&bcache.bucket[i].lock);
        b->prev = &bcache.bucket[id].head;
        b->next = bcache.bucket[id].head.next;
        bcache.bucket[id].head.next->prev = b;
        bcache.bucket[id].head.next = b;
        b->dev = dev;
        b->blockno = blockno;
        b->refcnt = 1;
        release(&bcache.bucket[id].lock);
        acquiresleep(&b->lock);
        return b; // uncache
      }
    }
    release(&bcache.bucket[i].lock);
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
  int id;
  if(!holdingsleep(&b->lock))
    panic("brelse");
  // printf("relse (%d,%d)\n",b->dev,b->blockno);

  id = BCACHE_HASH(b->dev,b->blockno);
  
  releasesleep(&b->lock);

  acquire(&bcache.bucket[id].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.bucket[id].head.next;
    b->prev = &bcache.bucket[id].head;
    bcache.bucket[id].head.next->prev = b;
    bcache.bucket[id].head.next = b;
    b->valid = 0;//不加这个会出现问题.
  }
  release(&bcache.bucket[id].lock);
}

void
bpin(struct buf *b) {
  int id;
  id = BCACHE_HASH(b->dev,b->blockno);
  acquire(&bcache.bucket[id].lock);
  b->refcnt++;
  release(&bcache.bucket[id].lock);
}

void
bunpin(struct buf *b) {
  int id;
  id = BCACHE_HASH(b->dev,b->blockno);
  acquire(&bcache.bucket[id].lock);
  b->refcnt--;
  release(&bcache.bucket[id].lock);
}


