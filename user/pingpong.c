#include "kernel/types.h"
#include "user/user.h"

int main(){
    int p1[2];
    int p2[2];
    pipe(p1); //子写父读
    pipe(p2); //子读父写
    if(fork() == 0){
        close(p2[1]);
        char buf[10];
        read(p2[0], buf, 4);
        fprintf(1, "%d: received %s\n", getpid(), buf);
        close(p2[0]);

        close(p1[0]);
        write(p1[1], "pong", 4);
        close(p1[1]);
        exit(0);
    }else{
        close(p2[0]);
        write(p2[1], "ping", 4);
        close(p2[1]);
        
        close(p1[1]);
        char buf[10];
        read(p1[0], buf, 4);
        fprintf(1, "%d: received %s\n", getpid(), buf);
        close(p1[0]);
        exit(0);
    }
}
