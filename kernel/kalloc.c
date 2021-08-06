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
  int num;
} kmems[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; ++i) {
    char lockname[20];
    snprintf(lockname, 20, "kmem%d", i);
    initlock(&kmems[i].lock, lockname);
  }

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
  int cpu_id;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  cpu_id = cpuid();

  acquire(&kmems[cpu_id].lock);
  r->next = kmems[cpu_id].freelist;
  kmems[cpu_id].freelist = r;
  kmems[cpu_id].num++;
  release(&kmems[cpu_id].lock);

  pop_off();
}

void 
steal(int cpu_id)
{
  struct run *r, *nr;
  // we should hold the lock for cpu_id when call this function
  for (int i = 0; i < NCPU; ++i)
  {
    if (cpu_id == i)
      continue;
    
    acquire(&kmems[i].lock);
    if (kmems[i].num > 0) {
      int n = kmems[i].num / 2;
      // smallest num should be one
      if (n == 0)
        n = 1;
      while (n-- > 0) {
        r = kmems[i].freelist;
        nr = r->next;
        r->next = kmems[cpu_id].freelist;
        kmems[cpu_id].freelist = r; 
        kmems[cpu_id].num++;
        kmems[i].freelist = nr; 
        kmems[i].num--; 
      }
      release(&kmems[i].lock); 
      return;
    }
    release(&kmems[i].lock);
  }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int cpu_id;

  push_off();
  cpu_id = cpuid();

  acquire(&kmems[cpu_id].lock);
  if (kmems[cpu_id].num == 0)
    steal(cpu_id);
  r = kmems[cpu_id].freelist;
  if(r) {
    kmems[cpu_id].freelist = r->next;
    kmems[cpu_id].num--;
  }
  release(&kmems[cpu_id].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  
  pop_off();
  return (void*)r;
}
