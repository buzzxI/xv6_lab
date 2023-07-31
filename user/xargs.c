#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

#define stdin 0
#define stdout 1
#define stderr 2
#define MAXBUF 8192

int main(int argc, char** argv) {
    char buff[MAXBUF];
    char* args[MAXARG + 1];
    int i = 0;
    for (; i < argc - 1; i++) args[i] = argv[i + 1];
    args[i + 1] = 0;
    int j = 0;
    for (; read(stdin, &buff[j], 1); j++) {
        if (buff[j] == '\n') {
            buff[j] = 0;
            args[i] = buff;
            if (fork() == 0) exec(args[0], args);
            else {
                int status;
                wait(&status);
                j = -1;
            }
        }
    }
    exit(0);
}
