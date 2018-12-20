/**
*Copyright (c) 2013.TianYuan
*All rights reserved.
*
*文件名称: char_device_driver10.c
*文件标识: 原子操作,针对设备节点打开的时候，不能再一次打开
*原子操作
*当前版本：1.0
*作者：wuyq 
*
*取代版本：xxx
*原作者：xxx
*完成日期：2013-11-29
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
 
#include <asm/gpio.h>
#include <plat/gpio-cfg.h>
 
 
 
MODULE_LICENSE("GPL");
#define CDD_MAJOR	200//cat /proc/devices找一个尚未使用的
#define CDD_MINOR	0
#define CDD_COUNT	1//一个cdev控制两个led
dev_t dev = 0;
u32 cdd_major = 0;
u32 cdd_minor = 0;
 
struct class *dev_class = NULL;
#define BUF_SIZE	100
struct cdd_cdev{
	struct cdev cdev;
	struct device *dev_device;
	u8 led;
	
	char kbuf[BUF_SIZE];
	
	u32 data_len;//记录缓冲区中已经写入数据的长度
	atomic_t av;//原子变量
	//int opentimes;//打开计数
};
 
struct cdd_cdev *cdd_cdevp = NULL;
 
unsigned long led_gpio_table[2] = {
	S5PV210_GPC1(3),//数字
	S5PV210_GPC1(4),
};
 
int cdd_open(struct inode* inode, struct file *filp)
{
	struct cdd_cdev *pcdevp = NULL;
	printk("enter cdd_open!\n");
 
	pcdevp = container_of(inode->i_cdev, struct cdd_cdev, cdev);
	printk("led = %d\n", pcdevp->led);
	//初值1 dec0
	if(!atomic_dec_and_test(&(pcdevp->av))){
		printk("cdev is opened ,not allowed open again!\n");
		atomic_inc(&(pcdevp->av));
		return -EBUSY;
	}
	/*
		if(!(pcdevp->opentimes-- == 1)){
			printk("cdev is already open!\n");
			pcdevp->opentimes++;
			return -EBUSY;
		}
		
	*/
	filp->private_data = pcdevp;
	
	return 0;
}
 
int cdd_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
	int ret = 0;
	u32 pos = *offset;
	u32 cnt = count;
	
	struct cdd_cdev *cdevp = filp->private_data;
	//printk("enter cdd_read!\n");
	if(cnt > (cdevp->data_len-pos) ){
		cnt = cdevp->data_len - pos;
	}
	
	ret = copy_to_user(buf, cdevp->kbuf+pos, cnt);
	//printk("kernel kbuf content:%s\n", cdevp->kbuf);
	*offset += cnt;
	
	return ret;
}
 
int cdd_write(struct file *filp, const char __user *buf, size_t count, loff_t *offset)
{
	int ret = 0;
	struct cdd_cdev *cdevp = filp->private_data;
	u32 pos = *offset;
	u32 cnt = count;
	
	//printk("enter cdd_write!\n");
	if(cnt > (BUF_SIZE - pos) ){
		cnt = BUF_SIZE - pos;
	}
	ret = copy_from_user(cdevp->kbuf+pos, buf, cnt);
	*offset += cnt;
	if(*offset > cdevp->data_len){
		cdevp->data_len = *offset;
	}
	return ret;
}
 
int cdd_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long data)
{
	//printk("enter cdd_ioctl!\n");
	switch(cmd){
		case 1://点亮灯
			//设置管脚为输出功能
			//参数：1.要设置的管脚编号2.默认的输出值 0低电平1高电平
			gpio_direction_output(led_gpio_table[data], 0);
			//禁止内部上拉
			s3c_gpio_setpull(led_gpio_table[data], SEC_GPIO_PULL_NONE);
			//设置输出值
			gpio_set_value(led_gpio_table[data], 1);
			
			break;
		case 0://熄灭灯
			
			//设置管脚为输出功能
			//参数：1.要设置的管脚编号2.默认的输出值 0低电平1高电平
			gpio_direction_output(led_gpio_table[data], 0);
			//禁止内部上拉
			s3c_gpio_setpull(led_gpio_table[data], SEC_GPIO_PULL_NONE);
			//设置输出值
			gpio_set_value(led_gpio_table[data], 0);
			
			break;
		default:
			return -EINVAL;
	}
	
	
	return 0;
}
 
