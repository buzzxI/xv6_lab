#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

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
  backtrace(); 
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

// install user-level handler
// this function will always returns 0;
uint64 sys_sigalarm(void) {
  
  struct proc* p = myproc();

  // user handler is processing, no installaion allowed
  if (p->uhandler.valid == 2) return 1; 

  int ticks;
  uint64 handler;

  argint(0, &ticks);
  argaddr(1, &handler);

  
  if (ticks == 0) {
    p->uhandler.valid = 0;
    if (p->uhandler.frame_cache) {
      kfree(p->uhandler.frame_cache);
      p->uhandler.frame_cache = 0;
    }
    return 0;
  }

  p->uhandler.valid = 1;
  p->uhandler.ticks = ticks;
  p->uhandler.passed_ticks = 0;
  p->uhandler.handler = handler;
  if (!p->uhandler.frame_cache) {
    if ((p->uhandler.frame_cache = (struct trapframe*)kalloc()) == 0) {
      printf("scause %p\n", r_scause());
      printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
      panic("sys_sigalarm:no:room:for:trapframe:cache");
    }
  }
  return 0;
}

// restore trapframe for user
uint64 sys_sigreturn() {
  struct proc* p = myproc();
  p->uhandler.valid = 1;
  memmove(p->trapframe, p->uhandler.frame_cache, sizeof(struct trapframe));
  return p->trapframe->a0;
}

