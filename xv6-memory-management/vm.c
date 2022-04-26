#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()


// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  //cprintf("loaduvm: %d\n", sz);

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  //cprintf("here %d %d, %d\n", myproc()->pid, oldsz, newsz);

  if(newsz >= KERNBASE){
    return 0;
  }

  if(newsz < oldsz){
    return oldsz;
  }

  /*------------------------- my changes starts -----------------------------*/

  // checking if the curproc is not init(1) or sh(2)
  int newNoOfPages = PGROUNDUP(newsz) / PGSIZE;
  if(myproc()->pid > 2 && newNoOfPages > MAX_TOTAL_PAGES){
    return 0;
  }

  /*------------------------- my changes ends -----------------------------*/

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){

     /*------------------------- my changes starts -----------------------------*/

    if(myproc() && myproc()->pid > 2 && myproc()->noOfPhysicalPages == MAX_PSYC_PAGES && 
          (a == PGROUNDUP(oldsz) ||(int) myproc()->physicalPages[myproc()->fifoHead] == 0)){

        goto skipAllocation;
    }

    /*------------------------- my changes ends -----------------------------*/

    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }

    /*------------------------- my changes starts -----------------------------*/

skipAllocation:
    // checking if the curproc is not init(1) or sh(2)
    if (myproc() && myproc()->pid > 2){
      
      if(myproc()->noOfPhysicalPages < MAX_PSYC_PAGES){
          if(insertPageToPhysicalMemory(myproc(), a, false) == -1){
            cprintf("allocuvm: %d/n", a);
            goto skipAllocation;
          }
      }
	    
	    else {
        pageOutToSwapFile(myproc());
        if(a == PGROUNDUP(oldsz) || (myproc()->usedAlgorithm == FIFO && 
            (int) myproc()->physicalPages[myproc()->fifoHead - 1] == 0)){

          cprintf("continue\n");
          a -= 4096;
          continue;
        }

        insertPageToPhysicalMemory(myproc(), a, true);
      }

    }

    /*------------------------- my changes ends -----------------------------*/

  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;


  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      char *v = P2V(pa);
      kfree(v);

      /*------------------------- my changes starts -----------------------------*/

      // checking if the curproc is not init(1) or sh(2)
      if (myproc() && myproc()->pid > 2){
        
        for (int i = 0; i < MAX_PSYC_PAGES; i++){
          if(myproc()->physicalPages[i] == a){
            removePageFromPhysicalMemory(myproc(), i, false);
          }
        }
      }

      /*------------------------- my changes ends -----------------------------*/

      *pte = 0;
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, KERNBASE, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;
  
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");

    /*------------------------- my changes starts -----------------------------*/
    if (*pte & PTE_PG){
      // means the page is paged out. so continue
      // updatePteFlags(myproc(), i, -1, false);

      goto mapPte;
    }

    if(!(*pte & PTE_P)){
      cprintf("i=%d\n", i);
      //cprintf("U=%d\n", (*pte & PTE_U));

      panic("copyuvm: page not present");
    }

    /*------------------------- my changes ends -----------------------------*/ 

mapPte:
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }
  }
  return d;

bad:
  freevm(d);
  return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.



/*------------------------- my changes starts -----------------------------*/

int fifo_getIndexOfNewPhysicalPage(struct proc *p){
    if(p->noOfPhysicalPages == MAX_PSYC_PAGES){
      return - 1;
    }

    return p->fifoTail;
}


int insertPageToPhysicalMemory(struct proc *p, uint vAddr, bool isMemoryFull){
  int index = -1;

  if((isMemoryFull == true && p->usedAlgorithm == NRU) || p->nruIndex != -1){
    index = p->nruIndex;
  }
  else{
    index = fifo_getIndexOfNewPhysicalPage(p);
  }

  if(index == -1){
      cprintf("insert index -1 failed\n");
      return -1;
  }
  p->noOfPhysicalPages++;
  p->physicalPages[index] = vAddr;

  if(isMemoryFull == false || p->usedAlgorithm == FIFO){
    p->fifoTail = (p->fifoHead + p->noOfPhysicalPages) % MAX_PSYC_PAGES;

    if(p->noOfPhysicalPages == MAX_PSYC_PAGES && (int) p->physicalPages[p->fifoHead] == 0){

        p->fifoHead = (p->fifoHead + 1) % MAX_PSYC_PAGES;
        p->fifoTail = (p->fifoHead + p->noOfPhysicalPages) % MAX_PSYC_PAGES;
    }    
  }

  //if(p->usedAlgorithm == NRU){
    printProcPages(myproc());
  //}

  p->nruIndex = -1;

  return 0;
}

