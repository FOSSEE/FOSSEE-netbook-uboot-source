#include <common.h>
#include <command.h>
#include <linux/ctype.h>
#include <asm/errno.h>
#include "../../../include/i2c.h"
#include "../../wmt_battery.h"
#include "sp2541_battery.h"

static int i2c_bus = I2C_BUS_ID;

static int sp2541_read(u8 cmd, u8 reg, u8 *rt_value, unsigned int len)
{
	struct i2c_msg_s msg[2];
	unsigned char data[2] = {cmd, reg};

	msg[0].addr = SP2541_I2C_ADDR;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	msg[1].addr = SP2541_I2C_ADDR;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = rt_value;

	return wmt_i2c_transfer(msg, 2, i2c_bus);
}

static int sp2541_write(u8 cmd, u8 reg, u8 const buf)
{
	struct i2c_msg_s msg;
	char data[3] = {cmd, reg, buf};

	msg.addr = SP2541_I2C_ADDR;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = data;
	return wmt_i2c_transfer(&msg, 1, i2c_bus);
}

static int EEPROM_read_byte(int addr, u8 *data)
{
	u8 addr_hi, addr_lo, i;
	u8 tmp = -1;
	addr_hi = (addr >> 8) & 0xFF;
	addr_lo = addr & 0xFF;
	if (sp2541_write(EEPROM_WRITE_CMD, 0x00, addr_lo) < 0)
		return -1;
	if (sp2541_write(EEPROM_WRITE_CMD, 0x01, addr_hi) < 0)
		return -1;
	if (sp2541_write(EEPROM_WRITE_CMD, 0x03, 0x06) < 0)
		return -1;
	for (i=0; i<10; i++) {
		sp2541_read(EEPROM_READ_CMD, 0x03, &tmp, 1);
		if (tmp == 0)
			break;
	}
	if (i == 10)
		return -1;
	if (sp2541_read(EEPROM_READ_CMD, 0x02, data, 1) < 0)
		return -1;
	return 0;
}

static int EEPROM_write_byte(int addr, u8 data)
{
	u8 addr_hi, addr_lo, i;
	u8 tmp = -1;
	addr_hi = (addr >> 8) & 0xFF;
	addr_lo = addr & 0xFF;
	if (sp2541_write(EEPROM_WRITE_CMD, 0x00, addr_lo) < 0)
		return -1;
	if (sp2541_write(EEPROM_WRITE_CMD, 0x01, addr_hi) < 0)
		return -1;
	if (sp2541_write(EEPROM_WRITE_CMD, 0x02, data) < 0)
		return -1;
	if (sp2541_write(EEPROM_WRITE_CMD, 0x03, 0x05) < 0)
		return -1;
	for (i=0; i<10; i++) {
		sp2541_read(EEPROM_READ_CMD, 0x03, &tmp, 1);
		if (tmp == 0)
			break;
	}
	if (i == 10)
		return -1;
	return 0;
}

static int sp2541_init(void)
{
	char *env, *p;
	char *endp;
	p = env = getenv("wmt.battery.param");
	if (!p)
		return -EINVAL;
	
	if (prefixcmp(p, "sp2541"))
		return -EINVAL;

	p = strchr(env, ':');
	p++;
	i2c_bus = simple_strtol(p, &endp, 10);
	if (*endp != ':')
		return -EINVAL;

	return 0;
}

static int sp2541_get_capacity(void)
{
	unsigned char buf[2];
	int ret;

	ret = sp2541_read(RAM_READ_CMD,	SP2541_REG_SOC, buf, 2);
	if (ret<0) {
		printf("error reading capacity\n");
		return ret;
	}
	ret = buf[0] | buf[1] << 8;
	printf("Current capacity: %d\%\n", ret);
	return ret;
}

static int sp2541_get_voltage(void)
{
	unsigned char buf[2];
	int ret;

	ret = sp2541_read(RAM_READ_CMD,	SP2541_REG_VOLT, buf, 2);
	if (ret<0) {
		printf("error reading capacity\n");
		return ret;
	}
	return buf[0] | buf[1] << 8;
}

static int sp2541_get_current(void)
{
	unsigned char buf[2];
	int ret;

	ret = sp2541_read(RAM_READ_CMD,	SP2541_REG_AI, buf, 2);
	if (ret<0) {
		printf("error reading capacity\n");
		return ret;
	}
	return buf[0] | buf[1] << 8;
}

static int sp2541_check_bl(void)
{
	int capacity;

	capacity = sp2541_get_capacity();
	if (capacity < 0)
		return -1;
	return (capacity < 3);
}


struct battery_dev sp2541_battery_dev = {
		.name		= "sp2541",
		.is_gauge	= 1,
		.init		= sp2541_init,
		.get_capacity	= sp2541_get_capacity,
		.get_voltage	= sp2541_get_voltage,
		.get_current	= sp2541_get_current,
		.check_batlow	= sp2541_check_bl,
};


static int do_sp2541(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int i, j;
	int capacity,voltage,current;
	u8 data_buf;
	int eeprom_data;
	int ret;

	if (sp2541_init())
		return 0;
	capacity = sp2541_get_capacity();
	voltage = sp2541_get_voltage();
	current = sp2541_get_current();
	printf("capacity = %d\n", capacity);
	printf("voltage = %d\n", voltage);
	printf("current = %d\n", current);

	/*for (i = 0; i < ARRAY_SIZE(default_table); i++) {
		printf("{0x%x, %d, %x}\n", default_table[i].addr, default_table[i].len, default_table[i].data);
		for (j = 0; j < default_table[i].len; j++) {
			data_buf = (default_table[i].data >> (8*(default_table[i].len-j-1))) & 0xFF;
			ret = EEPROM_write_byte(default_table[i].addr + j, data_buf);
			if (ret<0) {
				printf("error write eeprom.\n");
				return 0;
			}
		}
	}*/
	for (i = 0; i < ARRAY_SIZE(default_table); i++) {
		eeprom_data = 0;
		for (j = 0; j < default_table[i].len; j++) {
			ret = EEPROM_read_byte(default_table[i].addr + j, &data_buf);
			if (ret<0) {
				printf("error read eeprom.\n");
				return 0;
			}
			eeprom_data = (eeprom_data << 8*j) | data_buf;
		}
		//printf("{0x%x, %d, %x}\n", default_table[i].addr, default_table[i].len, default_table[i].data);
		printf("0x%x: %x\n", default_table[i].addr, eeprom_data);
	}
	return 0;
}

U_BOOT_CMD(
	sp2541,	1,	1,	do_sp2541,
	"sp2541\n",
	NULL
);
