# xv6-Memory-Management
Paging Framework & Memory management related functionalities are implemented.


## **Project Overview**

This project has enhanced the memory management system of xv6. The following tasks are implemented-
    1) Updating the process details viewer
    2) Developing a paging framework of xv6


### **Process Details Viewer**
It is a tool that is used to test the current mamory usage of the system. The built-in ***^P*** command provides important informations on each of the process. However, it reveals no information regarding the current memory state of each process. This project has enhanced the capability of ***xv6's process details viewer*** so that it can show the memory state of each process. The memory state includes the mapping from virtual pages to physical pages (for all the used pages), and also information regarding the used page tables.

For each process, the following should be displayed:

```
Process information currently printed with ^P
Page tables associated with the process
Page mappings - <virtual page number → physical page number>
```


















