#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/poll.h>

#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>


static struct class *buttonsdrv_class;
static struct class_device	*buttonsdrv_class_dev;

//中断方式+poll+fasync+互斥，同步，阻塞

static struct timer_list buttons_timer;

//生成一个等待队列头wait_queue_head     		button_waitq
static DECLARE_WAIT_QUEUE_HEAD(button_waitq);

// 中断事件标志, 中断服务程序将它置1，buttons_drv_read将它清0 
static volatile int ev_press = 0;

//定义一个异步机制结构体用于按键的异步通知
static struct fasync_struct *button_async;

#if 0   //使用原子锁

//同一时刻只有一个应用程序打开此驱动程序
//方法一使用原子操作
//原子操作指的是在执行过程中不会被别的代码路径所中断的操作。
//常用原子操作函数举例：
//atomic_t v = ATOMIC_INIT(0);     //定义原子变量v并初始化为0
//atomic_read(atomic_t *v);        //返回原子变量的值
//void atomic_inc(atomic_t *v);    //原子变量增加1
//void atomic_dec(atomic_t *v);    //原子变量减少1
//int atomic_dec_and_test(atomic_t *v); //自减操作后测试其是否为0，为0则返回true，否则返回false。
//定义原子变量并初始化为1
static atomic_t canopen = ATOMIC_INIT(1);     

#else	//使用信号量
//方法二使用信号量
//信号量（semaphore）是用于保护临界区的一种常用方法，只有得到信号量的进程才能执行临界区代码。
//当获取不到信号量时，进程进入休眠等待状态。

//定义信号量
//struct semaphore sem;
//初始化信号量
//void sema_init (struct semaphore *sem, int val);
//void init_MUTEX(struct semaphore *sem);//初始化为0

//static DECLARE_MUTEX(button_lock);     //定义互斥锁

//获得信号量
//void down(struct semaphore * sem);
//int down_interruptible(struct semaphore * sem); 
//int down_trylock(struct semaphore * sem);
//释放信号量
//void up(struct semaphore * sem);

//初始化信号量

static DECLARE_MUTEX(button_lock);     //定义互斥锁

#endif

struct pin_desc{
	unsigned int pin;
	unsigned int key_val;
};


/* 键值: 按下时, 0x01, 0x02, 0x03, 0x04 */
/* 键值: 松开时, 0x81, 0x82, 0x83, 0x84 */
static unsigned char key_val;

struct pin_desc pins_desc[4] = {
	{S3C2410_GPF0, 	0x01},
	{S3C2410_GPF2, 	0x02},
	{S3C2410_GPG3, 	0x03},
	{S3C2410_GPG11, 0x04},
};

static struct pin_desc *irq_pd;


/*
  * 确定按键值
  */
static irqreturn_t s3c24xx_buttons_irq(int irq, void *dev_id)
{
	//不防抖
	/*struct pin_desc * pindesc = (struct pin_desc *)dev_id;
	unsigned int pinval;
	
	pinval = s3c2410_gpio_getpin(pindesc->pin);

	if (pinval)
	{
		// 松开 
		key_val = 0x80 | pindesc->key_val;
	}
	else
	{
		//按下 
		key_val = pindesc->key_val;
	}

    ev_press = 1;                 //表示中断发生了 
    wake_up_interruptible(&button_waitq);   // 唤醒休眠的进程 

	kill_fasync (&button_async, SIGIO, POLL_IN); //异步机制向应用程序发出通知信息
	*/

	//10ms后启动定时器     防抖
	irq_pd = (struct pin_desc *)dev_id;
	mod_timer(&buttons_timer, jiffies+HZ/100);
	return IRQ_RETVAL(IRQ_HANDLED);
}

static int s3c24xx_buttons_drv_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	
#if 0   //使用原子锁
	//实现互斥功能，如果用一个变量来标记，无法确保原子操作，因此需要使用原子锁
	//int atomic_dec_and_test(atomic_t *v); //自减操作后测试其是否为0，为0则返回true，否则返回false。
	if (!atomic_dec_and_test(&canopen))
	{
		//void atomic_inc(atomic_t *v);    //原子变量增加1
		atomic_inc(&canopen);
		printk("buttons_drv is busy\n");
		return -EBUSY;
	}
#else	//使用信号量
	if (file->f_flags & O_NONBLOCK)    //O_NONBLOCK  表示非阻塞
	{
		if (down_trylock(&button_lock))   //试图获取信号量，如果不能获取，立即返回
		{	
			printk("buttons_drv is busy\n");
			return -EBUSY;
		}
	}
	else    //阻塞操作
	{
		/* 获取信号量 */
		down(&button_lock);  //如果不能获取信号量，进入僵死状态并等待信号量
	}
