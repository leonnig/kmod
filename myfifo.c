#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h> // copy_to/from_user
#include <linux/device.h> // class_create, device_create
#include <linux/mutex.h>
#include <linux/ioctl.h>
#include <linux/wait.h>

#define DEVICE_NAME "myfifo"
#define MAX_SIZE 1024
#define MYFIFO_RESET _IO('M', 1)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cool");
MODULE_DESCRIPTION("A simple FIFO Character Driver");

// Global variables
static char kernel_buffer[MAX_SIZE]; 
static int data_size = 0; //Numbers of data in buffer right now

static dev_t dev_num; //device number (major +minor)
static struct cdev my_cdev; 
static struct class *my_class;
static struct device *my_device;

DEFINE_MUTEX(mtx); // Initialize a mutex lock
DECLARE_WAIT_QUEUE_HEAD(wqh); // Inicialize a wait queue head

//--- File Operations ---

static int my_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "Myinfo: Device open\n");
    return 0;
}

static int my_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "Myinfo: Device closed\n");
    return 0;
}

static ssize_t my_read(struct file *file, char __user *user_buffer, size_t len, loff_t *offset)
{
    int bytes_to_read;
    int remaining_bytes;
    int ret;

    mutex_lock(&mtx);

    while (*offset >= data_size) {
        mutex_unlock(&mtx);
        ret = wait_event_interruptible(wqh, data_size > *offset);
        mutex_lock(&mtx);
        
        if (ret != 0) {
            mutex_unlock(&mtx);
            return -ERESTARTSYS; // Let system finish this reading request.
        }
    }

    remaining_bytes = data_size - *offset;
    if (len > remaining_bytes) {
        bytes_to_read = remaining_bytes;
    } else {
        bytes_to_read = len;
    }

    if (copy_to_user(user_buffer, kernel_buffer + *offset, bytes_to_read)) {
        mutex_unlock(&mtx);
        return -EFAULT;
    }
    
    *offset += bytes_to_read;

    mutex_unlock(&mtx);

    printk(KERN_INFO "MyFifo: Sent %d bytes to user\n", bytes_to_read);
    return bytes_to_read;
}

static ssize_t my_write(struct file *file, const char __user *user_buffer, size_t len, loff_t *offset) {
    int bytes_to_write;

    if (len > MAX_SIZE) {
        printk(KERN_INFO "MyFifo: Data too large!\n");
        bytes_to_write = MAX_SIZE;
    } else {
        bytes_to_write = len;
    }

    mutex_lock(&mtx);

    memset(kernel_buffer, 0, MAX_SIZE);

    if(copy_from_user(kernel_buffer, user_buffer, bytes_to_write)) {
        mutex_unlock(&mtx);
        return -EFAULT;
    }

    data_size = bytes_to_write;

    printk(KERN_INFO "MyFifo: Received %d bytes from user: %s\n", bytes_to_write, kernel_buffer);
    wake_up_interruptible(&wqh);
    mutex_unlock(&mtx);
    return bytes_to_write;
}

static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    switch(cmd) {
        case MYFIFO_RESET:
            printk(KERN_INFO "MyFifo: Resetting buffer...\n");
            mutex_lock(&mtx);
            data_size = 0;
            memset(kernel_buffer, 0, MAX_SIZE);
            mutex_unlock(&mtx);
            return 0;
        
        default:
            return -ENOTTY;
    }
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_release,
    .read = my_read,
    .write = my_write,
    .unlocked_ioctl = my_ioctl,
};

static int __init my_init(void) {
    if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0) {
        return -1;
    }
    printk(KERN_INFO "MyFifo: Registered with Major %d\n", MAJOR(dev_num));

    cdev_init(&my_cdev, &fops);
    if(cdev_add(&my_cdev, dev_num, 1) < 0) {
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    my_class = class_create("myfifo_class");
    if (IS_ERR(my_class)) {
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    my_device = device_create(my_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(my_device)) {
        class_destroy(my_class);
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    printk(KERN_INFO "MyFifo: Driver loaded successfully\n");
    return 0;
}

static void __exit my_exit(void) {
    device_destroy(my_class, dev_num);
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "MyFifo: Driver unloaded\n");
}

module_init(my_init);
module_exit(my_exit);