void removePageFromPhysicalMemory(struct proc *p, int index, bool isMemoryFull){
    p->noOfPhysicalPages--;
    p->physicalPages[index] = -2;

    //p->fifoTail = (p->fifoHead + p->noOfPhysicalPages) % MAX_PSYC_PAGES;

    if(p->usedAlgorithm == FIFO || isMemoryFull == false){
        p->fifoHead = (p->fifoHead + 1) % MAX_PSYC_PAGES;
    }
}


void updatePteFlags(struct proc* p, uint vAddr, uint pAddr, bool isPageout){
    pte_t *pte = walkpgdir(p->pgdir, (char*)vAddr, 0);

    if(pte){
      if(isPageout){
          *pte = *pte & ~PTE_P;
          *pte = *pte | PTE_PG;

          // clear the ppn of pte
          *pte = *pte & PTE_FLAGS(*pte);
      }

      else{
          *pte = *pte | (PTE_P | PTE_U | PTE_W);
          *pte = *pte & ~PTE_PG;

          // store physicalAddr as ppn
          *pte = *pte | pAddr;
      }

      //To refresh the TLB, refresh the rc3 register.
      lcr3(V2P(p->pgdir));
    } 
}

int getIndexOfPhysicalPage(struct proc *p, uint vAddr){
    for(int i = 0; i < MAX_PSYC_PAGES; i++){
        if(p->physicalPages[i] == vAddr){
            return i;
        }
    }
    return -1;
}


void pageOutToSwapFile(struct proc *p){
    int physicalPageIndex = -1;

calcIndex:
    if(p->usedAlgorithm == NRU){
        physicalPageIndex = nru_getIndexOfPageToBeSwappedOut(p);
    }
    else{
        physicalPageIndex = p->fifoHead;
    }

    cprintf("page out: index = %d, %d\n", physicalPageIndex, p->physicalPages[physicalPageIndex]);

    pte_t *pte = walkpgdir(p->pgdir, (char*)p->physicalPages[physicalPageIndex], 0);
    uint pAddr = PTE_ADDR(*pte);

    if(!(*pte & PTE_P) || !(*pte & PTE_U)){
        cprintf("pid=%d, present bit=%d, user bit=%d\n", p->pid, (*pte & PTE_P), (*pte & PTE_U));
        if(p->usedAlgorithm == FIFO){
            p->fifoHead = (p->fifoHead + 1) % MAX_PSYC_PAGES;
            p->fifoTail = (p->fifoHead + p->noOfPhysicalPages) % MAX_PSYC_PAGES;            
        
            goto calcIndex;
        }
    }

    // cprintf("present bit=%d, user bit=%d\n", (*pte & PTE_P), (*pte & PTE_U));

    // write the contents in swapfile and update swapFiles[i], physicalPages[i]
    int fetched = fetchPhysicalPageToSwapPage(p, physicalPageIndex, p->physicalPages[physicalPageIndex]);

    if(fetched == -1){
      cprintf("page out: Fetching failed\n");
    }

    // free physical memory
    char *va = (char*) P2V(pAddr);
    kfree(va);

    // update pte flags
    updatePteFlags(p, p->physicalPages[physicalPageIndex], -1, true);

    // remove physical pages
    removePageFromPhysicalMemory(p, physicalPageIndex, true);
}


