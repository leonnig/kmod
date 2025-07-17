#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h> /* for seq_file */
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define HAVE_PROC_OPS
#endif

#define PROC_NAME "iter"

/* This function is called at the beginning of a sequence. 
 * ie, when: 
 *   - the /proc file is read (first time) 
 *   - after the function stop (end of sequence) 
 */ 

static void *my_seq_start(struct seq_file *s, loff_t *pos)
{
    if (*pos == 0)
        return pos;
    
    return NULL; 
}

static void *my_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
    (*pos)++;
    if (*pos < 10){
        return pos;
    }
    return NULL;
}

static void my_seq_stop(struct seq_file *s, void *v)
{
    /* nothing to do */
}

static int my_seq_show(struct seq_file *s, void *v)
{
    loff_t *spos = (loff_t *)v;
    seq_printf(s, "%lld\n", *spos);
    return 0;
}

/* This structure gather "function" to manage the sequence */ 

static struct seq_operations my_seq_ops = { 

    .start = my_seq_start, 
    .next = my_seq_next, 
    .stop = my_seq_stop, 
    .show = my_seq_show, 
}; 

/* This function is called when the /proc file is open. */ 

static int my_open(struct inode *inode, struct file *file) 

{ 
    return seq_open(file, &my_seq_ops); 
}; 

/* This structure gather "function" that manage the /proc file */ 

#ifdef HAVE_PROC_OPS 
static const struct proc_ops my_file_ops = { 
    .proc_open = my_open, 
    .proc_read = seq_read, 
    .proc_lseek = seq_lseek, 
    .proc_release = seq_release, 
}; 

#else 
static const struct file_operations my_file_ops = { 
    .open = my_open, 
    .read = seq_read, 
    .llseek = seq_lseek, 
    .release = seq_release, 
}; 
#endif 

static int __init seq1_init(void)
{
    struct proc_dir_entry *entry;

    entry = proc_create(PROC_NAME, 0, NULL, &my_file_ops);
        if (entry == NULL) { 
        pr_debug("Error: Could not initialize /proc/%s\n", PROC_NAME); 
        return -ENOMEM; 
    } 

    return 0; 
}

static void __exit seq1_exit(void)
{
    remove_proc_entry(PROC_NAME, NULL);
    pr_info("/proc/%s removed\n", PROC_NAME); 
}

module_init(seq1_init);
module_exit(seq1_exit);

MODULE_LICENSE("GPL");