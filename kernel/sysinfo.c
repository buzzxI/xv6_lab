#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "sysinfo.h"
#include "spinlock.h"
#include "proc.h"

uint64 sys_sysinfo(void) {
    uint64 param_add;
    // user pointer to struct sysinfo
    argaddr(0, &param_add);
    // kernel stack object
    struct sysinfo info;
    info.nproc = process_count();
    info.freemem = kmem_acquire();
    struct proc* p = myproc();
    if (copyout(p->pagetable, param_add, (char*)&info, sizeof(info)) < 0) return -1;
    return 0;
}
