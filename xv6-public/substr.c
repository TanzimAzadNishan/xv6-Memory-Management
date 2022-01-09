#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]){
    char *s = argv[1];
    int start_idx = atoi(argv[2]);
    int len = atoi(argv[3]);

    printf(1 , "%s\n" , substr(s , start_idx , len));

    exit();
}