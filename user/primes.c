#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"

#define UPPER_BOUND 35
#define stdout 1
#define stderr 2

// prime child process routine
static void prime_routine(int rd_fd);
static void Pipe(int pipefd[2]);

int main(int argc, char** argv) {
    int pipefd[2];

    int split = 0;
    int eof = 1;

    Pipe(pipefd);

    int pid;
    if ((pid = fork()) == 0) {
        // child process
        close(pipefd[1]); 

        prime_routine(pipefd[0]);
        close(pipefd[0]);
    } else {
        // parent process
        close(pipefd[0]);
        int i;
        for (i = 2; i <= UPPER_BOUND; i++) {
            write(pipefd[1], &split, 1);    
            write(pipefd[1], &i, 1);
        }
        write(pipefd[1], &eof, 1);
        close(pipefd[1]);
        int status;
        wait(&status);
    }
    exit(0);
}

static void prime_routine(int rd_fd) {
    char buffer;
    int split = 0, eof = 1;
    int p = -1, n = -1;
    int pipefd[2];
    while (1) {
        read(rd_fd, &buffer, 1);
        if (buffer == 0) continue;
        if (buffer == 1) break;
        if (p < 0) {
            p = buffer;
            fprintf(stdout, "prime %d\n", p);
            Pipe(pipefd);
            int pid;
            if ((pid = fork()) == 0) {
                close(pipefd[1]);
                prime_routine(pipefd[0]);
                close(pipefd[0]);
                exit(0);
            }
        } else {
            n = buffer;
            if (n % p != 0) {
                write(pipefd[1], &split, 1);
                write(pipefd[1], &n, 1);
            }
        }
    }
    write(pipefd[1], &eof, 1);
    int status;
    wait(&status);
}

static void Pipe(int pipefd[2]) {
    int rst = pipe(pipefd);
    if (rst < 0) {
        fprintf(stderr, "pipe error, rst: %d\n", rst);
        exit(1);
    }
}
