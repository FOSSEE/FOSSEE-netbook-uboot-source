#ifndef __BOARD_WMT_BATTERY_H__
#define __BOARD_WMT_BATTERY_H__

#include "../include/common_def.h"
#include "../include/wmt_pmc.h"

#define pr_debug(fmt, args...)	printf(fmt, ##args)
#define pr_err(fmt, args...)	printf(fmt, ##args)

enum cable_type {
	CABLE_TYPE_DC,
	CABLE_TYPE_USB,
	CABLE_TYPE_UNKNOWN,
};

enum event_type {
	POWER_EVENT_DCDET_ADAPTER,
	POWER_EVENT_DCDET_USBPC,
	POWER_EVENT_DCDET_PLUGOUT,
	POWER_EVENT_LCDON,
	POWER_EVENT_LCDOFF,
	POWER_EVENT_DISCHARGING,
	POWER_EVENT_CHARGING,
	POWER_EVENT_CHARGING_FULL,
	POWER_EVENT_UNKNOWN,
};

struct charger_dev {
	const char	*name;
	enum cable_type (*cable_type)	(void);
	int		(*init)		(void);
	int		(*check_full)	(void);
	int		(*pc_charging)	(void);
	int		(*event_proc)	(enum event_type event);
} __attribute__((aligned(4)));

struct battery_dev {
	const char	*name;
	int		is_gauge;
	int		(*init)		(void);
	int		(*get_capacity)	(void);
	int		(*get_voltage)	(void);
	int		(*get_current)	(void);
	int		(*check_batlow)	(void);
} __attribute__((aligned(4)));

static inline int wmt_is_dc_plugin(void)
{
	return (DCDET_STS_VAL & 0x00000100) ? 1 : 0;
}

static inline int prefixcmp(const char *str, const char *prefix)
{
	for (; ; str++, prefix++)
		if (!*prefix)
			return 0;
		else if (*str != *prefix)
			return (unsigned char)*prefix - (unsigned char)*str;
}

extern void do_wmt_poweroff(void);
extern int wmt_power_supply_init(void);

/* use below function if wmt_power_supply_init() successed */

extern int wmt_charger_small_current(void);
extern int wmt_charger_large_current(void);
extern int wmt_charger_cable_type(void);
extern int wmt_charger_pc_charging(void);
extern int wmt_charger_event_callback(enum event_type event);

extern int wmt_battery_get_capacity(void);
extern int wmt_battery_get_current(void);
extern int wmt_battery_get_voltage(void);
extern int wmt_battery_is_lowlevel(void);
extern int wmt_battery_is_charging_full(void);
extern int wmt_battery_is_gauge(void);

extern struct charger_dev g2214_charger_dev;
extern struct charger_dev mp2625_charger_dev;

extern struct battery_dev vt1603_battery_dev;
extern struct battery_dev ug31xx_battery_dev;
extern struct battery_dev ug3102_battery_dev;
extern struct battery_dev sp2541_battery_dev;
extern struct battery_dev bq_battery_dev;

extern int is_g2214_avail(void);
extern int g2214_pmic_init(void);
extern int pmic_init(void);


#endif /* #ifndef __WMT_CHARGER_H__ */
