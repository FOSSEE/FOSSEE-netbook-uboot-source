/*
 * Copyright (c) 2014  WonderMedia Technologies, Inc.  
 *
 * This program is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.  
 *
 * WonderMedia Technologies, Inc.
 * 10F, 529, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C.  
 */

#include <common.h>
#include <command.h>
#include <asm/errno.h>

#include "../include/wmt_iomux.h"
#include "wmt_battery.h"

#define return_val_if_failed(val, expr)					\
do {									\
	if (!(expr)) {							\
		printf(" %s, %d, Warnning: " #expr " failed.\n",	\
		       __FUNCTION__, __LINE__);				\
		return val;						\
	}								\
} while(0)

struct battery_param {
	struct battery_dev *bat_dev;
};

struct charger_param {
	struct charger_dev *chg_dev;
};

static struct battery_param battery_param;
static struct charger_param charger_param;

static int full_by_hw = 0;

static struct battery_dev *battery_devices[] = {
	&vt1603_battery_dev,
	&ug31xx_battery_dev,
	&ug3102_battery_dev,
	&sp2541_battery_dev,
	&bq_battery_dev,
	NULL,
};

static struct charger_dev *charger_devices[] = {
	&mp2625_charger_dev,
	&g2214_charger_dev,
	NULL,
};

static int parse_charger_param(struct charger_param *cp)
{
	static const char uboot_env[] = "wmt.charger.param";
	struct charger_dev **dev = &charger_devices[0];
	char *env;
	
	if (!(env = getenv((char *)uboot_env))) {
		printf("please setenv %s\n", uboot_env);
		return -EINVAL;
	}

	while (*dev) {
		if ((*dev)->name &&
		    !strncmp((*dev)->name, env, strlen((*dev)->name)))
			break;
		++dev;
	}

	if (!(*dev))
		return -EINVAL;

	cp->chg_dev = *dev;
	printf("Charger matched: %s\n", cp->chg_dev->name);
	return 0;
}

static int parse_battery_param(struct battery_param *bp)
{
	static const char uboot_env[] = "wmt.battery.param";
	struct battery_dev **dev = &battery_devices[0];
	char *env;
	
	if (!(env = getenv((char *)uboot_env))) {
		printf("please setenv %s\n", uboot_env);
		return -EINVAL;
	}

	while (*dev) {
		if ((*dev)->name &&
		    !strncmp((*dev)->name, env, strlen((*dev)->name)))
			break;
		++dev;
	}

	if (!(*dev))
		return -EINVAL;

	bp->bat_dev = *dev;
	printf("Battery matched: %s\n", bp->bat_dev->name);
	return 0;
}

static int parse_battery_full_param(void)
{
	static const char uboot_env[] = "wmt.bat.hwfull";
	char *env;
	
	if (!(env = getenv((char *)uboot_env))) {
		full_by_hw = 0;
		return -EINVAL;
	}

	full_by_hw = simple_strtol(env, NULL, 10);
	printf("full_by_hw %d\n", full_by_hw);
	return 0;
}

static struct {
	int charging;
	int full;
	int led_power;
	int led_gpio_level;
} charger_led;

static int parse_charger_led(void)
{
	enum {
		idx_id,
		idx_v1,
		idx_v2,
		idx_max,
	};
	long ps[idx_max];
	char *p, *endp;
	int i = 0;

	// initialize as invalid gpio
	charger_led.charging = -1;
	charger_led.full     = -1;
	charger_led.led_power = -1;

	p = getenv("wmt.charger.led");
	if (!p)
		return -EINVAL;

	while (i < idx_max) {
		ps[i++] = simple_strtol(p, &endp, 10);
		if (*endp == '\0')
			break;
		p = endp + 1;
		if (*p == '\0')
			break;
	}

	if (i != 3)
		return -EINVAL;

	switch (ps[idx_id]) {
	case 0:
		charger_led.led_power = ps[idx_v1];
		charger_led.led_gpio_level = ps[idx_v2];
		printf("charger led power: %d (%d)\n",
		       charger_led.led_power, charger_led.led_gpio_level);
		break;
	case 1:
		charger_led.charging = ps[idx_v1];
		charger_led.full     = ps[idx_v2];
		printf("charger led pin: charging %d, full %d\n",
		       charger_led.charging, charger_led.full);
		break;
	default:
		printf("no valid charger led found\n");
		return -EINVAL;
	}

	return 0;
}

int wmt_power_supply_init(void)
{
	struct charger_param *cp = &charger_param;
	struct battery_param *bp = &battery_param;
	static int inited = 0;

	if (inited)
		return 0;
	++inited;

	if (parse_charger_param(cp)) {
		pr_err("charger not found\n");
		return -ENODEV;
	}
	if (cp->chg_dev->init()) {
		pr_err("%s init failed\n", cp->chg_dev->name);
		return -EIO;
	}

	if (parse_battery_param(bp)) {
		pr_err("cattery not found\n");
		return -ENODEV;
	}
	if (bp->bat_dev->init()) {
		pr_err("%s init failed\n", bp->bat_dev->name);
		return -EIO;
	}

	parse_charger_led();
	parse_battery_full_param();

	return 0;
}

int pmic_init(void)
{
	if (is_g2214_avail() != -1)
		g2214_pmic_init();
	return 0;
}

