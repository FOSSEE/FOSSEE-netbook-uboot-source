
#ifndef __MACH_WMT_IOMUX_H__
#define __MACH_WMT_IOMUX_H__

#include <linux/types.h>

#undef	WMT_PIN
#define WMT_PIN(__gp, __bit, __irq, __name)	__name,
enum iomux_pins {
	#include "iomux.h"
	IOMUX_MAX_PIN,
};

/* use gpiolib dispatchers */
#define gpio_get_value		__gpio_get_value
#define gpio_set_value		__gpio_set_value
#define gpio_cansleep		__gpio_cansleep

enum gpio_pulltype {
	GPIO_PULL_NONE = 0,
	GPIO_PULL_UP,
	GPIO_PULL_DOWN,
};

typedef struct _GPIO_ENV_ {
	int gpiono;
	int active;
} GPIO_ENV;

static inline int gpio_is_valid(int gpio)
{
	return (gpio >= 0 && gpio < IOMUX_MAX_PIN);
}

//extern int	gpio_request		(unsigned gpio);
extern void	gpio_free		(unsigned gpio);
extern int	gpio_get_value		(unsigned gpio);
extern void	gpio_set_value		(unsigned gpio, int value);
extern int	gpio_direction_input	(unsigned gpio);
extern int	gpio_direction_output	(unsigned gpio, int value);
extern int	gpio_setpull		(unsigned int gpio, enum gpio_pulltype pull);
extern int      parse_gpio_env          (char *name, GPIO_ENV *p_env);

#endif /* #ifndef __MACH_WMT_IOMUX_H__ */

