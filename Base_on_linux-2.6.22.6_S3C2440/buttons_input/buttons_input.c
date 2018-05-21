
// �ο�drivers\input\keyboard\gpio_keys.c 

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/irq.h>

#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/regs-gpio.h>

struct pin_desc{
	int irq;
	char *name;
	unsigned int pin;
	unsigned int key_val;
};

struct pin_desc pins_desc[4] = {
	{IRQ_EINT0,  "S2", S3C2410_GPF0,   KEY_L},
	{IRQ_EINT2,  "S3", S3C2410_GPF2,   KEY_S},
	{IRQ_EINT11, "S4", S3C2410_GPG3,   KEY_ENTER},
	{IRQ_EINT19, "S5",  S3C2410_GPG11, KEY_LEFTSHIFT},
};

static struct input_dev *s3c24xx_buttons_dev;
static struct pin_desc *irq_pd;
static struct timer_list s3c24xx_buttons_timer;

static irqreturn_t s3c24xx_buttons_irq(int irq, void *dev_id)
{
	// 10ms��������ʱ�� 
	irq_pd = (struct pin_desc *)dev_id;
	mod_timer(&s3c24xx_buttons_timer, jiffies+HZ/100);
	return IRQ_RETVAL(IRQ_HANDLED);
}

static void s3c24xx_buttons_timer_function(unsigned long data)
{
	struct pin_desc * pindesc = irq_pd;
	unsigned int pinval;

	if (!pindesc)
		return;
	
	pinval = s3c2410_gpio_getpin(pindesc->pin);

	if (pinval)
	{
		//�ɿ� : ���һ������: 0-�ɿ�, 1-���� 
		input_event(s3c24xx_buttons_dev, EV_KEY, pindesc->key_val, 0);
		//ͬʱ�ϱ�һ��ͬ���¼�
		input_sync(s3c24xx_buttons_dev);
	}
	else
	{
		// ���� 
		input_event(s3c24xx_buttons_dev, EV_KEY, pindesc->key_val, 1);
		//ͬʱ�ϱ�һ��ͬ���¼�
		input_sync(s3c24xx_buttons_dev);
	}
}

static int s3c24xx_buttons_init(void)
{
	int i;
	int ret = 0;
	
	// 1. ����һ��input_dev�ṹ�� 
	s3c24xx_buttons_dev = input_allocate_device();;

	// 2. ���� 
	// 2.1 �ܲ��������¼� 
	set_bit(EV_KEY, s3c24xx_buttons_dev->evbit);//�����ܲ����������¼�
	set_bit(EV_REP, s3c24xx_buttons_dev->evbit);  //���ò����ظ��ϱ�����
	
	// 2.2 �ܲ���������������Щ�¼�: L,S,ENTER,LEFTSHIT 
	set_bit(KEY_L, s3c24xx_buttons_dev->keybit);
	set_bit(KEY_S, s3c24xx_buttons_dev->keybit);
	set_bit(KEY_ENTER, s3c24xx_buttons_dev->keybit);
	set_bit(KEY_LEFTSHIFT, s3c24xx_buttons_dev->keybit);

	// 3. ע�� 
	input_register_device(s3c24xx_buttons_dev);
	
	// 4. Ӳ����صĲ��� ������
	init_timer(&s3c24xx_buttons_timer);
	s3c24xx_buttons_timer.function = s3c24xx_buttons_timer_function;
	add_timer(&s3c24xx_buttons_timer);
	
	for (i = 0; i < 4; i++)
	{
		ret = request_irq(pins_desc[i].irq, s3c24xx_buttons_irq, IRQT_BOTHEDGE, pins_desc[i].name, &pins_desc[i]);
		if(ret != 0)
		{
			free_irq(pins_desc[i].irq, &pins_desc[i]);
			return 0;
		}
	}
	
	return 0;
}

static void s3c24xx_buttons_exit(void)
{
	int i;
	for (i = 0; i < 4; i++)
	{
		free_irq(pins_desc[i].irq, &pins_desc[i]);
	}

	del_timer(&s3c24xx_buttons_timer);
	input_unregister_device(s3c24xx_buttons_dev);
	input_free_device(s3c24xx_buttons_dev);	
}

module_init(s3c24xx_buttons_init);

module_exit(s3c24xx_buttons_exit);

/* �������������һЩ��Ϣ�����Ǳ���� */
MODULE_AUTHOR("lc_sagacity");
MODULE_VERSION("0.1.0");
MODULE_DESCRIPTION("S3C2410/S3C2440 Buttons Driver");
MODULE_LICENSE("GPL");




