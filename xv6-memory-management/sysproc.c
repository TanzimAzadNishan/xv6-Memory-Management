#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_pgdir(void){

  for(int k = 0; k < 1023; k++){
    if(myproc()->pgdir[k] & (PTE_U | PTE_P) /*&& !(myproc()->pgdir[k] & 1000000)*/){
      cprintf("%d, %d\n", k, myproc()->pgdir[k]);
    }
  }

  return 0;
}

/*------------------------- my changes starts -----------------------------*/

void
sys_procState(void){
  struct proc *p = myproc();

  cprintf("\n");
  cprintf("pid=%d, sz=%d, name=%s, head=%d, tail=%d\n", p->pid, p->sz, p->name, p->fifoHead, p->fifoTail);
  cprintf("noOfPhysicalPages=%d, noOfSwapFilePages=%d, noOfPageFaults=%d\n", p->noOfPhysicalPages, p->noOfSwapFilePages, p->noOfPageFaults);

  cprintf("\n");

  cprintf("physicalPages:\t");
  for(int i = 0; i < MAX_PSYC_PAGES; i++){
    cprintf(" %d", p->physicalPages[i]);
  }
  cprintf("\n");

  cprintf("swapFilePages:\t");
  for(int i = 0; i < MAX_SWAPFILE_PAGES; i++){
    cprintf(" %d", p->swapFilePages[i]);
  }
  cprintf("\n\n");

  //return p->sz;
}

int
sys_processSize(void){
  return myproc()->sz;
}

int
sys_pageInfo(void){
  struct proc *p = myproc();

  int num;
  argint(0, &num);

  p->noOfPhysicalPages = p->sz / 4096;

  uint va = 0;
  for(int i = 0; i < p->noOfPhysicalPages; i++){
    p->physicalPages[i] = va;
    va += PGSIZE;
  }
  for(int i = p->noOfPhysicalPages; i < MAX_PSYC_PAGES; i++){
      p->physicalPages[i] = -1;
  }
  for(int i = 0; i < MAX_SWAPFILE_PAGES; i++){
    p->swapFilePages[i] = -1;
  }
  p->fifoHead = 0;
  p->fifoTail = p->noOfPhysicalPages;

  if(num == 1){
    p->usedAlgorithm = FIFO;
  }
  else if(num == 2){
    p->usedAlgorithm = NRU;
  }
  p->nruIndex = -1;

  return p->sz;    
}


/*------------------------- my changes ends -----------------------------*/