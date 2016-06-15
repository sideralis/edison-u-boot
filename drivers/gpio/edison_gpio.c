/*
 * edison_gpio.c
 *
 *  Created on: May 9, 2016
 *      Author: gautier
 */

#include <common.h>
#include <dm.h>
#include <asm/gpio.h>
#include <i2c.h>
#include <asm/arch/i2c.h>
#include <asm/io.h>

/* I2C registers address for U16 et U34 */
#define OUTPUT_PORT1_REG	0x3		/* Register to set GPIO as output */
#define CONF_PORT1_REG		0x7		/* Register to set a value to a GPIO */

/* Edison GPIO registers */
#define GPLR1				0xFF008008				/* Pin Level */
#define GPDR1				0xFF008020				/* Pin Direction */
#define GPSR1				0xFF008038				/* Output set */
#define GPCR1				0xFF008050				/* Output clear */

/* Edison Pad register */
#define GP_SSP_2_CLK_REG	0xFF0C1528				/* Pad setting */

struct edison_gpio_priv {
	/* TODO(sjg@chromium.org): Can we use a struct here? */
	void *base;	/* address of registers in physical memory */
};

static int edison_gpio_direction_input(struct udevice *dev, unsigned offset)
{
	printf("%s not implemented!\n",__func__);
	return 0;
}

static int edison_gpio_direction_output(struct udevice *dev, unsigned offset,
				       int value)
{
	uint8_t data;
	uint tmp;
	printf("%s\n",__func__);

	// Set ext mux: U16 IO1.3 to low to select GP40
	data = 0b11110111;
	i2c_write(U16_I2C_ADDR, CONF_PORT1_REG, 1, &data, 1);
	data = 0b00000000;
	i2c_write(U16_I2C_ADDR, OUTPUT_PORT1_REG, 1, &data, 1);

	// set external output: U34 IO1.5 to high
	data = 0b11011111;
	i2c_write(U34_I2C_ADDR, CONF_PORT1_REG, 1, &data, 1);
	data = 0b00100000;
	i2c_write(U34_I2C_ADDR, OUTPUT_PORT1_REG, 1, &data, 1);

	// set internal direction: gpio40 to output
	tmp = readl(GPDR1);
	tmp |= (1<<8);
	writel(tmp, GPDR1);

	// set internal value
	if (value == 0) {
		// set internal value: gpio40 to low
		writel((1<<8), GPCR1);
	} else if (value == 1) {
		// set internal value: gpio40 to high
		writel((1<<8), GPSR1);
	}

	return 0;
}
static int edison_gpio_get_value(struct udevice *dev, unsigned offset)
{
	printf("%s: not yet implemented\n",__func__);

	return 1;	// or 0
}

static int edison_gpio_set_value(struct udevice *dev, unsigned offset,
				 int value)
{
	printf("%s: not yet implemented\n",__func__);

	return 0;
}
static void edison_gpio_init()
{
	/* Select pin Mode 0 (gpio)*/
	writel(0x20, GP_SSP_2_CLK_REG);
}

static int edison_gpio_remove(struct udevice *dev)
{
	printf("%s: not yet implemented\n",__func__);

	return 0;
}

static int edison_gpio_probe(struct udevice *dev)
{
	struct edison_gpio_platdata *plat = dev_get_platdata(dev);
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	uc_priv->bank_name = plat->name;
	uc_priv->gpio_count = plat->count;

	edison_gpio_init();

	return 0;
}

static const struct dm_gpio_ops edison_gpio_ops = {
	.direction_input	= edison_gpio_direction_input,
	.direction_output	= edison_gpio_direction_output,
	.get_value		= edison_gpio_get_value,
	.set_value		= edison_gpio_set_value,
};


U_BOOT_DRIVER(edison_gpio_drv) = {
	.name	= "edison_gpio_drv",
	.id	= UCLASS_GPIO,
	.probe	= edison_gpio_probe,
	.remove = edison_gpio_remove,
	.ops	= &edison_gpio_ops,
	.priv_auto_alloc_size = sizeof(struct edison_gpio_priv),

};
