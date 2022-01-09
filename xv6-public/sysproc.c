#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "stat.h"

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
sys_getprocesssize(void)
{
  return myproc()->sz;
}

int
sys_addtwonumbers(void)
{
  int num1;
  int num2;

  argint(0, &num1);
  argint(1, &num2);

  return num1 + num2;
}

int
sys_addFloat(void)
{
  float* num1;
  float* num2;

  argptr(0, (void*)&num1, sizeof(*num1));
  argptr(1, (void*)&num2, sizeof(*num2));

  *num1 = ((*num1 * 1.0)  + (*num2 * 1.0)) * 1.0;

  return 0;
}

int
sys_shutdown(void){
  outw(0xB004, 0x0|0x2000);
  outw(0x604, 0x0|0x2000);

  return 0;
}

float
sys_addMultiple(void){
  struct multipleNum* st;
  argptr (0 , (void*)&st ,sizeof(*st));
  float result = 0;

  for(int i = 0; i < st->sz; i++){
    result += st->numbers[i];
  }
  // st->result = result;

  return result;
}

char*
sys_substr(void){
  char *str;
  int start_idx , len;

  argint(1 , &start_idx);
  argint(2 , &len);
  argstr(0 , &str);

  char* s = &str[0];

  int k = 0;
  for(int i = start_idx ; i < start_idx+len ; i++){
    s[k++] = str[i];
  }
  s[k]='\0';
  return s;
}

int*
sys_sort(void){
  struct mystat *ct;
  argptr (0 , (void*)&ct ,sizeof(*ct));
  int n = ct->sz;
 
  int temp;
   
  for (int i = 0; i < n; i++)
  {
    for (int j = i + 1; j < n; j++)
    {
        if (ct->nums[i] > ct->nums[j])
        {
          temp = ct->nums[i];
          ct->nums[i] = ct->nums[j];
          ct->nums[j] = temp;
        }
    }
  }
  return ct->nums;
}

int
sys_getreadcount(void)
{
  return myproc()->readid;
}