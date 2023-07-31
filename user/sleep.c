#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(2, "Usage: sleep [time]\n");
        exit(1);
    }

    int time = atoi(argv[1]);

    int rst = sleep(time);

    if (rst < 0) {
        fprintf(2, "sleep error\n");
        exit(1);
    }

    exit(0);
}