#endif
	
	//配置GPF0,2引脚功能 
	// 配置GPG3,11为引脚功能 

	printk("buttons_drv open\n");
	ret = request_irq(IRQ_EINT0,  s3c24xx_buttons_irq, IRQT_BOTHEDGE, "S2", &pins_desc[0]);
	if(ret != 0)
	{
		free_irq(IRQ_EINT0, &pins_desc[0]);
		return 0;
	}
	
	printk("S2 ret = %d\n",ret);
	ret = request_irq(IRQ_EINT2,  s3c24xx_buttons_irq, IRQT_BOTHEDGE, "S3", &pins_desc[1]);
	if(ret != 0)
	{
		free_irq(IRQ_EINT2, &pins_desc[1]);
		return 0;
	}

	printk("S3 ret = %d\n",ret);
	ret = request_irq(IRQ_EINT11, s3c24xx_buttons_irq, IRQT_BOTHEDGE, "S4", &pins_desc[2]);
	if(ret != 0)
	{
		free_irq(IRQ_EINT11, &pins_desc[2]);
		return 0;
	}
	
	printk("S4 ret = %d\n",ret);
	ret = request_irq(IRQ_EINT19, s3c24xx_buttons_irq, IRQT_BOTHEDGE, "S5", &pins_desc[3]);
	if(ret != 0)
	{
		free_irq(IRQ_EINT19, &pins_desc[3]);
		return 0;
	}

	printk("S5 ret = %d\n",ret);
	
	return 0;
}

ssize_t s3c24xx_buttons_drv_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	if (size != 1)
		return -EINVAL;

	// 如果没有按键动作, 休眠 
	//wait_event_interruptible(button_waitq, ev_press);

	if (file->f_flags & O_NONBLOCK)
	{
		if (!ev_press)
			return -EAGAIN;
	}
	else
	{
		/* 如果没有按键动作, 休眠 */
		wait_event_interruptible(button_waitq, ev_press);
	}

	//如果有按键动作, 返回键值 
	if(copy_to_user(buf, &key_val, 1))
	{
		ev_press = 0;
		return 1;
	}

	ev_press = 0;
	
	return 1;
}


int s3c24xx_buttons_drv_close(struct inode *inode, struct file *file)
{
#if 0   //使用原子锁
	atomic_inc(&canopen);		//void atomic_inc(atomic_t *v);  //原子变量增加1	
#else	//使用信号量
	up(&button_lock);
#endif

	free_irq(IRQ_EINT0, &pins_desc[0]);
	free_irq(IRQ_EINT2, &pins_desc[1]);
	free_irq(IRQ_EINT11, &pins_desc[2]);
	free_irq(IRQ_EINT19, &pins_desc[3]);
	return 0;
}

//Poll机制会判断fds中文件是否可读，如果可读则会立即返回，返回的值就是可读fd的数量，
//如果不可读，那么就进程就会休眠timeout这么长的时间，然后再来判断是否有文件可读，
//如果有，返回fd的数量，如果没有，则返回0. 
static unsigned s3c24xx_buttons_drv_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	//
	poll_wait(file, &button_waitq, wait); // 不会立即休眠

	if (ev_press)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

//有按键按下了，驱动程序来提醒（触发）“应用程序”去读键值。用“signal”在进程之间发信号，
//不用主动去查询和read
static int s3c24xx_buttons_drv_fasync (int fd, struct file *filp, int on)
{
	printk("driver: fifth_drv_fasync\n");

	//初始化用于按键的异步通知结构体
	return fasync_helper (fd, filp, on, &button_async);
}


static struct file_operations buttons_drv_fops = {
    .owner   =  THIS_MODULE,    // 这是一个宏，推向编译模块时自动创建的__this_module变量
    .open    =  s3c24xx_buttons_drv_open,   	//在应用程序调用open时调用 
	.read	 =	s3c24xx_buttons_drv_read,		//在应用程序调用read时调用  
	.release =  s3c24xx_buttons_drv_close,		//在应用程序调用close或卸载驱动时调用 
	.poll    =  s3c24xx_buttons_drv_poll,		//在应用程序调用poll时调用 
	.fasync	 =  s3c24xx_buttons_drv_fasync,		//在应用程序调用fcntl时调用 
};

