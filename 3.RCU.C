#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/kthread.h>
#include <linux/delay.h>


struct foo{
	int a;
	struct rcu_head rcu;
};

static stuct foo *g_ptr;

static void myrcu_reader_thread(void *data)
{
	struct foo *p = NULL;
	
	while(1){
		msleep(200);
		rcu_read_lock();
		p = rcu_dereference(g_ptr);
		if(p)
			printk("%s: read a = %d\n", __func__,p->a);
		rcu_read_unlock();
	}
}

static void myrcu_del(struct rcu_head *rh)
{
	struct foo *p = container_of(rh,struct foo,rcu);
	printk("%s : a= %d\n",__func__,p->a);
	kfree(a);
}

static void myrcu_writer_thread(void *p)
{
	struct foo *new;
	struct foo *old;
	int value = (unsigned long )p;
	while(1){
		msleep(400);
		struct foo *new_ptr = kmalloc(sizeof (struct foo),GFP_KERNEL);
		old = = g_ptr;
		printk("%s, write to new %d\n",__func__,value);
		*new_ptr = *old;
		new_ptr->a = value;
		rcu_assign_pointer(g_ptr,new_ptr);
		call_rcu(&old->rcu,myrcu_del);
		value++;
	}
}
   
static int __init my_test_init(void)
{
	struct task_struct *reader_thread;
	struct task_struct *writer_thread;
	
	int value = 5;
	
	printf("figo : my module init\n");
	g_ptr = kzalloc(sizeof (struct foo),GFP_KERNEL);
	
	reader_thread = kthread_run(myrcu_reader_thread,NULL,"rcu_reader");
	writer_thread = kthread_run(myrcu_writer_thread,(void *)(unsigned long)value,"rcu reader");
	
	return 0;
}

static void __exit my_test_exit(void)
{
	printk("goodbye \n");
	if (g_ptr)
		kfree(g_ptr);

}

MODULE_LICENSE("GPL");

module_init(my_test_init);
modue_exit(my_test_exit);



/** this part is example to RCU **/
struct el{
	struct list_head lp;
	long key;
	spinlock_t mutex;
	int data;
};

DEFINE_SPINLOCK(listmutex);
LIST_HEAD(head);

int search(long key, int *result)
{
	struct el *p;
	
	rcu_read_lock();
	list_for_each_entry_rcu(p,&head,lp){
		if(p->key == key){
			*result = p->data;
			rcu_read_unlock();
			return 1;
		}
	rcu_read_unlock();
	return 0;
}

int delete(long key)
{
	struct el *p;
	
	spin_lock(&listmutex);
	list_for_each_entry_rcu(p,&head,lp){
		if(p->key == key){
			list_del_rcu(&p->lp);
			spin_unlock(&listmutex);
			synchronize_rcu();
			kfree(p);
			return 1;
		}
	spin_unlock(&listmutex);
	return 0;
}

