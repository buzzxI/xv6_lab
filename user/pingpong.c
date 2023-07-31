#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"

int main(int argc, char *argv[]){
   int child_2_parent[2];
    int parent_2_child[2];
    int rst;
    rst = pipe(child_2_parent);

    if (rst < 0) {
        fprintf(2, "pipe error\n");
        exit(1);
    }

    rst = pipe(parent_2_child);
    
    if (rst < 0) {
        fprintf(2, "pipe error\n");
        exit(1);
    }

    int pid;
    char byte;

    pid = fork();
    if(pid == 0){
        // child process
        pid = getpid();
        if ((rst = read(parent_2_child[0], &byte, 1))) fprintf(1, "%d: received ping\n", pid);
        write(child_2_parent[1], &byte, 1);
    } else {
        // parent process
        pid = getpid();
        write(parent_2_child[1], &byte, 1);
        if((rst = read(child_2_parent[0], &byte, 1))) fprintf(1,"%d: received pong\n", pid);
    }
    close(child_2_parent[0]);
    close(child_2_parent[1]);
    close(parent_2_child[0]);
    close(parent_2_child[1]);
    exit(0);
}