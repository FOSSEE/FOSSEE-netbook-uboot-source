/*
 * ==========================================================================
 *
 *       Filename:  lcd-spi.c
 *
 *    Description:
 *
 *        Version:  0.01
 *        Created:  Thursday, December 27, 2012 02:50:05 HKT
 *
 *         Author:  Sam Mei (),
 *        Company:
 *
 * ==========================================================================
 */

#include <common.h>
#include <command.h>
#include <asm/types.h>
#include <asm/errno.h>
#include <asm/byteorder.h>

#include "../../../board/wmt/include/wmt_spi.h"
#include "../../../board/wmt/include/wmt_iomux.h"

#include "../lcd.h"

#define CHIPSELECT	0

#ifndef SPI_GPIO
static void spi_ctrl_9bit_tx(u8 val, int cmd_data)
{
	uint8_t buf[2],rbuf[2];

	if (cmd_data)
		buf[0] = (val >> 1) | BIT7;
	else
		buf[0] = (val >> 1) & 0x7f;

	buf[1] = (val << 7);

	spi_write_then_read_data(buf, rbuf, sizeof(buf), SPI_MODE_1, CHIPSELECT);
}

#else

#define CLK	WMT_PIN_GP12_SPI0CLK
#define MOSI	WMT_PIN_GP12_SPI0MOSI
#define MISO	WMT_PIN_GP12_SPI0MISO
#define SS	WMT_PIN_GP12_SPI0SS0

static void spi_gpio_9bit_tx(u8 val, int cmd_data)
{
	int i;

	// CS
	gpio_direction_output(SS, 1);
	gpio_direction_output(SS, 0);

	// D/C
	gpio_direction_output(MOSI, cmd_data);
	gpio_direction_output(CLK, 0);
	gpio_direction_output(CLK, 1);

	// 8-bit value
	for (i = 7; i >= 0; i--) {
		gpio_direction_output(MOSI, !!(val & (1 << i)));
		gpio_direction_output(CLK, 0);
		gpio_direction_output(CLK, 1);
	}

	// CS
	gpio_direction_output(SS, 1);
}
#endif

static inline void spi_9bit_tx(u8 val, int cmd_data)
{
#ifdef SPI_GPIO
	spi_gpio_9bit_tx(val, cmd_data);
#else
	spi_ctrl_9bit_tx(val, cmd_data);
#endif
}

static int ts8224b_cmd(u8 cmd)
{
	spi_9bit_tx(cmd, 0);
	return 0;
}

static int ts8224b_data(u8 data)
{
	spi_9bit_tx(data, 1);
	return 0;
}

/*
 * setenv wmt.lcd.setup 0
 * setenv wmt.display.param 2:0:24:1024:600:60
 * setenv wmt.display.tmr 30000:0:78:78:480:78:4:60:800:60
 */
static int ts8224b_init(void)
{

	static uint16_t settings[] = {
		#include "ts8224b.h"
	};
	int i;

	printf(" ## %s \n", __FUNCTION__);
	for (i = 0; i < ARRAY_SIZE(settings); i += 2) {
		ts8224b_cmd(settings[i] >> 8);
		ts8224b_data(settings[i+1]);
	}

	ts8224b_cmd(0x11);
	mdelay(120);

	ts8224b_cmd(0x29);
	mdelay(50);
	ts8224b_cmd(0x2c);
	return 0;
}

static int i2c_write(int i2c_idx, uint8_t reg, uint8_t value)
{
	unsigned char data[2] = { reg, value };
	struct i2c_msg_s msg[1] = {
		{
		.addr = 0xE0 >> 1,
		.flags = I2C_M_WR,
		.len = 2,
		.buf = data,
		},
	};

	if (wmt_i2c_transfer(&msg[0], 1, i2c_idx) > 0)
		return 0;

	printf(" ## lcd i2c1 write error %s, %d\n", __func__, __LINE__);
	return -1;
}

static uint8_t init_data[] = {
	0x4b, 0x01,
	0x0c, 0x01,
	0x05, 0x03,
	0x41, 0x03,
	0x10, 0x06,
	0x11, 0xE0,
	0x12, 0x00,
	0x13, 0x3C,
	0x14, 0x06,
	0x15, 0x40,
	0x16, 0x03,
	0x17, 0x9E,
	0x18, 0x00,
	0x19, 0x10,
	0x1a, 0x03,
	0x1b, 0x84,
	0x1c, 0x80,
	0x1d, 0x0A,
	0x1e, 0x80,
	0x1f, 0x06,
	0x3c, 0x17,
	0x3e, 0x16,
	0x36, 0x00,
	0x31, 0x00,
	0x35, 0x41,
	0x30, 0xB0,
	0x30, 0xB1,
	0x00, 0x0B,
};

static int chunghwa_init(void)
{
	int i;

	printf(" ## %s, %d\n", __FUNCTION__, __LINE__);
	for (i = 0; i < ARRAY_SIZE(init_data); i += 2) {
		i2c_write(1, init_data[i], init_data[i+1]);
	}

	return 0;
}

enum {
	LCD_SETUP_TS8224B,
	LCD_SETUP_CHUNGHWA,
	LCD_SETUP_MAX,
};

int lcd_spi_setup(void)
{
	int id;
	char *s = getenv("wmt.lcd.setup");

	if (!s)
		return -ENODEV;

	id = simple_strtoul(s, NULL, 10);
	switch (id) {
	case LCD_SETUP_TS8224B:
		return ts8224b_init();
	case LCD_SETUP_CHUNGHWA:
		return chunghwa_init();
	default:
		return -EINVAL;
	}
}

