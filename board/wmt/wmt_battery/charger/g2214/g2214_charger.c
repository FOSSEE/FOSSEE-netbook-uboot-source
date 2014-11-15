/*
 * Copyright (C) 2013  WonderMedia Technologies, Inc.  
 *
 * This program is free software; you can redistribute it and/or modify it
 * under  the terms of the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the License, or (at your
 * option) any later version.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <common.h>
#include <command.h>
#include <linux/ctype.h>
#include <asm/errno.h>

#include "../../../include/wmt_pmc.h"
#include "../../../include/wmt_spi.h"
#include "../../../include/wmt_clk.h"
#include "../../../include/wmt_gpio.h"
#include "../../../include/common_def.h"
#include "../../../include/wmt_iomux.h"

#include "../../wmt_battery.h"

#define I2C_M_WR                0x00
#define I2C_M_RD                0x01

struct i2c_msg_s {
	unsigned short addr;	// slave address
	unsigned short flags;	// flags
	unsigned short len;	// msg length
	unsigned char *buf;	// pointer to msg data
};

#define SECURITY_KEY	0x5A	//i2c read/write 

#define	G2214_I2C_SLAVE_ADDRESS	(0x12)

#define REG_A0     0x00
#define REG_A1     0x01
#define REG_A2     0x02
#define REG_A3     0x03
#define REG_A4     0x04
#define REG_A5     0x05
#define REG_A6     0x06
#define REG_A7     0x07
#define REG_A8     0x08
#define REG_A9     0x09
#define REG_A10    0x0A
#define REG_A11    0x0B
#define REG_A12    0x0C
#define REG_A13    0x0D

static struct g2214_charger {
	int		ac_online;
	int		batt_full;

	union {
		struct {
			unsigned int cable_type:4;
			unsigned int current_sw_mode:1;
			unsigned int pc_charging:1;
		};
		uint32_t flag;
	};

	int		iset_dcin;
	int		iset_vbus;
	int		vseta;
	int		iseta_small;
	int		iseta_large;
	int		safety_time;
	int		otg_power;
} g_charger;

extern int wmt_i2c_transfer(struct i2c_msg_s msgs[], int num, int adap_id);

static int g2214_read(struct g2214_charger *ch, u8 reg)
{
	int status;
	unsigned char ret;
	unsigned char data[2] = {
		reg,
		SECURITY_KEY,
	};

	struct i2c_msg_s msg_buf1[2] = {
		{ G2214_I2C_SLAVE_ADDRESS, I2C_M_WR, 1,	&data[0],},
		{ G2214_I2C_SLAVE_ADDRESS, I2C_M_RD, 1,	&ret, },
	};
	struct i2c_msg_s msg_buf2[2] = {
		{ G2214_I2C_SLAVE_ADDRESS, I2C_M_WR, 2, &data[0], },
		{ G2214_I2C_SLAVE_ADDRESS, I2C_M_RD, 1, &ret, },
	};

	if (reg < 0x80) {
		status = wmt_i2c_transfer(msg_buf1, 2, 3);
	} else {
		status = wmt_i2c_transfer(msg_buf2, 2, 3);
	}

	if (status > 0) {
		return ret;
	}
	return (-1);
}

static int g2214_write(struct g2214_charger *ch, u8 reg, int rt_value)
{
	int status;
	unsigned char data1[2];
	struct i2c_msg_s msg_buf[1] = {
		{
			.addr = G2214_I2C_SLAVE_ADDRESS,
			.flags = I2C_M_WR,
			.len = 2,
			.buf = data1,
		},
	};

	data1[0] = reg;
	data1[1] = rt_value;

	status = wmt_i2c_transfer(&msg_buf[0], 1, 3);
	if (status > 0) {
		return (0);
	}
	pr_debug(" ## i2c error %s, %d\n", __func__, __LINE__);
	return (-1);
}

static inline void g2214_enotg_config(struct g2214_charger *ch, int enable)
{
	int val = g2214_read(ch, REG_A8);
	if (enable)
		val |= BIT3;
	else
		val &= ~BIT3;
	pr_debug(" ## %s, %d: REG_A8 = 0x%x\n", __func__, __LINE__, val);
	g2214_write(ch, REG_A8, val);
}

static inline void g2214_vseta_init(struct g2214_charger *ch)
{
	int val, vseta;

	if (ch->vseta <= 4150)
		vseta = 0;
	else if (ch->vseta <= 4150)
		vseta = 1;
	else if (ch->vseta <= 4200)
		vseta = 2;
	else
		vseta = 3;

	val = g2214_read(ch, REG_A8);
	val &= ~(3 << 6);
	val |= vseta << 6;
	pr_debug(" ## %s, %d: REG_A8 = 0x%x\n", __func__, __LINE__, val);
	g2214_write(ch, REG_A8, val);
}

static inline void g2214_iset_config(struct g2214_charger *ch, int large_current)
{
	int iset_dcin, iset_vbus, iseta, charging_current;

	if (ch->iset_dcin <= 1000)
		iset_dcin = 0;
	else if (ch->iset_dcin <= 1500)
		iset_dcin = 1;
	else if (ch->iset_dcin <= 2000)
		iset_dcin = 2;
	else
		iset_dcin = 3;

	if (ch->iset_vbus <= 95)
		iset_vbus = 0;
	else if (ch->iset_vbus < 475)
		iset_vbus = 1;
	else if (ch->iset_vbus <= 950)
		iset_vbus = 2;
	else
		iset_vbus = 3;

	charging_current = (large_current) ? ch->iseta_large : ch->iseta_small;
	if (charging_current < 300 || charging_current > 1800)
		iseta = 2;
	else
		iseta = ((charging_current - 300) / 100);

	pr_debug(" ## %s, %d: REG_A5 = 0x%x\n",
		 __func__, __LINE__, iset_dcin << 6 | iset_vbus << 4 | iseta);
	g2214_write(ch, REG_A5, iset_dcin << 6 | iset_vbus << 4 | iseta);
}

static void g2214_endpm_config(struct g2214_charger *ch, int en)
{
	int val = g2214_read(ch, REG_A0);
	if (en)
		val |= BIT3;
	else
		val &= ~BIT3;
	pr_debug(" ## %s, %d: REG_A0 = 0x%x\n", __func__, __LINE__, val);
	g2214_write(ch, REG_A0, val);
}

static void g2214_safety_time_init(struct g2214_charger *ch)
{
	int val;
	int safety_time = ch->safety_time - 1;

	if (safety_time < 0)
		safety_time = 0;
	else if (safety_time > 16)
		safety_time = 15;

	val = g2214_read(ch, REG_A6);
	val &= (~(BIT4 | BIT5 | BIT6 | BIT7));
	val |= (safety_time << 4);
	pr_debug(" ## %s, %d: REG_A6 = 0x%x\n", __func__, __LINE__, val);
	g2214_write(ch, REG_A6, val);
}

static void g2214_regs_dump(struct g2214_charger *ch)
{
	int reg;
	for (reg = REG_A0; reg <= REG_A13; reg++)
		printf(" ## reg A%d: 0x%02x\n", reg, g2214_read(ch, reg));
}

static int g2214_reg_init(struct g2214_charger *ch)
{
	pr_debug(" ## %s, %d\n", __func__, __LINE__);
	g2214_write(ch, REG_A9,0xFF);
	g2214_write(ch, REG_A11,0xFF);
	g2214_write(ch, REG_A12,0x00);

	g2214_vseta_init(ch);
	g2214_safety_time_init(ch);
	g2214_enotg_config(ch, 0);
	g2214_iset_config(ch, 0);
	g2214_endpm_config(ch, 1);
	g2214_regs_dump(ch);
	return 0;
}

static int parse_charger_param(void)
{
	static const char uboot_env[] = "wmt.charger.param";
	enum {
		idx_flag,	// dex
		idx_iset_dcin,
		idx_iset_vbus,
		idx_vseta,
		idx_iseta_small,
		idx_iseta_large,
		idx_safety_time,
		idx_otg_power,
		idx_max,
	};
	char *p, *endp;
	long ps[idx_max];
	int i = 0, base = 16;
	struct g2214_charger *ch = &g_charger;

	if (!(p = getenv((char *)uboot_env)))
		return -EINVAL;

	if (strncmp(p, "g2214:", 6))
		return -EINVAL;
	p += 6;

	while (i < idx_max) {
		ps[i++] = simple_strtoul(p, &endp, base);
		if (*endp == '\0')
			break;
		p = endp + 1;
		if (*p == '\0')
			break;
		base = 10;
	}

	ch->flag	= ps[idx_flag];
	ch->iset_dcin	= ps[idx_iset_dcin];
	ch->iset_vbus	= ps[idx_iset_vbus];
	ch->vseta	= ps[idx_vseta];
	ch->iseta_small	= ps[idx_iseta_small];
	ch->iseta_large	= ps[idx_iseta_large];
	ch->safety_time	= ps[idx_safety_time];
	ch->otg_power	= ps[idx_otg_power];

	if (ch->cable_type != CABLE_TYPE_DC &&
	    ch->cable_type != CABLE_TYPE_USB) {
		printf("unkonw cable type %d\n", ch->cable_type);
		return -EINVAL;
	}

	printf("charger match g2214, %s cable, dcin %d mA, vbus %d mA, %d mV\n"
	       "charging current %d~%d mA %d hour, %s otg power\n",
	       (ch->cable_type == CABLE_TYPE_DC) ? "DC" : "USB",
	       ch->iset_dcin, ch->iset_vbus, ch->vseta,
	       ch->iseta_small, ch->iseta_large,
	       ch->safety_time, ch->otg_power ? "switch" : "no");

	return 0;
}

static int g2214_chg_init(void)
{
	if (parse_charger_param())
		return -EINVAL;

	return g2214_reg_init(&g_charger);
}

static enum cable_type g2214_cable_type(void)
{
	return g_charger.cable_type;
}

static int g2214_check_full(void)
{
	uint8_t a12 = g2214_read(&g_charger, REG_A12);
	return !!(a12 & 0x10);
}

static int g2214_pc_charging(void)
{
	return g_charger.pc_charging;
}

static int __set_small_current(void)
{
	g2214_iset_config(&g_charger, 0);
	return 0;
}

static int __set_large_current(void)
{
	g2214_iset_config(&g_charger, 1);
	return 0;
}

static int do_g2214(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int i;
	for (i = 0; i <= 13; i++)
		printf(" reg%02d 0x%02x\n", i, g2214_read(&g_charger, i));
	return 0;
}

U_BOOT_CMD(g2214,	1,	1,	do_g2214,
	   "g2214\n", NULL);

static int g2214_event_proc(enum event_type event)
{
	switch (event) {
	case POWER_EVENT_DCDET_ADAPTER:
	case POWER_EVENT_CHARGING:
		return __set_large_current();

	case POWER_EVENT_DCDET_USBPC:
		return __set_small_current();

	case POWER_EVENT_DCDET_PLUGOUT:
	case POWER_EVENT_DISCHARGING:
		return __set_small_current();

	case POWER_EVENT_CHARGING_FULL:
		return 0;
	default:
		return -EINVAL;
	}
}

static void parse_default_param(void)
{
	struct g2214_charger *ch = &g_charger;
	ch->flag	= 0;
	ch->iset_dcin	= 1500;
	ch->iset_vbus	= 1800;
	ch->vseta	= 4200;
	ch->iseta_small	= 400;
	ch->iseta_large	= 800;
	ch->safety_time	= 8;
	ch->otg_power	= 0;
}

int is_g2214_avail(void)
{
	return g2214_read(&g_charger, REG_A0);
}

int g2214_pmic_init(void)
{
	parse_default_param();
	g2214_reg_init(&g_charger);
	return 0;
}

struct charger_dev g2214_charger_dev = {
	.name		= "g2214",
	.init		= g2214_chg_init,
	.cable_type	= g2214_cable_type,
	.check_full	= g2214_check_full,
	.event_proc	= g2214_event_proc,
	.pc_charging	= g2214_pc_charging,
};

