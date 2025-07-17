#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
#include <linux/minmax.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define HAVE_PROC_OPS
#endif

#define PROCFS_MAX_SIZE 1024
#define PROCFS_NAME "buffer1k"

static struct proc_dir_entry *our_proc_file;

static char procfs_buffer[PROCFS_MAX_SIZE];

static unsigned long procfs_buffer_size = 0;

//呼叫此函數並讀取 /proc 檔案
static ssize_t procfs_read(struct file *filp, char __user *buffer,
                             size_t length, loff_t *offset)
{
    if (*offset || procfs_buffer_size == 0){
        pr_info("procfs_read: END\n");
        *offset = 0;
	return 0;
    }
    procfs_buffer_size = min(procfs_buffer_size, length);
    if (copy_to_user(buffer, procfs_buffer, procfs_buffer_size))
	    return -EFAULT;
    *offset += procfs_buffer_size;

    pr_info("procfs_read: read %lu bytes\n", procfs_buffer_size);
    return procfs_buffer_size;
}

static ssize_t procfs_write(struct file *file, const char __user *buffer,
			     size_t len, loff_t *off)
{
    procfs_buffer_size = min(PROCFS_MAX_SIZE, len);
    if (copy_from_user(procfs_buffer, buffer, procfs_buffer_size))
	    return -EFAULT;
    *off += procfs_buffer_size;

    pr_info("procfs_write: write %lu bytes\n", procfs_buffer_size);
    return procfs_buffer_size;
}

static int procfs_open(struct inode *inode, struct file *file)
{
    pr_info("procfs is opened");
    try_module_get(THIS_MODULE);
    return 0;
}

static int procfs_close(struct inode *inode, struct file *file)
{
    pr_info("procfs is closed");
    module_put(THIS_MODULE);
    return 0;
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops proc_file_fops = {
    .proc_read = procfs_read,
    .proc_write = procfs_write,
    .proc_open = procfs_open,
    .proc_release = procfs_close,
};
#else
static const struct file_operations proc_file_fops = {
    .proc_read = procfs_read,
    .proc_write = procfs_write,
    .proc_open = procfs_open,
    .proc_release = procfs_close,
};
#endif

static int __init procfs_init(void)
{
    our_proc_file = proc_create(PROCFS_NAME, 0644, NULL, &proc_file_fops);
    if (our_proc_file == NULL) {
        pr_alert("Error:Could not initialize /proc/%s\n", PROCFS_NAME);
        return -ENOMEM;
    }

    proc_set_size(our_proc_file, 80);
    proc_set_user(our_proc_file, GLOBAL_ROOT_UID, GLOBAL_ROOT_GID);

    pr_debug("/proc/%s created\n", PROCFS_NAME);
    return 0;
}

static void __exit procfs_exit(void)
{
    remove_proc_entry(PROCFS_NAME, NULL);
    pr_debug("/proc/%s removed\n", PROCFS_NAME);
}

module_init(procfs_init);
module_exit(procfs_exit);

MODULE_LICENSE("GPL");
