#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]){

    // Add 2 numbers
    /*if(argc < 3){
        printf(2, "Usage: demo num1 num2\n");
        exit();
    }

    int num1 = atoi(argv[1]);
    int num2 = atoi(argv[2]);

    for(int i = 0; i < strlen(argv[1]); i++){
        if(argv[1][i] >= '0' && argv[1][i] <= '9'){
            continue;
        }
        printf(2, "1st argument is not a number\n");
        exit();
    }

    for(int i = 0; i < strlen(argv[2]); i++){
        if(argv[2][i] >= '0' && argv[2][i] <= '9'){
            continue;
        }
        printf(2, "2nd argument is not a number\n");
        exit();
    }


    printf(1, "result: %d\n", addtwonumbers(num1, num2));
    exit();*/



    // add 2 float numbers
    // if(argc < 3){
    //     printf(2, "Usage: demo num1 num2\n");
    //     exit();
    // }

    float num1 = atof(argv[1]);
    printfloat(1, "num1: %f\n", num1);
    printfloat(1, "num2: %f\n", 34.89);
    printfloat(1, "num2: %f\n", 3.289);
    // float num2 = atof(argv[2]);

    // float num1 = 12.5;
    // int num2 = (int) num1;
    // printf(1, "num1: %d\n", num2);

    exit();

    /*int fd;
    int i;
    char buf[8192];
    int noOfBytes = 2000;

    i = read(fd, buf, noOfBytes);
    if(i == noOfBytes){
        printf(1, "read succeeded ok\n");
    } 
    else {
        printf(1, "read failed\n");
        exit();
    }
    exit();*/
}