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

struct mem_list {
  struct spinlock lock;
  struct run* freelist;
  uint64 mem_end;
};

struct {
  struct mem_list mem_list[NCPU];
} kmem;

// physical memory is split into chuncks
// each core owns a  chunck
void
kinit()
{
  uint64 pa_begin = PGROUNDUP((uint64)end);
  uint64 pa_end = PGROUNDDOWN(PHYSTOP);
  uint64 pages = (pa_end - pa_begin) / PGSIZE;
  uint64 per_pages = pages / NCPU;
  uint64 left = pages % NCPU;

  // initialize memory for cores
  uint64 mem_begin = pa_begin;
  char lock_name[8];
  for (int i = 0; i < NCPU; i++) {
    snprintf(lock_name, 8, "kmem_%d", i + 1);
    initlock(&kmem.mem_list[i].lock, lock_name);
    uint64 mem_end = mem_begin + PGSIZE * per_pages;
    if (i < left) mem_end += PGSIZE;  
    kmem.mem_list[i].mem_end = mem_end;
    freerange((char*)mem_begin, (char*)mem_end);
    mem_begin = mem_end;
  }
}

void
freerange(void *pa_start, void *pa_end)
{
  for(char* p = pa_start; p + PGSIZE <= (char*)pa_end; p += PGSIZE) kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  push_off();
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  r = (struct run*)pa;

  int idx = 0;
  for (; idx < NCPU && (uint64)pa > kmem.mem_list[idx].mem_end; idx++);

  acquire(&kmem.mem_list[idx].lock);
  r->next = kmem.mem_list[idx].freelist;
  kmem.mem_list[idx].freelist = r;
  release(&kmem.mem_list[idx].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{

  struct run *r = 0;
  push_off();
  int idx = cpuid();
  for (int i = 0; i < NCPU && !r; i++, idx++) {
    if (idx == NCPU) idx -= NCPU;
    acquire(&kmem.mem_list[idx].lock);
    r = kmem.mem_list[idx].freelist;
    if (r) {
      kmem.mem_list[idx].freelist = r->next;
    }
    release(&kmem.mem_list[idx].lock);
  }

  if (r) memset((char*)r, idx, PGSIZE); // fill with junk

  pop_off();
  return (void*)r;
}
