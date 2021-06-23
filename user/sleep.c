#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int time;
    
    if(argc <= 1){
        fprintf(2, "usage: sleep time \n");
        exit(1);
    }

    time = atoi(argv[1]);
    if (sleep(time) < 0) {
        exit(1);
    }
    exit(0);
}