int cdd_release(struct inode *inode, struct file *filp)
{
	struct cdd_cdev *pcdevp = filp->private_data;
	printk("enter cdd_release!\n");
	atomic_inc(&(pcdevp->av));
	//pcdevp->opentimes++;
	return 0;
}
 
loff_t cdd_llseek(struct file *filp, loff_t offset, int whence)
{
	struct cdd_cdev *pcdevp = filp->private_data;
	loff_t newpos = 0;
	switch(whence){
		case SEEK_SET:
			newpos = offset;
			break;
		case SEEK_CUR:
			newpos = filp->f_pos + offset;
			break;
		case SEEK_END:
			newpos = pcdevp->data_len + offset;
			break;
		default:
			return -EINVAL;//无效的参数
	}
	
	if( newpos<0 || newpos>= BUF_SIZE ){
		return -EINVAL;
	}
	filp->f_pos = newpos;
	return newpos;
}
 
struct file_operations cdd_fops = {
	.owner = THIS_MODULE,
	.open = cdd_open,
	.read = cdd_read,
	.write = cdd_write,
	.ioctl = cdd_ioctl,
	.release = cdd_release,
	.llseek = cdd_llseek,
	};
 
int __init cdd_init(void)
{
	int ret = 0;
	int i = 0;
	
	if(cdd_major){
		dev = MKDEV(CDD_MAJOR, CDD_MINOR);//生成设备号
		//注册设备号;1、要注册的起始设备号2、连续注册的设备号个数3、名字
		ret = register_chrdev_region(dev, CDD_COUNT, "cdd_demo");
	}else{
		// 动态分配设备号
		ret = alloc_chrdev_region(&dev, cdd_minor, CDD_COUNT, "cdd_demo02");
	}
	
	if(ret < 0){
		printk("register_chrdev_region failed!\n");
		goto failure_register_chrdev;
	}
	//获取主设备号
	cdd_major = MAJOR(dev);
	printk("cdd_major = %d\n", cdd_major);
	
	cdd_cdevp = kzalloc(sizeof(struct cdd_cdev)*CDD_COUNT, GFP_KERNEL);
	if(IS_ERR(cdd_cdevp)){
		printk("kzalloc failed!\n");
		goto failure_kzalloc;
	}
	/*创建设备类*/
	dev_class = class_create(THIS_MODULE, "cdd_class");
	if(IS_ERR(dev_class)){
		printk("class_create failed!\n");
		goto failure_dev_class;
	}
	for(i=0; i<CDD_COUNT; i++){
		/*初始化cdev*/
		cdev_init(&(cdd_cdevp[i].cdev), &cdd_fops);
		/*添加cdev到内核*/
		cdev_add(&(cdd_cdevp[i].cdev), dev+i, 1);
		//初始化原子变量
		ATOMIC_SET(&(cdd_cdevp[i].av), 1);
		/*
		cdd_cdevp[i].opentimes = 1;
		*/
		
		/* “/dev/xxx” */
		device_create(dev_class, NULL, dev+i, NULL, "cdd%d", i);
		
		cdd_cdevp[i].led = i;
		
	}
	
	return 0;
failure_dev_class:
	kfree(cdd_cdevp);
failure_kzalloc:
	unregister_chrdev_region(dev, CDD_COUNT);
failure_register_chrdev:
	return ret;
}
 
void __exit cdd_exit(void)
{
/*逆序消除*/
	int i = 0;
	for(; i < CDD_COUNT; i++){
		device_destroy(dev_class, dev+i);
		cdev_del(&(cdd_cdevp[i].cdev));
		//cdev_del(&((cdd_cdevp+i)->cdev));
	}
	class_destroy(dev_class);
	kfree(cdd_cdevp);
	unregister_chrdev_region(dev, CDD_COUNT);
	
}	
 
module_init(cdd_init);
module_exit(cdd_exit);