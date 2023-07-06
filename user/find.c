#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

#define stderr 2
#define stdout 1
#define MAXBUF 512


void find_routine(char* path, char* pattern);
void last_name(char* full_path, char* rst);

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <path> <pattern>\n", argv[0]);
        exit(1);
    }
    find_routine(argv[1], argv[2]);
    exit(0);
}

void find_routine(char* path, char* pattern) {

    char buffer[MAXBUF], *p;
    int fd;
    struct dirent de;
    struct stat st;
    // fprintf(stdout, "path: %s\n", path);
    if ((fd = open(path, 0)) < 0) {
        fprintf(stderr, "find: cannot open:%s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(stderr, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    
    switch (st.type)
    {
    case T_DEVICE:
    case T_FILE:
        last_name(path, buffer);
        if (strlen(buffer) == 0) {
            if (strcmp(path, pattern) == 0) fprintf(stdout, "%s\n", path);
        } else {
            if (strcmp(buffer, pattern) == 0) fprintf(stdout, "%s\n", path);
        }
        break;
    case T_DIR:
        strcpy(buffer, path);
        p = buffer + strlen(buffer);
        *p++ = '/';
        int i;
        // pass parent and current directory
        for (i = 0; i < 2; i++) read(fd, &de, sizeof(de));
        while ((read(fd, &de, sizeof(de))) == sizeof(de)) {
            if (de.inum == 0) continue;
            strcpy(p, de.name);
            find_routine(buffer, pattern);
        }
        break;
    }
    close(fd);
}

// get file last name
void last_name(char* full_path, char* rst) {
    char* p = full_path + strlen(full_path);
    while (*p != '/' && p >= full_path) p--;
    if (p <= full_path) *rst = 0;
    else {
        p++;
        strcpy(rst, p);
    }
}