bool pageInToPhysicalMemory(struct proc *p, uint vAddr){
    // page fault
    p->noOfPageFaults++;

    // get physical page index
    int physicalIndex = -1;

    if(p->usedAlgorithm == FIFO){
        physicalIndex = fifo_getIndexOfNewPhysicalPage(p);
    }
    else if(p->usedAlgorithm == NRU){
        physicalIndex = p->nruIndex;
    }

    cprintf("from trap: %d, %d\n", vAddr, physicalIndex);

    // fetch page from swap file and update swapFiles[i]
    char buffer[PGSIZE];
    int fetched = fetchSwapPageToPhysicalPage(p, physicalIndex, vAddr, buffer);

    if(fetched == -1){
      cprintf("Fetching failed\n");
    }

    // kalloc returns virtual address.
    char* newMemory = kalloc();

    // copy page contents to physical location
    memmove(newMemory, buffer, PGSIZE);  

    // update pte flags
    uint pAddr = V2P(newMemory);
    updatePteFlags(p, vAddr, pAddr, false);

    // insert page and update physicalPages[i]
    int isInserted = insertPageToPhysicalMemory(p, vAddr, true);
    if(isInserted == -1){
      cprintf("invalid: size = %d, va = %d\n", p->noOfPhysicalPages, vAddr);
      return false;
    } 

    return true;            
}


bool isPageWrittable(struct proc *p, void* vAddr){
    pte_t* pte = walkpgdir(p->pgdir, vAddr, 0);

    if(pte){
        if(*pte & PTE_W){
            return true;
        }
    }

    return false;
}

bool updateWritePermission(struct proc *p, void* vAddr){
    pte_t* pte = walkpgdir(p->pgdir, vAddr, 0);

    if(pte){
        *pte = *pte | PTE_W;
        return true;
    }
    return false;
}

bool isPageMovedToSwapFile(struct proc *p, void* vAddr){
    pte_t* pte = walkpgdir(p->pgdir, vAddr, 0);

    if(pte){
        if(*pte & PTE_PG){
            return true;
        }
    }

    return false;
}

int nru_getIndexOfPageToBeSwappedOut(struct proc *p){
    int index = -1;
    int priority = 3;    

    for(int i = 0; i < MAX_PSYC_PAGES; i++){
      uint vAddr = p->physicalPages[i];
      pte_t* pte = walkpgdir(p->pgdir, (char*)vAddr, 0);

      if(!(*pte & PTE_U)){
        continue;
      }

      int isModified = 0;
      int isReferenced = 0;

      if((int) (*pte & PTE_A) != 0){
        isReferenced = 1;
      }
      if((int) (*pte & PTE_D) != 0){
        isModified = 1;
      }

      if(isReferenced == 0 && isModified == 0){
          //cprintf("----------------- 0   0   0    0 ------------------------ \n");
          index = i;
          priority = 0;
          //cprintf("break from for loop %d\n", i);
          break;
      }
      else if(isReferenced == 0 && isModified == 1){
          if(priority > 1){
              //cprintf("----------------- 1    1   1   1   1 ------------------------ \n");
              index = i;
              priority = 1;
          }
      }
      else if(isReferenced == 1 && isModified == 0){
          if(priority > 2){
              //cprintf("----------------- 2   2   2    2 ------------------------ \n");
              index = i;
              priority = 2;
          }
      }
      else{
          if(priority == 3 && index == -1){
              //cprintf("----------------- 3   3   3    3 ------------------------ \n");
              index = i;
          }
      }
    }

    p->nruIndex = index;
    //cprintf("nru index=%d, priority=%d, va=%d\n", index, priority, p->physicalPages[index]);

    return index;
}

void printProcPages(struct proc *p){

    cprintf("\nphysicalPages:\t");
    for(int i = 0; i < MAX_PSYC_PAGES; i++){
      cprintf(" %d", p->physicalPages[i]);
    }
    cprintf("\n");
    cprintf("swapFilePages:\t");
    for(int i = 0; i < MAX_SWAPFILE_PAGES; i++){
      cprintf(" %d", p->swapFilePages[i]);
    }
    cprintf("\n");
    cprintf("pid=%d, sz=%d, name=%s, head=%d, tail=%d\n", p->pid, p->sz, p->name, p->fifoHead, p->fifoTail);
    cprintf("noOfPhysicalPages=%d, noOfSwapFilePages=%d, noOfPageFaults=%d\n", p->noOfPhysicalPages, p->noOfSwapFilePages, p->noOfPageFaults);
    cprintf("\n");
}

void resetAccessBit(struct proc *p){
    for(int i = 0; i < p->sz; i+= 4096){
        pte_t* pte = walkpgdir(p->pgdir, (char*)i, 0);

        if(pte){
            *pte = *pte & ~PTE_A;
        }
    }
}


/*------------------------- my changes ends -----------------------------*/
