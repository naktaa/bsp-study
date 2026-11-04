#include <linux/errno.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "char_ioctl.h"

#define DEVICE_NAME "char_drv"
#define DRIVER_AUTHOR "Nakta"
#define DRIVER_DESC "Character device read and write practice"

static int major;
static char saved_data[64];
static size_t saved_length;
static DEFINE_MUTEX(data_lock);

static int char_open(struct inode *inode, struct file *filp)
{
	pr_info("char_drv: [OPEN] opened\n");
	return 0;
}

static int char_release(struct inode *inode, struct file *filp)
{
	pr_info("char_drv: [CLOSE] released\n");
	return 0;
}

static ssize_t char_write(struct file *filp, const char __user *buf,
			  size_t count, loff_t *f_pos)
{
	char data[64];

	if (count == 0)
		return 0;
	if (count >= sizeof(data))
		return -EMSGSIZE;

	/* 사용자 복사가 실패하면 기존 저장 내용은 유지한다. */
	if (copy_from_user(data, buf, count))
		return -EFAULT;

	data[count] = '\0';

	mutex_lock(&data_lock);

	memcpy(saved_data, data, count + 1);
	saved_length = count;

	/* 같은 열린 파일에서 새 메시지를 처음부터 읽도록 한다. */
	*f_pos = 0;

	pr_info("char_drv: [WRITE] received %zu bytes: %s\n", count, saved_data);

	mutex_unlock(&data_lock);

	return count;
}

static ssize_t char_read(struct file *filp, char __user *buf,
			 size_t count, loff_t *f_pos)
{
	ssize_t result = 0;

	if (*f_pos < 0)
		return -EINVAL;
	if (count == 0)
		return 0;

	mutex_lock(&data_lock);

	if (*f_pos >= (loff_t)saved_length)
		goto out;
	if (count > saved_length - *f_pos)
		count = saved_length - *f_pos;

	if (copy_to_user(buf, saved_data + *f_pos, count)) {
		result = -EFAULT;
		goto out;
	}

	*f_pos += count;
	pr_info("char_drv: [READ] sent %zu bytes\n", count);
	result = count;

out:
	mutex_unlock(&data_lock);

	return result;
}

static long char_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case CHAR_CLEAR:
		mutex_lock(&data_lock);

		memset(saved_data, 0, sizeof(saved_data));
		saved_length = 0;

		/* EOF 뒤의 재확인도 처음 위치에서 수행하도록 한다. */
		filp->f_pos = 0;

		pr_info("char_drv: [IOCTL] buffer cleared\n");

		mutex_unlock(&data_lock);
		return 0;
	default:
		return -ENOTTY;
	}
}

static const struct file_operations char_fops = {
	.owner = THIS_MODULE,
	.open = char_open,
	.release = char_release,
	.write = char_write,
	.read = char_read,
	.unlocked_ioctl = char_ioctl,
};

static int __init char_init(void)
{
	major = register_chrdev(0, DEVICE_NAME, &char_fops);
	if (major < 0) {
		pr_err("char_drv: [MODULE] registration failed: %d\n", major);
		return major;
	}

	pr_info("char_drv: [MODULE] loaded, major=%d\n", major);
	return 0;
}

static void __exit char_exit(void)
{
	unregister_chrdev(major, DEVICE_NAME);
	pr_info("char_drv: [MODULE] unloaded\n");
}

module_init(char_init);
module_exit(char_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
