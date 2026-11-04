#ifndef CHAR_IOCTL_H
#define CHAR_IOCTL_H

#include <linux/ioctl.h>

#define CHAR_IOC_MAGIC 'C'
#define CHAR_CLEAR _IO(CHAR_IOC_MAGIC, 1)

#endif
