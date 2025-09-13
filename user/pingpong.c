#include "kernel/types.h"
#include "user.h"

int main() {
    int f2c[2]; 
    int c2f[2]; 
    char buf[8];
    int ppid = getpid();
    pipe(f2c);
    pipe(c2f);

    int pid = fork();

    if (pid == 0) {
        close(f2c[1]); 
        close(c2f[0]); 

        read(f2c[0], buf, sizeof(buf));
        printf("%d: received %s from pid %d\n", getpid(),buf,ppid);

        write(c2f[1], "pong", 4);

        close(f2c[0]);
        close(c2f[1]);
        exit(0);
    } else {
        close(f2c[0]); 
        close(c2f[1]); 

        write(f2c[1], "ping", 4);
        read(c2f[0], buf, sizeof(buf));
        printf("%d: received %s from pid %d\n", getpid(), buf ,pid);

        close(f2c[1]);
        close(c2f[0]);
        wait(0);
    }

    exit(0);
}
