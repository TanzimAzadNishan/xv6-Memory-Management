#include "types.h"
#include "stat.h"
#include "user.h"
#include "mmu.h"


void test2(){
    int sz = 4096 * 15;
    int* mem = (int*)malloc(sz);

    for(int i = 0; i < sz/4; i++){
        mem[i] = 13 * i;
    }

    free((void*) mem);
}


void test(int sz){
    int pages = PGROUNDUP(sz) / PGSIZE;
    int noOfRemPhyPages = MAX_PSYC_PAGES - pages;

    for(int i = 0; i < noOfRemPhyPages; i++){
        sbrk(PGSIZE);
    }

    printf(1, "after test\n");
    procState();

    // sbrk(PGSIZE);
    // printf(1, "after test 2.%d\n", 1);
    // procState();

    for(int i = 0; i < 10; i++){
        sbrk(PGSIZE);
        printf(1, "after test 2.%d\n", i);
        procState();
    }
}

int main(int argc, char *argv[]){
    printf(1, "starting...\n");

    // 1 for FIFO, 2 for NRU
    int sz = pageInfo(1);
    test(sz);

    // pageInfo(1);
    // test2();    

    fork();

    printf(1, "after fork\n");
    procState();

    //sleep(200);

    wait();


    exit();
}