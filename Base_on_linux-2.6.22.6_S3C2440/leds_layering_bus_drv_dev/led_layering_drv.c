

/* 分配/设置/注册一个platform_driver */

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
#include <asm/uaccess.h>
#include <asm/io.h>

static int major;


static struct class *cls;
static volatile unsigned long *gpio_con;
static volatile unsigned long *gpio_dat;
static int pin;

static int s3c24xx_layering_leds_open(struct inode *inode, struct file *file)
{
	//printk("first_drv_open\n");
	/* 配置为输出 */
	*gpio_con &= ~(0x3<<(pin*2));
	*gpio_con |= (0x1<<(pin*2));
	return 0;	
}

static ssize_t s3c24xx_layering_leds_write(struct file *file, const char __user *buf, size_t count, loff_t * ppos)
{
	int val;

	//printk("first_drv_write\n");

	copy_from_user(&val, buf, count); //	copy_to_user();

	if (val == 1)
	{
		// 点灯
		*gpio_dat &= ~(1<<pin);
	}
	else
	{
		// 灭灯
		*gpio_dat |= (1<<pin);
	}
	
	return 0;
}


static struct file_operations s3c24xx_layering_leds_fops = {
    .owner  =   THIS_MODULE,    /* 这是一个宏，推向编译模块时自动创建的__this_module变量 */
    .open   =   s3c24xx_layering_leds_open,     
	.write	=	s3c24xx_layering_leds_write,	   
};

static int s3c24xx_layering_leds_probe(struct platform_device *pdev)
{
	struct resource		*res;

	/* 根据platform_device的资源进行ioremap */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	gpio_con = ioremap(res->start, res->end - res->start + 1);
	gpio_dat = gpio_con + 1;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	pin = res->start;

	/* 注册字符设备驱动程序 */

	printk("s3c24xx_layering_leds_probe, found s3c24xx_layering_leds\n");

	major = register_chrdev(0, "mys3c24xx_layering_leds", &s3c24xx_layering_leds_fops);

	cls = class_create(THIS_MODULE, "mys3c24xx_layering_leds");

	class_device_create(cls, NULL, MKDEV(major, 0), NULL, "s3c24xx_layering_leds"); /* /dev/s3c24xx_layering_leds */
	
	return 0;
}

static int s3c24xx_layering_leds_remove(struct platform_device *pdev)
{
	/* 卸载字符设备驱动程序 */
	/* iounmap */
	printk("s3c24xx_layering_leds_remove, remove s3c24xx_layering_leds\n");

	class_device_destroy(cls, MKDEV(major, 0));
	class_destroy(cls);
	unregister_chrdev(major, "mys3c24xx_layering_leds");
	iounmap(gpio_con);
	
	return 0;
}


struct platform_driver s3c24xx_layering_leds_drv = {
	.probe		= s3c24xx_layering_leds_probe,
	.remove		= s3c24xx_layering_leds_remove,
	.driver		= {
		.name	= "mys3c24xx_layering_leds",
	}
};


static int s3c24xx_layering_leds_drv_init(void)
{
	platform_driver_register(&s3c24xx_layering_leds_drv);
	return 0;
}

static void s3c24xx_layering_leds_drv_exit(void)
{
	platform_driver_unregister(&s3c24xx_layering_leds_drv);
}

module_init(s3c24xx_layering_leds_drv_init);
module_exit(s3c24xx_layering_leds_drv_exit);

MODULE_LICENSE("GPL");



