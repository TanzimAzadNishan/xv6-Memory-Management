#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]){

    if(argc < 4){
        printf(2, "Usage: testb1 char num1 num2\n");
        exit();
    }

    char ch = argv[1][0];
    int num1 = atoi(argv[2]);
    int num2 = atoi(argv[3]);

    int multResult = 0;
    float divResult = 0;
    int modResult = 0;

    if(ch == 'p'){
        multResult = mult(num1, num2);
        printf(1, "multiply Result: %d\n", multResult);
    }
    else if(ch == 'd'){
        divResult = div(num1, num2);
        printfloat(1, "divide result: %f\n", divResult);
    }
    else if(ch == 'm'){
        modResult = mod(num1, num2);
        printf(1, "mod Result: %d\n", modResult);
    }
    else{
        printf(1, "Invalid ch\n");
        exit();
    }

    exit();
}