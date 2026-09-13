#ifndef MYFIFO_IOCTL_H
#define MYFIFO_IOCTL_H

#include <linux/ioctl.h>

#define MYFIFO_MAGIC 'M'
#define MYFIFO_RESET _IO(MYFIFO_MAGIC, 1)
#define MYFIFO_GET_COUNT _IOR(MYFIFO_MAGIC, 2, int)

#endif
