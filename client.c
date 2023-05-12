#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    char buf[630];
    char write_buf[] = "testing writing";
    int offset = 3000; /* TODO: try test something bigger than the limit */

    FILE *fp = fopen("time_with_clz.txt", "w");
    if (!fp) {
        printf("failed to open the file.\n");
        return 1;  // EXIT_FAILURE
    }

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        int sz = read(fd, buf, 630);
        if (sz)
            printf("returned message was truncated!\n");
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
        sz = write(fd, write_buf, strlen(write_buf));
        fprintf(fp, "%s\n", buf);
    }

    close(fd);
    return 0;
}
