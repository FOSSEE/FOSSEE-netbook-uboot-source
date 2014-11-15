#include <common.h>
#include <command.h>
#include <linux/ctype.h>
#include <asm/errno.h>
#include "../../../include/i2c.h"
#include "../../wmt_battery.h"
#include "bq_battery_i2c.h"
#define I2C_BUS_ID 3

static int i2c_bus = I2C_BUS_ID;

static short bq_battery_read_voltage(void);
static short bq_battery_read_current(void);

static int bq_i2c_read(unsigned char reg, unsigned char *rt_value, unsigned int len)
{
	struct i2c_msg_s msg[1];
	unsigned char data[2];
	int err;

	msg->addr = BQ_I2C_DEFAULT_ADDR;
	msg->flags = 0;
	msg->len = 1;
	msg->buf = data;

	data[0] = reg;
	err = wmt_i2c_transfer(msg, 1, i2c_bus);

	if (err >= 0) {
		msg->len = len;
		msg->flags = I2C_M_RD; 
		msg->buf = rt_value;
		err = wmt_i2c_transfer(msg, 1, i2c_bus);
	}
	return err;
}

static int bq_batt_init(void)
{
	char *env, *p;
	char *endp;
	p = env = getenv("wmt.battery.param");
	if (!p)
		return -EINVAL;
	
	if (prefixcmp(p, "bq27xx"))
		return -EINVAL;

	p = strchr(env, ':');
	p++;
	i2c_bus = simple_strtol(p, &endp, 10);

	return 0;
}

static short bq_battery_read_capacity(void)
{
	short ret = 0;
	unsigned char value[2] = {0};
    bq_i2c_read(BQ_REG_NAC, value, 2);
	ret = value[1] << 8 | value[0];
	
	//printf("%s %d\n",__FUNCTION__,ret);
    return ret;	
}

static short bq_battery_read_temperature(void)
{
	short ret = 0;
	unsigned char value[2] = {0};
    bq_i2c_read(BQ_REG_TEMP, value, 2);
	ret = (value[1] << 8 | value[0]) - 2731;
    return ret;	
}

static unsigned short bq_battery_read_percentage(void)
{
	unsigned short ret = 0;
	unsigned char value[2];
	value[0] = 0;
	value[1] = 0;
    bq_i2c_read(BQ_REG_SOC, value, 2);
	ret = value[1] << 8 | value[0];
	printf("Current percentage: %d\%\n", ret);
	printf("Voltage: %dmV\n", bq_battery_read_voltage());
	printf("Current: %dmA\n", bq_battery_read_current());
	printf("temperature: %d\n", bq_battery_read_temperature());
	printf("capacity: %d\n", bq_battery_read_capacity());
	
	return ret;
}

static short bq_battery_read_voltage(void)
{
	short ret = 0;
	unsigned char value[2] = {0};
    bq_i2c_read(BQ_REG_VOLT, value, 2);
	ret = (value[1] << 8 | value[0]) * 2;
	
    return ret;	
}

static short bq_battery_read_current(void)
{
	short ret = 0;
	unsigned char value[2] = {0};
    bq_i2c_read(BQ_REG_AI, value, 2);
	ret = value[1] << 8 | value[0];
	
    return ret;	
}

static int bq_check_bl(void)
{
	int percentage = bq_battery_read_percentage();

	if (percentage < 0)
		return -1;
	return (percentage < 3);
}



struct battery_dev bq_battery_dev = {
		.name		= "bq27xx",
		.is_gauge	= 1,
		.init		= bq_batt_init,
		.get_capacity	= bq_battery_read_percentage,
		.get_voltage	= bq_battery_read_voltage,
		.get_current	= bq_battery_read_current,
		.check_batlow	= bq_check_bl,
};

