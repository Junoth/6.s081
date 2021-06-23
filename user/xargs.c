
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int i, last, len = 0, rc;
    char buf[MAXPATH];
    char *nargv[MAXARG];
    if(argc <= 1){
        fprintf(2, "usage: xargs command [args...] \n");
        exit(1);
    }
    for (i = 1; i < argc; ++i) {
        nargv[i - 1] = argv[i];
    }

    while ((rc = read(0, buf + len, MAXPATH)) > 0) {
        if (len + rc > MAXPATH) {
            fprintf(2, "xargs: arg too long");
            exit(2);
        }
        len += rc;
    }

    for (i = 0, last = 0; i < len; ++i) {
        if (buf[i] == '\n') {
            nargv[argc - 1] = buf + last;
            buf[i] = 0;
            // fork child and exec
            if (fork() == 0) {
                exec(nargv[0], nargv);
                fprintf(2, "xargs: child exec error\n");
                exit(1);
            } else {
                wait(0);
            }
            last = i + 1;
        }
    }
    
    exit(0);
}