#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

static int hello2_data __initdata = 3;

static int __init hello2_init(void)
{
	pr_info("Hello, world %d\n", hello2_data);
	return 0;
}

static void __exit hello2_exit(void)
{
	pr_info("Goodbyr, world 3\n");
}

module_init(hello2_init);
module_exit(hello2_exit);

MODULE_LICENSE("GPL");
