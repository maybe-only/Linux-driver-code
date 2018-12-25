#include <linux/module.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
 
#define GLOBAL_MAJOR 0
#define GLOBAL_MAXMEM 0x1000
#define DEV_NAME ("globalmem")
 
int globalmem_major = GLOBAL_MAJOR;
spinlock_t lock;
int open_count = 0;
 
struct globalmem_dev{
	struct cdev cdev;
	unsigned char mem[GLOBAL_MAXMEM];
};
 
struct globalmem_dev *globalmem_devp;
 
static int globalmem_open(struct inode *inode, struct file *filp)
{
	spin_lock(&lock);
	if(open_count){
		spin_unlock(&lock);
		return -EBUSY;
	}
	open_count++;
	spin_unlock(&lock);
	
	filp->private_data = globalmem_devp;
	return 0;
}
 
static int globalmem_release(struct inode *inode, struct file *filp)
{
	spinlock(&lock);
	open_count--;
	spin_unlock(&lock);
	return 0;
}
 
static ssize_t globalmem_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	
	struct globalmem_dev *dev = filp->private_data;
	
	if(p >= GLOBAL_MAXMEM)
		return 0;
	if(count + p > GLOBAL_MAXMEM)
		count = GLOBAL_MAXMEM - p;
	
	if(copy_to_user(buf, (void*)dev->mem, count))
	{
		return -EFAULT;
	}
	else
	{
		*ppos += count;
		ret = count;
	}
	
	return ret;
}
 
static ssize_t globalmem_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
	unsigned long p = *ppos;
	unsigned int count = size;
	int ret = 0;
	
	struct globalmem_dev *dev = filp->private_data;
	
	if(p > GLOBAL_MAXMEM)
		return 0;
	if(count + p > GLOBAL_MAXMEM)
		count = GLOBAL_MAXMEM - p;
		
	if(copy_from_user(dev->mem, buf, count))
	{
		return -EFAULT;
	}
	else
	{
		*ppos += count;
		ret = count;
	}
	return ret;
}
 
static const struct file_operations globalmem_ops = {
	.owner = THIS_MODULE,
	.open = globalmem_open,
	.read = globalmem_read,
	.write = globalmem_write,
	.release = globalmem_release,
};
 
static void globalmem_setup(struct globalmem_dev *dev, int index)
{
	int err, devno = MKDEV(globalmem_major, index);
	
	cdev_init(&dev->cdev, &globalmem_ops);
	dev->cdev.owner = THIS_MODULE;
	err = cdev_add(&dev->cdev, devno, 1);
	if(err)
		printk(KERN_ERR "### cdev_add fail \n");
}
 
static int __init globalmem_init(void)
{
	int res;
	spin_lock_init(&lock);
	dev_t devno = MKDEV(globalmem_major, 0);
	
	if(globalmem_major)
		res = register_chrdev_region(devno, 1, DEV_NAME);
	else
	{
		res = alloc_chrdev_region(&devno, 0, 1, DEV_NAME);
		globalmem_major = MAJOR(devno);
	}
	if(res < 0)
		return res;
		
	globalmem_devp = kmalloc(sizeof(struct globalmem_dev), GFP_KERNEL);
	if(!globalmem_devp)
	{
		res = -ENOMEM;
		goto fail_malloc;
	}
	
	memset(globalmem_devp, 0, sizeof(struct globalmem_dev));
	
	globalmem_setup(globalmem_devp, 0);
	return 0;
	
fail_malloc:
	unregister_chrdev_region(devno, 1);
	return res;
}
 
static void __exit globalmem_exit(void)
{
	cdev_del(&globalmem_devp->cdev);
	kfree(globalmem_devp);
	unregister_chrdev_region(MKDEV(globalmem_major, 0), 1);
}
 
MODULE_LICENSE("GPL");
module_init(globalmem_init);
module_exit(globalmem_exit);
