#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "myfifo_ioctl.h"

int main(void)
{
    int fd = open("/dev/myfifo", O_RDWR);
    int count = -1;

    if (fd < 0) { perror("open"); return 1; }

    /* 清空後應該是 0 */
    assert(ioctl(fd, MYFIFO_RESET) == 0);
    assert(ioctl(fd, MYFIFO_GET_COUNT, &count) == 0);
    printf("after reset: %d\n", count);
    assert(count == 0);

    /* 寫入 5 bytes 後應該是 5 */
    assert(write(fd, "hello", 5) == 5);
    assert(ioctl(fd, MYFIFO_GET_COUNT, &count) == 0);
    printf("after write 5: %d\n", count);
    assert(count == 5);

    /* 讀走 3 bytes 後應該剩 2 */
    char buf[3];
    assert(read(fd, buf, 3) == 3);
    assert(ioctl(fd, MYFIFO_GET_COUNT, &count) == 0);
    printf("after read 3: %d\n", count);
    assert(count == 2);

    /* 未知命令應該回 ENOTTY */
    assert(ioctl(fd, _IO('X', 99)) < 0);

    close(fd);
    printf("all passed\n");
    return 0;
}