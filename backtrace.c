#include <csapp.h>
#define STACK_SIZE 32

int main(int argc, char** argv) {
    int fd = Open("./backtrace.txt", O_RDONLY, S_IRUSR);
    rio_t rio;
    Rio_readinitb(&rio, fd);
    char* args[STACK_SIZE] = {"/usr/bin/addr2line", "-e", "kernel/kernel"};
    char buff[MAXBUF];
    char* p = buff;
    ssize_t len = 0;
    int idx = 3;
    for (; idx < STACK_SIZE && (len = Rio_readlineb(&rio, p, MAXBUF)); idx++) {
        args[idx] = p;
        p += len;
        if (*(p - 1) == '\n') *(p - 1) = 0;
        else {
            *p = 0;
            p++;
        }
    } 
    args[idx] = NULL;
    Close(fd);
    for (int i = 3; i < idx; i++) {
        fprintf(stdout, "args[%d]:%s\n", i, args[i]);
    }
    pid_t pid;
    if ((pid = Fork()) == 0) {
        Execve(args[0], args, environ);
    }
    while (wait(NULL) != pid) ;
    return 0;
}