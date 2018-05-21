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

//�жϷ�ʽ+poll+fasync+���⣬ͬ��������

static struct timer_list buttons_timer;

//����һ���ȴ�����ͷwait_queue_head     		button_waitq
static DECLARE_WAIT_QUEUE_HEAD(button_waitq);

// �ж��¼���־, �жϷ����������1��buttons_drv_read������0 
static volatile int ev_press = 0;

//����һ���첽���ƽṹ�����ڰ������첽֪ͨ
static struct fasync_struct *button_async;

#if 0   //ʹ��ԭ����

//ͬһʱ��ֻ��һ��Ӧ�ó���򿪴���������
//����һʹ��ԭ�Ӳ���
//ԭ�Ӳ���ָ������ִ�й����в��ᱻ��Ĵ���·�����жϵĲ�����
//����ԭ�Ӳ�������������
//atomic_t v = ATOMIC_INIT(0);     //����ԭ�ӱ���v����ʼ��Ϊ0
//atomic_read(atomic_t *v);        //����ԭ�ӱ�����ֵ
//void atomic_inc(atomic_t *v);    //ԭ�ӱ�������1
//void atomic_dec(atomic_t *v);    //ԭ�ӱ�������1
//int atomic_dec_and_test(atomic_t *v); //�Լ�������������Ƿ�Ϊ0��Ϊ0�򷵻�true�����򷵻�false��
//����ԭ�ӱ�������ʼ��Ϊ1
static atomic_t canopen = ATOMIC_INIT(1);     

#else	//ʹ���ź���
//������ʹ���ź���
//�ź�����semaphore�������ڱ����ٽ�����һ�ֳ��÷�����ֻ�еõ��ź����Ľ��̲���ִ���ٽ������롣
//����ȡ�����ź���ʱ�����̽������ߵȴ�״̬��

//�����ź���
//struct semaphore sem;
//��ʼ���ź���
//void sema_init (struct semaphore *sem, int val);
//void init_MUTEX(struct semaphore *sem);//��ʼ��Ϊ0

//static DECLARE_MUTEX(button_lock);     //���廥����

//����ź���
//void down(struct semaphore * sem);
//int down_interruptible(struct semaphore * sem); 
//int down_trylock(struct semaphore * sem);
//�ͷ��ź���
//void up(struct semaphore * sem);

//��ʼ���ź���

static DECLARE_MUTEX(button_lock);     //���廥����

#endif

struct pin_desc{
	unsigned int pin;
	unsigned int key_val;
};


/* ��ֵ: ����ʱ, 0x01, 0x02, 0x03, 0x04 */
/* ��ֵ: �ɿ�ʱ, 0x81, 0x82, 0x83, 0x84 */
static unsigned char key_val;

struct pin_desc pins_desc[4] = {
	{S3C2410_GPF0, 	0x01},
	{S3C2410_GPF2, 	0x02},
	{S3C2410_GPG3, 	0x03},
	{S3C2410_GPG11, 0x04},
};

static struct pin_desc *irq_pd;


/*
  * ȷ������ֵ
  */
static irqreturn_t s3c24xx_buttons_irq(int irq, void *dev_id)
{
	//������
	/*struct pin_desc * pindesc = (struct pin_desc *)dev_id;
	unsigned int pinval;
	
	pinval = s3c2410_gpio_getpin(pindesc->pin);

	if (pinval)
	{
		// �ɿ� 
		key_val = 0x80 | pindesc->key_val;
	}
	else
	{
		//���� 
		key_val = pindesc->key_val;
	}

    ev_press = 1;                 //��ʾ�жϷ����� 
    wake_up_interruptible(&button_waitq);   // �������ߵĽ��� 

	kill_fasync (&button_async, SIGIO, POLL_IN); //�첽������Ӧ�ó��򷢳�֪ͨ��Ϣ
	*/

	//10ms��������ʱ��     ����
	irq_pd = (struct pin_desc *)dev_id;
	mod_timer(&buttons_timer, jiffies+HZ/100);
	return IRQ_RETVAL(IRQ_HANDLED);
}

static int s3c24xx_buttons_drv_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	
#if 0   //ʹ��ԭ����
	//ʵ�ֻ��⹦�ܣ������һ����������ǣ��޷�ȷ��ԭ�Ӳ����������Ҫʹ��ԭ����
	//int atomic_dec_and_test(atomic_t *v); //�Լ�������������Ƿ�Ϊ0��Ϊ0�򷵻�true�����򷵻�false��
	if (!atomic_dec_and_test(&canopen))
	{
		//void atomic_inc(atomic_t *v);    //ԭ�ӱ�������1
		atomic_inc(&canopen);
		printk("buttons_drv is busy\n");
		return -EBUSY;
	}
#else	//ʹ���ź���
	if (file->f_flags & O_NONBLOCK)    //O_NONBLOCK  ��ʾ������
	{
		if (down_trylock(&button_lock))   //��ͼ��ȡ�ź�����������ܻ�ȡ����������
		{	
			printk("buttons_drv is busy\n");
			return -EBUSY;
		}
	}
	else    //��������
	{
		/* ��ȡ�ź��� */
		down(&button_lock);  //������ܻ�ȡ�ź��������뽩��״̬���ȴ��ź���
	}
#endif
	
	//����GPF0,2���Ź��� 
	// ����GPG3,11Ϊ���Ź��� 

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

	// ���û�а�������, ���� 
	//wait_event_interruptible(button_waitq, ev_press);

	if (file->f_flags & O_NONBLOCK)
	{
		if (!ev_press)
			return -EAGAIN;
	}
	else
	{
		/* ���û�а�������, ���� */
		wait_event_interruptible(button_waitq, ev_press);
	}

	//����а�������, ���ؼ�ֵ 
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
#if 0   //ʹ��ԭ����
	atomic_inc(&canopen);		//void atomic_inc(atomic_t *v);  //ԭ�ӱ�������1	