static void s3c24xx_buttons_timer_function(unsigned long data)
{
	struct pin_desc * pindesc = irq_pd;
	unsigned int pinval;

	if (!pindesc)
		return;
	
	pinval = s3c2410_gpio_getpin(pindesc->pin);

	if (pinval)
	{
		// 松开 
		key_val = 0x80 | pindesc->key_val;
	}
	else
	{
		//按下 
		key_val = pindesc->key_val;
	}

    ev_press = 1;                  //表示中断发生了 
    wake_up_interruptible(&button_waitq);   // 唤醒休眠的进程 
	
	kill_fasync (&button_async, SIGIO, POLL_IN);
	
}



int major;

// 执行insmod命令时就会调用这个函数 
static int s3c24xx_buttons_drv_init(void)
{
	//初始化定时器
	init_timer(&buttons_timer);
	buttons_timer.function = s3c24xx_buttons_timer_function;   //设置定时器处理函数
	//buttons_timer.expires = 0;      //设置超时参数
	add_timer(&buttons_timer);


	major = register_chrdev(0, "buttons_drv", &buttons_drv_fops);

	buttonsdrv_class = class_create(THIS_MODULE, "buttons_drv");

	// 为了让mdev根据这些信息来创建设备节点
	buttonsdrv_class_dev = class_device_create(buttonsdrv_class, NULL, MKDEV(major, 0), NULL, "buttons"); /* /dev/buttons */

	printk("buttons_drv initialized\n");

	return 0;
}

// 执行rmmod命令时就会调用这个函数  
static void s3c24xx_buttons_drv_exit(void)
{
	unregister_chrdev(major, "buttons_drv");
	//卸载驱动程序
	class_device_unregister(buttonsdrv_class_dev);
	class_destroy(buttonsdrv_class);
}

//查询方式
/*
volatile unsigned long *gpfcon;
volatile unsigned long *gpfdat;

volatile unsigned long *gpgcon;
volatile unsigned long *gpgdat;

static int s3c24xx_buttons_drv_open(struct inode *inode, struct file *file)
{
	// 配置GPF0,2为输入引脚 
	*gpfcon &= ~((0x3<<(0*2)) | (0x3<<(2*2)));

	//配置GPG3,11为输入引脚 
	*gpgcon &= ~((0x3<<(3*2)) | (0x3<<(11*2)));

	return 0;
}

ssize_t s3c24xx_buttons_drv_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	//返回4个引脚的电平 
	unsigned char key_vals[4];
	int regval;

	if (size != sizeof(key_vals))
		return -EINVAL;

	// 读GPF0,2 
	regval = *gpfdat;
	key_vals[0] = (regval & (1<<0)) ? 1 : 0;
	key_vals[1] = (regval & (1<<2)) ? 1 : 0;
	

	// 读GPG3,11 
	regval = *gpgdat;
	key_vals[2] = (regval & (1<<3)) ? 1 : 0;
	key_vals[3] = (regval & (1<<11)) ? 1 : 0;

	if(copy_to_user(buf, key_vals, sizeof(key_vals)))
	{
		return 0;
	}
	
	return sizeof(key_vals);
}



static struct file_operations buttons_drv_fops = {
    .owner  =   THIS_MODULE,    //这是一个宏，推向编译模块时自动创建的__this_module变量 
    .open   =   s3c24xx_buttons_drv_open,     
	.read	=	s3c24xx_buttons_drv_read,	   
};


int major;
static int s3c24xx_buttons_drv_init(void)
{
	major = register_chrdev(0, "buttons_drv", &buttons_drv_fops);

	buttonsdrv_class = class_create(THIS_MODULE, "buttons_drv");

	buttonsdrv_class_dev = class_device_create(buttonsdrv_class, NULL, MKDEV(major, 0), NULL, "buttons"); // /dev/buttons 

	gpfcon = (volatile unsigned long *)ioremap(0x56000050, 16);
	gpfdat = gpfcon + 1;

	gpgcon = (volatile unsigned long *)ioremap(0x56000060, 16);
	gpgdat = gpgcon + 1;

	return 0;
}

static void s3c24xx_buttons_drv_exit(void)
{
	unregister_chrdev(major, "buttons_drv");
	class_device_unregister(buttonsdrv_class_dev);
	class_destroy(buttonsdrv_class);
	iounmap(gpfcon);
	iounmap(gpgcon);
}
*/



/* 这两行指定驱动程序的初始化函数和卸载函数 */
module_init(s3c24xx_buttons_drv_init);
module_exit(s3c24xx_buttons_drv_exit);

/* 描述驱动程序的一些信息，不是必须的 */
MODULE_AUTHOR("lc_sagacity");
MODULE_VERSION("0.1.0");
MODULE_DESCRIPTION("S3C2410/S3C2440 Buttons Driver");
MODULE_LICENSE("GPL");


