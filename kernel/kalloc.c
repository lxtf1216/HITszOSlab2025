// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void kfree4cpu(void *pa,int id);
void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem{
  struct spinlock lock;
  struct run *freelist;
} ;
static char kmem_name[16];
struct kmem kmems[NCPU];

void
kinit()
{
  for(int i = 0;i < NCPU ; ++i) {
    snprintf(kmem_name , sizeof(kmem_name),"kmem%02d",i);
    initlock(&(kmems[i].lock),kmem_name);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  int id = 0;
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    kfree4cpu(p,id);
    id = (id + 1) %NCPU;
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

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  int id = cpuid();
  struct kmem *cur_kmem = &(kmems[id]);
  acquire(&(cur_kmem->lock));
  r->next = cur_kmem->freelist;
  cur_kmem->freelist = r;
  release(&(cur_kmem->lock));
}

void kfree4cpu(void *pa,int id) {
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  struct kmem *cur_kmem = &(kmems[id]);
  acquire(&(cur_kmem->lock));
  r->next = cur_kmem->freelist;
  cur_kmem->freelist = r;
  release(&(cur_kmem->lock));
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{

  int id = cpuid();
  struct kmem *cur_kmem = &(kmems[id]);
  struct run *r;

  // alloc locally
  acquire(&(cur_kmem->lock));
  r = cur_kmem->freelist;

  if(r) {
    cur_kmem->freelist = r->next;
  }
  release(&(cur_kmem->lock));

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    return (void*)r;
  }

  //steal from other CPU
  for(int i = 1;i< NCPU ; ++i) {

    struct kmem *other_kmem = &(kmems[(id+i)%NCPU]);

    acquire(&(other_kmem->lock)) ;

    r=other_kmem->freelist;

    if(r) {
      other_kmem->freelist = r->next;
    }

    release(&(other_kmem->lock));

    if(r) {
      memset((char*)r, 5, PGSIZE); // fill with junk
      return (void*)r;
    }
  }

  return 0;
}