static inline void led_enable(int gpio, int enable)
{
	if (gpio_is_valid(gpio)) {
		if (enable)
			gpio_direction_output(gpio, 1);
		else {
			gpio_direction_output(gpio, 0);

			/* walkaround for hardware bug.
			 * set gpio to input to let led off.
			 */
			gpio_direction_input(gpio);
		}
	}
}

static inline void led_power_enable(int enable)
{
	int gpio = charger_led.led_power;

	if (gpio_is_valid(gpio)) {
		if (enable)
			gpio_direction_output(gpio, charger_led.led_gpio_level);
		else
			gpio_direction_output(gpio, !charger_led.led_gpio_level);
	}
}

int wmt_charger_event_callback(enum event_type event)
{
	struct charger_dev *dev = charger_param.chg_dev;

	return_val_if_failed(-1, dev != NULL);
	return_val_if_failed(-1, dev->event_proc);

	printf(" ## %s, %d\n", __func__, event);

	switch (event) {
	case POWER_EVENT_DCDET_ADAPTER:
	case POWER_EVENT_CHARGING:
		led_power_enable(1);
		if (wmt_battery_is_charging_full()) {
			led_enable(charger_led.charging, 0);
			led_enable(charger_led.full, 1);
		} else {
			led_enable(charger_led.charging, 1);
			led_enable(charger_led.full, 0);
		}
		break;
	case POWER_EVENT_DCDET_USBPC:
		if (wmt_charger_pc_charging()) {
			led_power_enable(1);
			if (wmt_battery_is_charging_full()) {
				led_enable(charger_led.charging, 0);
				led_enable(charger_led.full, 1);
			} else
				led_enable(charger_led.charging, 1);
		} else {
			led_power_enable(0);
			led_enable(charger_led.charging, 0);
		}
		led_enable(charger_led.full, 0);
		break;
	case POWER_EVENT_DCDET_PLUGOUT:
	case POWER_EVENT_DISCHARGING:
		led_power_enable(0);
		led_enable(charger_led.charging, 0);
		led_enable(charger_led.full, 0);
		break;
	case POWER_EVENT_CHARGING_FULL:
		led_power_enable(1);
		led_enable(charger_led.charging, 0);
		led_enable(charger_led.full, 1);
		break;
	default:
		break;
	}

	return dev->event_proc(event);
}

int wmt_charger_cable_type(void)
{
	struct charger_dev *dev = charger_param.chg_dev;

	return_val_if_failed(-1, dev != NULL);
	return_val_if_failed(-1, dev->cable_type);

	return dev->cable_type();
}

int wmt_charger_pc_charging(void)
{
	struct charger_dev *dev = charger_param.chg_dev;

	return_val_if_failed(-1, dev != NULL);

	return (dev->pc_charging) ? dev->pc_charging() : 0;
}

int wmt_battery_get_capacity(void)
{
	struct battery_dev *dev = battery_param.bat_dev;
	static unsigned int ref_full = 0;
	int capacity;

	return_val_if_failed(-1, dev != NULL);
	return_val_if_failed(-1, dev->get_capacity);

	capacity = dev->get_capacity();

	if (!wmt_battery_is_gauge()) {

		if (full_by_hw) {
			if (wmt_battery_is_charging_full())
				return capacity;
			else if (capacity == 100)
				return 99;
		}

		if (capacity == 100 && !wmt_battery_is_charging_full()) {
			if (ref_full < 30 * 60) {
				capacity = 99;
				ref_full++;
			} else
				printf("ref full %d, 30 minutes, report full\n", ref_full);
		} else
			ref_full = 0;

		printf("-> %d, ref full %d\n", capacity, ref_full);
	}

	return capacity;
}

int wmt_battery_get_voltage(void)
{
	struct battery_dev *dev = battery_param.bat_dev;

	return_val_if_failed(-1, dev != NULL);
	return_val_if_failed(-1, dev->get_voltage);

	return dev->get_voltage();
}

int wmt_battery_get_current(void)
{
	struct battery_dev *dev = battery_param.bat_dev;

	return_val_if_failed(-1, dev != NULL);
	return_val_if_failed(-1, dev->get_current);

	return dev->get_current();
}

int wmt_battery_is_lowlevel(void)
{
	struct battery_dev *dev = battery_param.bat_dev;

	return_val_if_failed(-1, dev != NULL);
	return_val_if_failed(-1, dev->check_batlow);

	return dev->check_batlow();
}

int wmt_battery_is_charging_full(void)
{
	struct charger_dev *dev = charger_param.chg_dev;

	return_val_if_failed(-1, dev != NULL);
	return_val_if_failed(-1, dev->check_full);

	if (!wmt_is_dc_plugin())
		return -1;

	return dev->check_full();
}

int wmt_battery_is_gauge(void)
{
	struct battery_dev *dev = battery_param.bat_dev;

	return_val_if_failed(-1, dev != NULL);

	return dev->is_gauge;
}

static int do_batt(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	wmt_power_supply_init();

	printf(" voltage %d mV\n", wmt_battery_get_voltage());
	printf(" capacity %d\n", wmt_battery_get_capacity());
	printf(" dinc     %d\n", wmt_is_dc_plugin());

	return 0;
}

U_BOOT_CMD(
	batt,	1,	1,	do_batt,
	"batt  - print Board Info structure\n",
	NULL
);

