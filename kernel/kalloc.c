// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(int cpuid,void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

struct {
  int is_steal;
  int from;
}steal[32768];

void
kinit()
{
  int i;
  static char tmp_buf[16*NCPU];//注意传到锁那边不会再做拷贝。
  uint64 total_sz,chunk,new_end,tmp;

  for(i=0;i<NCPU;i++){
    snprintf(tmp_buf+i*16,16,"kmem-%d",i);
    initlock(&kmem[i].lock, tmp_buf+i*16);
  }
  total_sz = (uint64)((char*)PHYSTOP - end + PGSIZE);
  chunk = total_sz / PGSIZE / NCPU;
  new_end = PHYSTOP;
  for(i=NCPU-1;i>=0;i--){
    if(i==0)freerange(i,end,(void*)new_end);
    else{
      tmp = new_end - PGSIZE * chunk + PGSIZE;
      freerange(i,(void*)tmp,(void*)new_end);
      new_end = tmp -PGSIZE;
    }
  }

}

void
freerange(int cpuid,void *pa_start, void *pa_end)
{
  char *p;
  struct run *r;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    if(((uint64)p % PGSIZE) != 0 || (char*)p < end || (uint64)p >= PHYSTOP)
      panic("kfree");

    // Fill with junk to catch dangling refs.
    memset(p, 1, PGSIZE);

    r = (struct run*)p;

    acquire(&kmem[cpuid].lock);
    r->next = kmem[cpuid].freelist;
    kmem[cpuid].freelist = r;
    release(&kmem[cpuid].lock);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int id;
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  if(steal[GET_STEAL_ADDR(r)].is_steal){
    // 如果是偷来的id
    id = steal[GET_STEAL_ADDR(r)].from;
  }else{
    push_off();//关中断，查看cpuid
    id = cpuid();
    pop_off();
  }
  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int id;

  push_off();//关中断，查看cpuid
  id = cpuid();
  pop_off();
  
  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if(r)
    kmem[id].freelist = r->next;
  release(&kmem[id].lock);

  if(!r){
    // 尝试去别的链上取
    for(int i =0;i<NCPU; i++){
        if(i!=id){
          acquire(&kmem[i].lock);
          r = kmem[i].freelist;
          if(r){
            steal[GET_STEAL_ADDR(r)].is_steal = 1;
            steal[GET_STEAL_ADDR(r)].from = i;
            kmem[i].freelist = r->next;
            release(&kmem[i].lock);
            break;
          }
          else{
            release(&kmem[i].lock);
          }
        }
      }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
