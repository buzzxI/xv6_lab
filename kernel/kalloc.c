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

struct pageinfo {
  uint64 ref_cnt;
  // struct pageinfo* next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  uint64 free_pages;
  uint64 start_addr;
  uint64 end_addr;
  struct pageinfo* pages;
} kmem;

// cal pages counts for pageinfo
static inline uint64 info_pages(uint64 page_cnt, uint64 info_size) {
  return (page_cnt * info_size + PGSIZE + info_size - 1) / (PGSIZE + info_size);
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

// the physical memory will be constructed with: available pages and page_info pages(to count each page refcount)
void
freerange(void *pa_start, void *pa_end)
{
  kmem.start_addr = PGROUNDUP((uint64)pa_start);
  kmem.end_addr = PGROUNDDOWN((uint64)pa_end);
  
  uint64 total_page = (kmem.end_addr - kmem.start_addr) / PGSIZE;
  uint64 info_page = info_pages(total_page, sizeof(struct pageinfo));
  kmem.pages = (struct pageinfo*)(kmem.end_addr - PGSIZE * info_page);
  kmem.free_pages = 0;

  for (uint64 p = kmem.start_addr, i = 0; p < (uint64)kmem.pages; p += PGSIZE, i++) {
    // compromise for kfree
    kmem.pages[i].ref_cnt = 1;
    kfree((char*)p);
  } 
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  acquire(&kmem.lock);
  struct run *r;
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  uint64 pa_idx = (PGROUNDDOWN((uint64)pa) - kmem.start_addr) / PGSIZE;
  // other process still occupy this phyiscal page
  kmem.pages[pa_idx].ref_cnt--;
  if (!kmem.pages[pa_idx].ref_cnt) {
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;
    r->next = kmem.freelist;
    kmem.freelist = r;
    kmem.free_pages++;
  }
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{

  acquire(&kmem.lock);
  
  struct run *r;
  r = kmem.freelist;
  if (r) {
    kmem.freelist = r->next;
    uint64 pa_idx = (PGROUNDDOWN((uint64)r) - kmem.start_addr) / PGSIZE;
    if (kmem.pages[pa_idx].ref_cnt != 0) panic("kalloc:allocate:page:with:nonzero:ref:count");
    kmem.pages[pa_idx].ref_cnt = 1; 
    memset((char*)r, 5, PGSIZE); // fill with junk
    kmem.free_pages--;
    if (kmem.free_pages == (uint64)-1) panic("kalloc:free:page:negative"); 
  } else {
    printf("kalloc:run:out:of:memory\n")
  }

  release(&kmem.lock);
  return (void*)r;
}

// returns pa reference count after increase
// returns -1 on pa is illegal
int kinc(uint64 pa) {
  if (pa < kmem.start_addr || pa >= kmem.end_addr) return -1;
  acquire(&kmem.lock);
  uint64 pa_idx = (PGROUNDDOWN(pa) - kmem.start_addr) / PGSIZE;
  if (!kmem.pages[pa_idx].ref_cnt) panic("kinc:original:page:ref:count:0");
  kmem.pages[pa_idx].ref_cnt++;
  release(&kmem.lock);
  return kmem.pages[pa_idx].ref_cnt;
}