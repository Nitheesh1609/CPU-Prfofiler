#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/proc_fs.h>
#include<linux/seq_file.h>
#include<linux/spinlock.h>
#include<linux/kprobes.h>
#include<linux/hashtable.h>
#include<linux/sched.h>
#include<linux/types.h>
#include<linux/slab.h>
#include<linux/stacktrace.h>
#include<linux/init.h>
#include<linux/jhash.h>
#include<linux/kallsyms.h>

#define PID_HASH_BITS 10
#define MAX_SYMBOL_LEN	64
#define FUNCTION_NAME_LENGTH 20
#define MAX_ARRAY_SIZE 50
#define PROFILER_OUTPUT_NO 20

//krpobe to be hooked at pick_next_task_fair
static char symbol[MAX_SYMBOL_LEN] = "pick_next_task_fair";
module_param_string(symbol, symbol, sizeof(symbol), 0644);

extern unsigned int stack_trace_save_user(unsigned long *store, unsigned int size);
typedef typeof(&stack_trace_save_user) stack_trace_save_user_fn;
#define stack_trace_save_user (* (stack_trace_save_user_fn)map_stack_trace_save_user)
void *map_stack_trace_save_user =NULL;

//static DEFINE_HASHTABLE(pid_hash,PID_HASH_BITS);
static DEFINE_SPINLOCK(pid_lock);

struct rb_root mytree = RB_ROOT;
unsigned long long timer_value=0;
//HASH implementaion
/*
struct pid_entry{
	pid_t pid;
	struct hlist_node hash;
	int count;
	int key;
	unsigned long long sched_time;
	unsigned int size2;
	unsigned long array[MAX_ARRAY_SIZE];
};
*/

struct rb_entry{
	pid_t pid;
	struct rb_node rbentry;
	int count;
	int key;
	unsigned long long sched_time;
	unsigned int size2;
	unsigned long array[MAX_ARRAY_SIZE];
};

/*
static int store_hash(pid_t pid, int key,unsigned long *array,unsigned int size2,unsigned long long time)
{
	int i=0;
	struct pid_entry *temp;
	hash_for_each_possible(pid_hash,temp,hash,key){
		if(temp!=NULL){
			temp->count=temp->count+1;
			temp->sched_time+=time;
			return 0;
		}
	}

	temp=kmalloc(sizeof(struct pid_entry),GFP_ATOMIC);
	if(!temp)
		return ENOMEM;
	temp->count=1;
	temp->key=key;
	if(size2<=MAX_ARRAY_SIZE){
	for(i=0;i<size2;i++)
		temp->array[i]=array[i];
	}
	temp->pid=pid;
	temp->size2=size2;
	temp->sched_time=time;
	hash_add(pid_hash,&temp->hash,key);
	return 0;
}
*/

static int store_rbtree(pid_t pid,unsigned int key, unsigned long array[], unsigned int size2, unsigned long long time)
{
	int i = 0;
	unsigned long long r_time = 0;
	unsigned int count = 0;
	struct rb_entry *node3, *anode,*travnode;
	struct rb_node **cnode, *parent = NULL,*prevnode;
	prevnode= rb_last(&mytree);
	while(prevnode)
	{
		travnode = rb_entry(prevnode,struct rb_entry,rbentry);
		prevnode = rb_prev(prevnode);
		if(travnode->key == key)
		{
			r_time = travnode->sched_time;
			count=travnode->count;
		       	rb_erase(&travnode->rbentry,&mytree);
			kfree(travnode);
		}
	}
	node3 = (struct rb_entry*)kmalloc(sizeof(struct rb_entry),GFP_ATOMIC);
	if(!node3)
		return ENOMEM;
	node3->key = key;
	node3->count=count+1;
	node3->pid=pid;
	node3->size2=size2;
	node3->sched_time = r_time+time;
	if(size2<MAX_ARRAY_SIZE){
	for(i = 0; i < size2;i++)
	{
		node3->array[i] = array[i];
	}
	}
	cnode = &mytree.rb_node;
	while(*cnode != NULL)
	{
		parent = *cnode;
		anode = rb_entry(parent, struct rb_entry, rbentry);
		if(node3->sched_time < anode->sched_time)
			cnode = &((*cnode)->rb_left);
		else
			cnode = &((*cnode)->rb_right);
	}
	rb_link_node(&node3->rbentry, parent, cnode);
	rb_insert_color(&(node3->rbentry),&mytree);
	
	return 0;
}


/*
static void destroy_hashtable(void)
	{
	struct pid_entry *temp;
		int bkt=0;
		hash_for_each(pid_hash,bkt,temp,hash){
			hash_del(&temp->hash);
			kfree(temp);
		}
}
*/

