/*
 * edison_i2c.c
 *
 *  Created on: Apr 27, 2016
 *      Author: gautier
 */

#include <common.h>
#include <i2c.h>
#include <asm/arch/i2c.h>
#include <asm/io.h>

/* Base address */
#define I2C1_BASE 		0xFF18B000

/* Register address */
#define I2C_ENABLE 		(I2C1_BASE+0x6c)
#define I2C_CON 		(I2C1_BASE+0x00)
#define I2C_TAR 		(I2C1_BASE+0x04)
#define I2C_DATA_CMD	(I2C1_BASE+0x10)
#define I2C_STATUS		(I2C1_BASE+0x70)
#define I2C_RXFLR 		(I2C1_BASE+0x78)

/* bit field */
#define MST_ACTIVITY_BIT	(1 << 5)
#define TFE_BIT				(1 << 2)

/* Commands */
#define READ			0x100
#define WRITE			0x000
#define STOP			0x200
#define RESTART			0x400

/*
 * MUX           / GPIO243 (linux) / I2C, low = write 0x22 0x3 0xf0, high = write 0x22 0x3 0xf8
 * OUTPUT		 / GPIO261 (linux) / I2C, low = write 0x23 0x3 0xdf, high = write 0x22 0x3 0xff
 */

static void edison_i2c_init(struct i2c_adapter *adap, int speed, int slaveadd)
{
	/* Configure i2c in master mode */
	writel(0x00, I2C_ENABLE);
	writew(0x65, I2C_CON);
	writew(0x20, I2C_TAR);
	writel(0x01, I2C_ENABLE);
}

static int edison_i2c_probe(struct i2c_adapter *adap, uchar chip)
{
	printf("%s: %i - not yet implemented\n",__func__, chip);
	return 0;
}

static int edison_i2c_read(struct i2c_adapter *adap, uchar chip, uint addr,
			   int alen, uchar *buffer, int len)
{
	uchar val;	/* Data received */
	int tmp;	/* Number of data received */
	int i;

	/* Update address of target */
	writel(0x00, I2C_ENABLE);
	writew(chip, I2C_TAR);
	writel(0x01, I2C_ENABLE);

	/* Read data */
	writew(addr | WRITE, I2C_DATA_CMD);
	writew(addr | READ | STOP, I2C_DATA_CMD);
	do {
		tmp = readl(I2C_RXFLR);
	} while (tmp == 0);

	/* Read all data */
	i = 0;
	while (tmp != 0) {
		val = readw(I2C_DATA_CMD);
		/* But save only the len first one */
		if (i<len)
			buffer[i++] = val;
		tmp -= 1;
	}
	return 0;
}
static int edison_i2c_write(struct i2c_adapter *adap, uchar chip, uint addr,
			    int alen, uchar *buffer, int len)
{
	volatile uint tmp;

	if (len != 1) {
		printf("%s: Can write only one data!\n",__func__);
		return -1;
	}

	/* Update address of target */
	writel(0x00, I2C_ENABLE);
	writew(chip, I2C_TAR);
	writel(0x01, I2C_ENABLE);

	/* Send our unique data */
	writew(addr | WRITE, I2C_DATA_CMD);
	writew(buffer[0] | WRITE | STOP, I2C_DATA_CMD);
	
	/* Wait for end of transfer */
	do {
		tmp = readl(I2C_STATUS);
		tmp = tmp & (MST_ACTIVITY_BIT | TFE_BIT);
	} while (tmp != TFE_BIT);

	return 0;
}
static uint edison_i2c_setspeed(struct i2c_adapter *adap, uint speed)
{
	printf("%s: not yet implemented\n",__func__);
	return 0;
}

U_BOOT_I2C_ADAP_COMPLETE(edison_i2c1,
		edison_i2c_init,
		edison_i2c_probe,
		edison_i2c_read,
		edison_i2c_write,
		edison_i2c_setspeed,
		CONFIG_SYS_I2C_SPEED,
		0,
		0)
