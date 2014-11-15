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
#include <asm/errno.h>

#include "../../../include/wmt_iomux.h"
#include "../../../include/common_def.h"

#include "../../wmt_battery.h"

struct mp2625_charger {
	union {
		struct {
			unsigned int cable_type:4;
			unsigned int current_sw_mode:1;
			unsigned int pc_charging:1;
		};
		uint32_t flag;
	};

	int full_pin;
	int full_level;
	int current_pin;
	int current_large_level;
};

static struct mp2625_charger charger = {
	.full_pin		= -1,
	.full_level		= -1,
	.current_pin		= -1,
	.current_large_level	= -1,
};

static inline int __set_large_current(void)
{
	if (gpio_is_valid(charger.current_pin))
		return gpio_direction_output(charger.current_pin,
					     charger.current_large_level);
	else
		return -EIO;
}

static inline int __set_small_current(void)
{
	if (gpio_is_valid(charger.current_pin))
		return gpio_direction_output(charger.current_pin,
					     !charger.current_large_level);
	else
		return -EIO;
}

static int parse_charger_param(void)
{
	static const char uboot_env[] = "wmt.charger.param";
	enum {
		idx_type,
		idx_full,
		idx_full_l,
		idx_curr,
		idx_curr_l,
		idx_max,
	};
	char *p, *endp;
	long ps[idx_max];
	int i = 0, base = 16;
	
	if (!(p = getenv((char *)uboot_env))) {
		printf("please setenv %s\n", uboot_env);
		return -EINVAL;
	}

	if ((strncmp(p, "mp2625:", 7)))
		return -EINVAL;
	p += 7;

	while (i < idx_max) {
		ps[i++] = simple_strtol(p, &endp, base);
		if (*endp == '\0')
			break;
		p = endp + 1;

		if (*p == '\0')
			break;
		base = 10;
	}
	charger.flag			= ps[0];
	charger.full_pin		= ps[1];
	charger.full_level		= ps[2];
	charger.current_pin		= ps[3];
	charger.current_large_level	= ps[4];

	printf("charger type %s, full_pin %d(%d), current_pin %d(%d)\n",
	       (charger.cable_type == CABLE_TYPE_DC) ? "DC" : "USB",
	       charger.full_pin, charger.full_level,
	       charger.current_pin, charger.current_large_level);

	if (gpio_is_valid(charger.full_pin))
		gpio_direction_input(charger.full_pin);
	__set_small_current();
	return 0;
}

static int mp2625_chg_init(void)
{
	return parse_charger_param();
}

static enum cable_type mp2625_cable_type(void)
{
	return charger.cable_type;
}

static int mp2625_check_full(void)
{
	if (gpio_is_valid(charger.full_pin))
		return (gpio_get_value(charger.full_pin) == charger.full_level);
	else
		return -EIO;
}

static int mp2625_pc_charging(void)
{
	return charger.pc_charging;
}

static int mp2625_event_proc(enum event_type event)
{
	switch (event) {
	case POWER_EVENT_DCDET_ADAPTER:
	case POWER_EVENT_CHARGING:
		return __set_large_current();

	case POWER_EVENT_DCDET_USBPC:
		return __set_small_current();

	case POWER_EVENT_DCDET_PLUGOUT:
		return __set_small_current();

	case POWER_EVENT_CHARGING_FULL:
		return 0;

	default:
		return -EINVAL;
	}
}

// export for extern interface.
struct charger_dev mp2625_charger_dev = {
	.name			= "mp2625",
	.init			= mp2625_chg_init,
	.cable_type		= mp2625_cable_type,
	.check_full		= mp2625_check_full,
	.event_proc		= mp2625_event_proc,
	.pc_charging		= mp2625_pc_charging,
};

