#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

#define DRIVER_AUTHOR "Nakta"
#define DRIVER_DESC "hello test driver"

static int __init hello_init(void) {
    pr_info("hello: loaded\n");
    return 0;
}

static void __exit hello_exit(void) {
    pr_info("hello: unloaded\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
