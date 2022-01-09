#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]){
    struct multipleNum *st = malloc (sizeof(struct multipleNum));
    st->sz = argc - 1;

    for(int i = 0; i < st->sz; i++){
        st->numbers[i] = atof(argv[i + 1]);
    }

    printfloat(1, "result: %f\n", (addMultiple(st)));

    exit();
}