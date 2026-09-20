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
#include <linux/poll.h>
#include "myfifo_ioctl.h"

#define DEVICE_NAME "myfifo"
#define MAX_SIZE 1024

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Cool");
MODULE_DESCRIPTION("A simple FIFO Character Driver");

// Global variables
static char kernel_buffer[MAX_SIZE]; 
static int data_size = 0; //Numbers of data in buffer right now
static int head = 0;
static int tail = 0;

static dev_t dev_num; //device number (major +minor)
static struct cdev my_cdev; 
static struct class *my_class;
static struct device *my_device;

DEFINE_MUTEX(mtx); // Initialize a mutex lock
DECLARE_WAIT_QUEUE_HEAD(wqh_r); // Inicialize a wait queue head(Reader)
DECLARE_WAIT_QUEUE_HEAD(wqh_w); // Inicialize a wait queue head(Writer)

//--- File Operations ---

static int my_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "Myfifo: Device open\n");
    return 0;
}

static int my_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "Myfifo: Device closed\n");
    return 0;
}

static ssize_t my_read(struct file *file, char __user *user_buffer, size_t len, loff_t *offset)
{
    int bytes_to_read;
    int remaining_bytes;
    int ret;

    if (len == 0)
        return 0;

    mutex_lock(&mtx);

    while (data_size == 0) {
        if (file->f_flags & O_NONBLOCK) {
            mutex_unlock(&mtx);
            return -EAGAIN; //try again
        }
        mutex_unlock(&mtx);
        ret = wait_event_interruptible(wqh_r, data_size > 0);
        mutex_lock(&mtx);
        
        if (ret != 0) {
            mutex_unlock(&mtx);
            return -ERESTARTSYS; // Let system finish this reading request.
        }
    }

    remaining_bytes = data_size;

    if (len > remaining_bytes) {
        bytes_to_read = remaining_bytes;
    } else {bytes_to_read = len;}

    if ((head + bytes_to_read - 1) > (MAX_SIZE - 1)) {
        if(copy_to_user(user_buffer, kernel_buffer + head, MAX_SIZE - head)){
            mutex_unlock(&mtx);
            return -EFAULT;
        }
        if(copy_to_user(user_buffer + MAX_SIZE - head, kernel_buffer, (head + bytes_to_read) % MAX_SIZE)){
            mutex_unlock(&mtx);
            return -EFAULT;
        }
        head = (head + bytes_to_read) % MAX_SIZE;
    } else {
        if(copy_to_user(user_buffer, kernel_buffer + head, bytes_to_read)){
            mutex_unlock(&mtx);
            return -EFAULT;
        }
        head = (head + bytes_to_read) % MAX_SIZE ;
    }

    data_size -= bytes_to_read;

    mutex_unlock(&mtx);

    printk(KERN_INFO "MyFifo: Sent %d bytes to user\n", bytes_to_read);
    wake_up_interruptible(&wqh_w);
    return bytes_to_read;
}

static ssize_t my_write(struct file *file, const char __user *user_buffer, size_t len, loff_t *offset) {
    int bytes_to_write;
    int space_available;
    int ret;

    if (len == 0)
        return 0;

    mutex_lock(&mtx);

    while (data_size == MAX_SIZE) {
        if (file->f_flags & O_NONBLOCK) {
            mutex_unlock(&mtx);
            return -EAGAIN; //try again
        }
        mutex_unlock(&mtx);
        ret = wait_event_interruptible(wqh_w, data_size < MAX_SIZE);
        mutex_lock(&mtx);

        if (ret != 0){
            mutex_unlock(&mtx);
            return -ERESTARTSYS;
        }
    }
        
    space_available = MAX_SIZE - data_size;

    if (len > space_available) {
        bytes_to_write = space_available;
        printk(KERN_INFO "MyFifo: partial write, %d of %zu bytes\n", bytes_to_write, len);
    } else {
        bytes_to_write = len;
    }

    if ((tail + bytes_to_write - 1) > (MAX_SIZE - 1)) {
        if(copy_from_user(kernel_buffer + tail, user_buffer, MAX_SIZE - tail)){
            mutex_unlock(&mtx);
            return -EFAULT;
        }
        if(copy_from_user(kernel_buffer, user_buffer + MAX_SIZE - tail, (tail + bytes_to_write) % MAX_SIZE)){
            mutex_unlock(&mtx);
            return -EFAULT;
        }
        tail = (tail + bytes_to_write) % MAX_SIZE;
    }  else {
        if(copy_from_user(kernel_buffer + tail, user_buffer, bytes_to_write)){
            mutex_unlock(&mtx);
            return -EFAULT;
        }
        tail = (tail + bytes_to_write) % MAX_SIZE;
    }

    data_size += bytes_to_write;

    mutex_unlock(&mtx);

    printk(KERN_INFO "MyFifo: Received %d bytes from user\n", bytes_to_write);
    wake_up_interruptible(&wqh_r);
    return bytes_to_write;
}

static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    switch(cmd) {
        case MYFIFO_RESET:
            printk(KERN_INFO "MyFifo: Resetting buffer...\n");
            mutex_lock(&mtx);
            data_size = 0;
            memset(kernel_buffer, 0, MAX_SIZE);
            head = 0;
            tail = 0;
            mutex_unlock(&mtx);
            wake_up_interruptible(&wqh_w);
            return 0;
        case MYFIFO_GET_COUNT:{
            int count; 
            mutex_lock(&mtx);
            count = data_size;
            mutex_unlock(&mtx);
            if(copy_to_user((int __user*) arg, &count, sizeof(count)))
                return -EFAULT;
            return 0;
        }
        
        default:
            return -ENOTTY;
    }
}

static __poll_t my_poll(struct file *file, poll_table *wait) {

    __poll_t mask = 0;
    poll_wait(file, &wqh_r, wait);
    poll_wait(file, &wqh_w, wait);

    mutex_lock(&mtx);
    if (data_size != MAX_SIZE)
        mask |= EPOLLOUT | EPOLLWRNORM;
    if (data_size != 0)
        mask |= EPOLLIN | EPOLLRDNORM;
    mutex_unlock(&mtx);

    return mask;

}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_release,
    .read = my_read,
    .write = my_write,
    .unlocked_ioctl = my_ioctl,
    .poll = my_poll,
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