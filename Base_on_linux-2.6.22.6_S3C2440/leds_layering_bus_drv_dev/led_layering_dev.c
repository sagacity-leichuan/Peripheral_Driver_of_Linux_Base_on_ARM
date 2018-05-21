#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>


/* 分配/设置/注册一个platform_device */

static struct resource s3c24xx_leds_resource[] = {
    [0] = {
        .start = 0x56000050,
        .end   = 0x56000050 + 8 - 1,
        .flags = IORESOURCE_MEM,
    },
    [1] = {
        .start = 5,
        .end   = 5,
        .flags = IORESOURCE_IRQ,
    }

};

static void s3c24xx_leds_release(struct device * dev)
{
}


static struct platform_device s3c24xx_leds_dev = {
    .name         = "mys3c24xx_leds",
    .id       = -1,
    .num_resources    = ARRAY_SIZE(s3c24xx_leds_resource),
    .resource     = s3c24xx_leds_resource,
    .dev = { 
    	.release = s3c24xx_leds_release, 
	},
};

static int s3c24xx_leds_dev_init(void)
{
	platform_device_register(&s3c24xx_leds_dev);
	return 0;
}

static void s3c24xx_leds_dev_exit(void)
{
	platform_device_unregister(&s3c24xx_leds_dev);
}

module_init(s3c24xx_leds_dev_init);
module_exit(s3c24xx_leds_dev_exit);

MODULE_LICENSE("GPL");