#else	//ʹ���ź���
	up(&button_lock);
#endif

	free_irq(IRQ_EINT0, &pins_desc[0]);
	free_irq(IRQ_EINT2, &pins_desc[1]);
	free_irq(IRQ_EINT11, &pins_desc[2]);
	free_irq(IRQ_EINT19, &pins_desc[3]);
	return 0;
}

//Poll���ƻ��ж�fds���ļ��Ƿ�ɶ�������ɶ�����������أ����ص�ֵ���ǿɶ�fd��������
//������ɶ�����ô�ͽ��̾ͻ�����timeout��ô����ʱ�䣬Ȼ�������ж��Ƿ����ļ��ɶ���
//����У�����fd�����������û�У��򷵻�0. 
static unsigned s3c24xx_buttons_drv_poll(struct file *file, poll_table *wait)
{
	unsigned int mask = 0;
	//
	poll_wait(file, &button_waitq, wait); // ������������

	if (ev_press)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

//�а��������ˣ��������������ѣ���������Ӧ�ó���ȥ����ֵ���á�signal���ڽ���֮�䷢�źţ�
//��������ȥ��ѯ��read
static int s3c24xx_buttons_drv_fasync (int fd, struct file *filp, int on)
{
	printk("driver: fifth_drv_fasync\n");

	//��ʼ�����ڰ������첽֪ͨ�ṹ��
	return fasync_helper (fd, filp, on, &button_async);
}


static struct file_operations buttons_drv_fops = {
    .owner   =  THIS_MODULE,    // ����һ���꣬�������ģ��ʱ�Զ�������__this_module����
    .open    =  s3c24xx_buttons_drv_open,   	//��Ӧ�ó������openʱ���� 
	.read	 =	s3c24xx_buttons_drv_read,		//��Ӧ�ó������readʱ����  
	.release =  s3c24xx_buttons_drv_close,		//��Ӧ�ó������close��ж������ʱ���� 
	.poll    =  s3c24xx_buttons_drv_poll,		//��Ӧ�ó������pollʱ���� 
	.fasync	 =  s3c24xx_buttons_drv_fasync,		//��Ӧ�ó������fcntlʱ���� 
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
		// �ɿ� 
		key_val = 0x80 | pindesc->key_val;
	}
	else
	{
		//���� 
		key_val = pindesc->key_val;
	}

    ev_press = 1;                  //��ʾ�жϷ����� 
    wake_up_interruptible(&button_waitq);   // �������ߵĽ��� 
	
	kill_fasync (&button_async, SIGIO, POLL_IN);
	
}



int major;

// ִ��insmod����ʱ�ͻ����������� 
static int s3c24xx_buttons_drv_init(void)
{
	//��ʼ����ʱ��
	init_timer(&buttons_timer);
	buttons_timer.function = s3c24xx_buttons_timer_function;   //���ö�ʱ��������
	//buttons_timer.expires = 0;      //���ó�ʱ����
	add_timer(&buttons_timer);


	major = register_chrdev(0, "buttons_drv", &buttons_drv_fops);

	buttonsdrv_class = class_create(THIS_MODULE, "buttons_drv");

	// Ϊ����mdev������Щ��Ϣ�������豸�ڵ�
	buttonsdrv_class_dev = class_device_create(buttonsdrv_class, NULL, MKDEV(major, 0), NULL, "buttons"); /* /dev/buttons */

	printk("buttons_drv initialized\n");

	return 0;
}

// ִ��rmmod����ʱ�ͻ�����������  
static void s3c24xx_buttons_drv_exit(void)
{
	unregister_chrdev(major, "buttons_drv");
	//ж����������
	class_device_unregister(buttonsdrv_class_dev);
	class_destroy(buttonsdrv_class);
}

//��ѯ��ʽ
/*
volatile unsigned long *gpfcon;
volatile unsigned long *gpfdat;

volatile unsigned long *gpgcon;
volatile unsigned long *gpgdat;

static int s3c24xx_buttons_drv_open(struct inode *inode, struct file *file)
{
	// ����GPF0,2Ϊ�������� 
	*gpfcon &= ~((0x3<<(0*2)) | (0x3<<(2*2)));

	//����GPG3,11Ϊ�������� 
	*gpgcon &= ~((0x3<<(3*2)) | (0x3<<(11*2)));

	return 0;
}

ssize_t s3c24xx_buttons_drv_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	//����4�����ŵĵ�ƽ 
	unsigned char key_vals[4];
	int regval;

	if (size != sizeof(key_vals))
		return -EINVAL;

	// ��GPF0,2 
	regval = *gpfdat;
	key_vals[0] = (regval & (1<<0)) ? 1 : 0;
	key_vals[1] = (regval & (1<<2)) ? 1 : 0;
	

	// ��GPG3,11 
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
    .owner  =   THIS_MODULE,    //����һ���꣬�������ģ��ʱ�Զ�������__this_module���� 
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



/* ������ָ����������ĳ�ʼ��������ж�غ��� */
module_init(s3c24xx_buttons_drv_init);
module_exit(s3c24xx_buttons_drv_exit);

/* �������������һЩ��Ϣ�����Ǳ���� */
MODULE_AUTHOR("lc_sagacity");
MODULE_VERSION("0.1.0");
MODULE_DESCRIPTION("S3C2410/S3C2440 Buttons Driver");
MODULE_LICENSE("GPL");


