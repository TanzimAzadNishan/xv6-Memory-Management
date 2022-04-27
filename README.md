# xv6-Memory-Management
Paging Framework & Memory management related functionalities are implemented.<br />


<br /><br />
## **Project Overview**

This project has enhanced the memory management system of xv6. The following tasks are implemented-
    1) Updating the process details viewer
    2) Developing a paging framework of xv6


<br /><br />
### **Process Details Viewer**
It is a tool that is used to test the current mamory usage of the system. The built-in ***^P*** command provides important informations on each of the process. However, it reveals no information regarding the current memory state of each process. This project has enhanced the capability of ***xv6's process details viewer*** so that it can show the memory state of each process. The memory state includes the mapping from virtual pages to physical pages (for all the used pages), and also information regarding the used page tables.

For each process, the following should be displayed:

```
Process information currently printed with ^P
Page tables associated with the process
Page mappings - <virtual page number → physical page number>
```


<br /><br />
### **Paging Framework**

An important feature lacking in xv6 is the ability to swap out pages to a backing store. That is, at each moment in time all processes are held within the physical memory.

Paging Framework takes care of this. It can take out pages and store them to disk. Also, the framework will retrieve pages back to the memory on demand. Each process is responsible for paging in and out its own pages. This framework consists of the following things-
    1) File Framework
    2) Storing pages in a swap file
    3) Retrieving pages from a swap file
    4) Page Replacement Algorithms


<br /><br />
### **File Framework**
It handles the swap file related operations such as ***createSwapFile, readFromSwapFile, writeToSwapFile, removeSwapFile***. All the four functions have a parameter will hold a pointer to a file that will hold the swapped memory. The files will be named ***"/.swap"*** where the id is the process id.


<br /><br />
### **Storing pages in a swap file**
In any given time, a process should have no more than **MAX_PSYC_PAGES(15)** pages in the physical memory. Also, a process will not be larger than **MAX_TOTAL_PAGES(30)** pages. Whenever a process exceeds the MAX_PSYC_PAGES limitation, it must select enough pages and move them to its dedicated file. It is assumed that any given user process will not require more than MAX_TOTAL_PAGES pages. To know which page is in the process' page file and where it is in that file (i.e., paging meta-data); a data structure has been maintained.


<br /><br />
### **Retrieving pages from a swap file**
When a page fault has occurred, a trap to the kernel has been made. Then the os will fetch the required page from the swap file. The os will allocate a new physical page, copy its data from the file, and map it back to the page table. After returning from the trap frame to user space, the process will retry executing the last failed command again (should not generate a page fault now).


<br /><br />
### **Page Replacement Algorithms**
There are many page replacement algorithms. In this project, **FIFO(First in First out)** & **NRU(Not recently used)** has been implemented.

<br /><br />
## **Run the Project**

First, clone the repository.<br />

### **Install Qemu on Ubuntu**
Step 1:
> sudo apt-get install qemu-system-i386
<br />
Step 2: Go to the ***xv6-memory-management*** folder. <br /><br />
Step 3: compile the project
> make

### **Run Qemu**
Compile and run the emulator qemu
> make qemu-nox

<br /><br />
## **Developers**
#### [Md. Tanzim Azad](https://github.com/TanzimAzadNishan)





























