#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int readint(int fd, int *num) {
    int rs = read(fd, num, 4);
    if (rs == 0) {
        return 0;
    } else if (rs < 4) {
        fprintf(2, "Read error\n");
        exit(1);
    }
    return rs;
}

void writeint(int fd, int num) {
    int ws = write(fd, &num, 4);
    if (ws < 4) {
        fprintf(2, "Write error\n");
        exit(1);
    }
}

void nextprocess(int *p, int init) {
    int np[2];
    int prime = init ? 2 : 0;
    int n = prime, status = 0, sum = 0;
    pipe(np);
    if (init) {
        // close p0 and print 2 when at the start
        printf("prime 2\n");
    }
    for(;;) {
        if (init) {
            ++n;
            if (n > 35) {
                close(np[1]);
                if (fork() == 0) {
                    nextprocess(np, 0);
                } else {
                    wait(&status);
                }
                exit(status);
            }
            if (n % prime != 0) {
                writeint(np[1], n);
            }
        } else {
            if (readint(p[0], &n) > 0) {
                sum++;
                if (prime == 0) {
                    prime = n;
                    printf("prime %d\n", prime);
                } else if (n % prime != 0) {
                    writeint(np[1], n);
                }
            } else {
                // finish
                close(p[0]);
                close(np[1]);
                if (sum == 0) {
                    fprintf(2, "Empty process\n");
                    close(np[0]);
                    exit(1);
                } else if (sum == 1) {
                    // only one num left, exit directly
                    close(np[0]);
                    exit(0);
                }
                if (fork() == 0) {
                    nextprocess(np, 0);
                } else {
                    wait(&status);
                }
                exit(status);
            }
        }
    } 
}  

int main(int argc, char *argv[]) {
    nextprocess(0, 1);
    // should never reaches here
    fprintf(2, "Reach to the end, which is not expected\n");
    exit(1);
}