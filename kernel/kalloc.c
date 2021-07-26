// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

int ref[PGROUNDUP(PHYSTOP) / PGSIZE];

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  for (int i = 0; i < PGROUNDUP(PHYSTOP) / PGSIZE; ++i)
    ref[i] = 1;
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
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

  r = (struct run*)pa;

  acquire(&kmem.lock);
  ref[(uint64)pa / PGSIZE]--;
  if (ref[(uint64)pa / PGSIZE] == 0) {
    // only free the page when ref count is 0 
    r->next = kmem.freelist;
    kmem.freelist = r;
  }
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  // if (ref[pa / PGSIZE] != 0) {
  //   printf("%d\n", ref[pa / PGSIZE]);
  //   panic("kalloc");
  // }
  ref[(uint64)r / PGSIZE] = 1;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

void
krefadd(void *pa)
{
  uint64 addr;
  if((char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("krefadd");
  
  addr = (uint64)pa; 
  addr = PGROUNDUP(addr); 

  acquire(&kmem.lock);
  ref[addr / PGSIZE]++; 
  release(&kmem.lock); 
}
