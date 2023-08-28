#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
// added for lab: mmap
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// added for lab: mmap
uint64 sys_mmap(void) {
  uint64 addr;

  int length, prot, flags, fd, offset;

  // the first arg is useless in this lab
  argaddr(0, &addr);

  // vma size
  argint(1, &length);
  // vma prot 
  argint(2, &prot);
  // vma flags
  argint(3, &flags);
  // map this area to file -> fd
  argint(4, &fd);
  // the last arg is useless in this lab (always zero)
  argint(5, &offset);

  struct proc *p = myproc();

  int idx = 0; 
  for (; idx < NVMA && p->vmas[idx].valid; idx++);

  // all virtual memory areas has been allocated
  if (idx == NVMA) return -1;
  
  struct file *file = p->ofile[fd];
  if (file == 0) return -1;

  // if area is shared then area access permission should be subset of opening file
  if (flags == MAP_SHARED) {
    if ((prot & PROT_WRITE) && !file->writable) return -1;
    if ((prot & PROT_READ) && !file->readable) return -1;
  }
  
  uint64 end_addr = MAXVA - (PGSIZE << 1);
  
  for (int i = 0; i < NVMA; i++) {
    if (!p->vmas[i].valid) continue;
    if (p->vmas[i].file_start < end_addr) end_addr = p->vmas[i].file_start; 
  }

  uint64 start_addr = end_addr - length;
  struct vma *vma = &p->vmas[idx];
  vma->valid = 1;
  vma->file_start = start_addr;
  vma->start = vma->file_start;
  vma->file_target = p->ofile[fd];
  vma->length = length;
  vma->prot = prot;
  vma->flags = flags;
  filedup(p->vmas[idx].file_target);

  return vma->start;
}

uint64 sys_munmap(void) {

  uint64 addr;
  int length;

  argaddr(0, &addr);
  argint(1, &length);

  struct proc *p = myproc();
  
  int idx = 0;
  for (; idx < NVMA; idx++) {
    if (!p->vmas[idx].valid) continue;
    uint64 start = p->vmas[idx].start;
    uint64 end = start + p->vmas[idx].length;
    if (addr >= start && addr < end) break;
  }
  if (idx == NVMA) return -1;

  struct vma *vma = &p->vmas[idx];
  if ((addr - vma->start) % PGSIZE) return -1;
  if (length % PGSIZE) return -1;
  if (addr + length > vma->start + vma->length) return -1;

  munmmap(addr, length, vma, p->pagetable);

  return 0;
}