static void destroy_rbtree(void){
	struct rb_node *node = rb_first(&mytree);
	while(node)
	{
		struct rb_entry *temp = rb_entry(node, struct rb_entry, rbentry);
		node = rb_next(node);
		rb_erase(&temp->rbentry,&mytree);
		kfree(temp);
	}
}

//proc show
static int perftop_show(struct seq_file *m, void *v) {
//	struct pid_entry *temp;
//	int bkt=0;
	unsigned long flags;
	struct rb_node *tmpnode = rb_last(&mytree); 
    	struct rb_entry *current_node;
	int i=0, j=0;
	spin_lock_irqsave(&pid_lock,flags);
	seq_printf(m,"20 Most Scheduled tasks are");
	for (j = 1; j <= 20; j++){
		if(tmpnode!=NULL){
			current_node=rb_entry(tmpnode, struct rb_entry,rbentry);
			seq_printf(m,"\n%d.The PID %d has scheduled %d times and has executed %lld clock cycles\n",j,current_node->pid,current_node->count,current_node->sched_time);
			seq_printf(m, "The Stack Trace of size %d is: \n",current_node->size2);
			for (i = 0; i < current_node->size2; i++)
				seq_printf(m,"%pB\t",(void*)current_node->array[i]);
			tmpnode= rb_prev(tmpnode); 
		}
	}
	seq_printf(m,"\n");
	

///	hash_for_each(pid_hash,bkt,temp,hash){
//		seq_printf(m,"\nThe PID %d has scheduled %d times and has executed %lld clock cycles",temp->pid,temp->count,temp->sched_time);
//		seq_printf(m,"\n The stacktrace is\t");
//		for(i=0;i<(temp->size2);i++)
//			seq_printf(m," %lu \t",temp->array[i]);
//		}
//	seq_printf(m,"\n");
	spin_unlock_irqrestore(&pid_lock,flags);
	return 0;
}

static struct kprobe kp={
	.symbol_name= symbol,
};

static struct kprobe kl = {
		.symbol_name = "kallsyms_lookup_name"
};

static int handler_pre(struct kprobe *p,struct pt_regs *regs)
{
	unsigned long long curr_timer,time;
	unsigned long pointer = regs->si;
	struct task_struct* prev= (struct task_struct*)pointer;
	int ret=0;
	unsigned long array[MAX_ARRAY_SIZE],flags;
	unsigned int pid_here=0,size2=0,jhashkey;
	if(prev==NULL)
	{
		printk("NULL pointer");
		return 0;
	}
	spin_lock_irqsave(&pid_lock,flags);
	curr_timer=rdtsc();
	time=curr_timer - timer_value;	
	pid_here = (unsigned int )prev->pid;
	if((prev->mm)==NULL)
	{
		size2=stack_trace_save(array,MAX_ARRAY_SIZE,0);
	}
	else
	{
		size2=stack_trace_save_user(array,MAX_ARRAY_SIZE);
		printk("\n length is %d",size2);
	}
	jhashkey=jhash2((u32 *)array,size2,0);
//	ret=store_hash(pid_here,jhashkey,array,size2,time);
	ret=store_rbtree(pid_here,jhashkey,array,size2,time);
	if(ret!=0)
		printk("\nError in hash_entry\n");
	spin_unlock_irqrestore(&pid_lock,flags);
	return 0;	
}

static void handler_post(struct kprobe *p,struct pt_regs *regs,unsigned long flags){
	timer_value= rdtsc();
}

static int perftop_open(struct inode *inode, struct  file *file) {
	  return single_open(file, perftop_show, NULL);
}

static const struct proc_ops perftop_fops = {
	.proc_open = perftop_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int __init perftop_init(void) {
	int ret;
	typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
	kallsyms_lookup_name_t kallsyms_lookup_name;
	register_kprobe(&kl);
	kallsyms_lookup_name = (kallsyms_lookup_name_t) kl.addr;
	unregister_kprobe(&kl);
	map_stack_trace_save_user = (void*)kallsyms_lookup_name("stack_trace_save_user");
	timer_value=rdtsc();
	proc_create("perftop", 0, NULL, &perftop_fops);
	kp.pre_handler=handler_pre;
	kp.post_handler=handler_post;
	ret=register_kprobe(&kp);
	if(ret<0){
		printk("register_kprobe failed %d \n",ret);
		return ret;
	}
	    return 0;
}

static void __exit perftop_exit(void) {
	unregister_kprobe(&kp);
//	destroy_hashtable();
	destroy_rbtree();
	remove_proc_entry("perftop", NULL);
}

MODULE_LICENSE("GPL");
module_init(perftop_init);
module_exit(perftop_exit);

