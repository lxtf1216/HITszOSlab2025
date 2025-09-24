#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

extern struct proc proc[NPROC];

uint64 sys_exit(void) {
  int n;
  if (argint(0, &n) < 0) return -1;
  exit(n);
  return 0;  // not reached
}

uint64 sys_getpid(void) { return myproc()->pid; }

uint64 sys_fork(void) { return fork(); }

uint64 sys_wait(void) {
  uint64 p1;
  int p2;
  if (argaddr(0, &p1) < 0) return -1;
  if (argint(1, &p2) < 0) return -1;
  return wait(p1,p2);
}

uint64 sys_sbrk(void) {
  int addr;
  int n;

  if (argint(0, &n) < 0) return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0) return -1;
  return addr;
}

uint64 sys_sleep(void) {
  int n;
  uint ticks0;

  if (argint(0, &n) < 0) return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (myproc()->killed) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64 sys_kill(void) {
  int pid;

  if (argint(0, &pid) < 0) return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64 sys_uptime(void) {
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_rename(void) {
  char name[16];
  int len = argstr(0, name, MAXPATH);
  if (len < 0) {
    return -1;
  }
  struct proc *p = myproc();
  memmove(p->name, name, len);
  p->name[len] = '\0';
  return 0;
}

uint64 sys_yield(void) {
  struct proc *cur_proc = myproc();
  
  acquire(&cur_proc->lock);
  printf("Save the context of the process to the memory region from address %p to %p\n", &(cur_proc->context), (&(cur_proc->context)) + 1);
  printf("Current running process pid is %d and user pc is %p\n", cur_proc->pid,cur_proc->trapframe->epc );
  release(&cur_proc->lock);

  int i,start = cur_proc - proc;
  struct proc *p;
  for (i = 1; i <= NPROC; ++i) {
    p = &proc[(start + i) % NPROC];
    acquire(&p->lock);
    if (p->state == RUNNABLE) {
      printf("Next runnable process pid is %d and user pc is %p\n", p->pid, p->trapframe->epc);
      release(&p->lock);
      break;
    }
    release(&p->lock);      
  }
  yield();
  return 0;
}