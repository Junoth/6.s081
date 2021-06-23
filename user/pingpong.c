#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    char *buf[1];  
    int p1[2], p2[2];

    pipe(p1);
    pipe(p2);
    if (fork() == 0) {
        // child
        close(p1[1]);
        close(p2[0]);
        if (read(p1[0], buf, 1) < 1) {
            fprintf(2, "Read error\n");
            exit(1);
        }
        printf("%d: received ping\n", getpid()); 
        if (write(p2[1], buf, 1) < 1) {
            fprintf(2, "Write error\n");
            exit(1);
        }
    } else {
        // parent
        close(p1[0]);
        close(p2[1]);
        if (write(p1[1], buf, 1) < 1) {
            fprintf(2, "Write error\n");
            exit(1);
        }
        if (read(p2[0], buf, 1) < 1) {
            fprintf(2, "Read error\n");
            exit(1);
        }
        printf("%d: received pong\n", getpid());
    }

    exit(0);
}