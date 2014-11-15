/*++
Copyright (c) 2014 WonderMedia Technologies, Inc.

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details. You
should have received a copy of the GNU General Public License along with this
program. If not, see http://www.gnu.org/licenses/>.

WonderMedia Technologies, Inc.
2014-06-20, HowayHuo, ShenZhen
--*/

/*--- History -------------------------------------------------------------------
*     DATE	    |	      AUTHORS	      |        DESCRIPTION
*   2014/06/20		    Howay Huo		  v1.0, First Release
*
*------------------------------------------------------------------------------*/

#include <common.h>
#include "../board/wmt/include/wmt_pmc.h"
#include "../board/wmt/include/wmt_iomux.h"
#include "wmt_display/wmt_display.h"
#include "wmt_display/minivgui.h"

/*
* Format:
*     setenv wmt.fastboot.key enable:powerkey:gpiono:active
* Example:
*     Press PowerKey and GPIO10(volume-) key in the same time to force enter fastboot
*     The GPIO10 is low level when pressing the volume- key
*
* setenv wmt.fastboot.key 1:1:10:0
*/
#define ENV_FASTBOOT_KEY       "wmt.fastboot.key"

/*
* setenv wmt.fastboot.checktime 1000
*/
#define ENV_FASTBOOT_CHECKTIME  "wmt.fastboot.checktime"

static char* CMD_LOAD_FASTBOOT_FILE_FROM_SDCARD = "fatload mmc 0 0 fastboot.enter";
static char* CMD_LOAD_FASTBOOT_FILE_FROM_UDISK  = "fatload usb 0 0 fastboot.enter";

typedef struct _FASTBOOT_KEY_ENV_ {
	int enable;     // 1: enable the function of pressing key to enter fastboot. 0: don't enable
	int powerkey;   // 1: press powerkey+gpiokey combination 0: don't press powerkey, only press gpiokey
	int gpiono;     // the gpio no of gpiokey
	int active;     // 1: the gpio level is 1 when the gpiokey is pressed. 0: the gpio level is 0 when the gpiokey is pressed
} FASTBOOT_KEY_ENV;

static FASTBOOT_KEY_ENV default_fastboot_key = {
	.enable   = 1,            // enable pressing key to enter fastboot
	.powerkey = 1,            // powerkey+gpiokey combination
	.gpiono = 10,             // Using GPIO10. GPIO10 is volume- key
	.active = 0               // the gpio level is 0 when the volume- key is pressed
};

extern int usb_plugin(void);
extern void udc_fastboot_exit(void);
extern int fastboot_loop(int check_usb_plugin);

void run_fastboot(int backlight_on)
{
	if(backlight_on)
		show_text_to_screen("Enter Fastboot Mode", 0xFF00);
	else
		show_text_to_screen_no_backlight("Enter Fastboot Mode", 0xFF00);

	printf("Enter Fastboot Mode\n");
	fastboot_loop(1);
}

static void set_pwm_duty(int duty_percent)
{
	int duty;

	if(!(g_display_vaild & DISPLAY_ENABLE))
		return;

	if (g_display_param.vout == VPP_VOUT_LCD) {
		if ((g_display_vaild & PWMDEFMASK) == PWMDEFTP) {
			if(duty_percent <= g_pwm_setting.duty) {
				lcd_blt_set_pwm(g_pwm_setting.pwm_no,
				                duty_percent, g_pwm_setting.period);

				mdelay(10);
			}
		} else {
			duty = (duty_percent * g_pwm_setting.period) / 100;
			if(duty <= g_pwm_setting.duty) {
				pwm_set_duty(g_pwm_setting.pwm_no, duty - 1);

				mdelay(10);
			}
		}
	}
}

static void enter_fastboot_mode(FASTBOOT_KEY_ENV *p_fastboot_key)
{
	char tmpbuf[50];
	int len;

	if(p_fastboot_key->powerkey)
		len = sprintf(tmpbuf, "Power key and GPIO%d key are pressed", p_fastboot_key->gpiono);
	else
		len = sprintf(tmpbuf, "GPIO%d key is long pressed", p_fastboot_key->gpiono);

	tmpbuf[len] = 0;

	printf("%s\n", tmpbuf);
	printf("Enter Fastboot Mode\n");

	/*
	Why set the pwm to 30%?
	If the MID no battery, the board maybe be reset by PMIC when light the backlight.
	Set pwm duty to 30% for avoiding the board is reset
	*/
	set_pwm_duty(30);

	show_2lines_text_to_screen(tmpbuf, "Enter Fastboot Mode", 0xFF00);

	fastboot_loop(0);
}

static int parse_fastboot_key_env(char *name, FASTBOOT_KEY_ENV *p_env)
{
 	enum
	{
		idx_enable,
		idx_powerkey,
		idx_gpiono,
		idx_active,
		idx_max
	};

	char *p;
	long ps[idx_max] = {0};
	char * endp;
	int i = 0;

	p = getenv(name);
	if (!p)
        	return -1;

   	while (i < idx_max) {
		ps[i++] = simple_strtoul(p, &endp, 0);

        	if (*endp == '\0')
            		break;
        	p = endp + 1;

        	if (*p == '\0')
			break;
	}

	p_env->enable = ps[0];
	p_env->powerkey = ps[1];
	p_env->gpiono = ps[2];
	p_env->active = ps[3];

	if(i != 4) {
		printf("parse_fastboot_key_env: wrong env num in %s\n", name);
		return -1;
	}

	if(p_env->enable == 0)
		return 0;

	if(gpio_is_valid(p_env->gpiono))
		return 0;
	else {
		printf("parse_fastboot_key_env: wrong gpio no in %s\n", name);
		return -1;
	}
}

#define PERIOD_OF_CHECKING_PRESS_KEY  100  // 100 ms  //How long time to check whether the keys are pressed
#define CHECK_TIME_OF_PRESSING_KEY    1000 // 1s      //The Total check time

/*
* Function: enter_fastboot_by_press_key
* Purpose:
*     long press power key + volume- key to force enter fastboot
*     or long press volume- key to force enter fastboot
*
* Return:
* 0 : success to enter fastboot
* -1: fail to enter fastboot
*
*/
int enter_fastboot_by_press_key(void)
{
	int i, ret;
	int gpio_key_pressed;
	unsigned int checktime, checkcount;
	char *s;
	FASTBOOT_KEY_ENV fastboot_key;

	ret = parse_fastboot_key_env(ENV_FASTBOOT_KEY, &fastboot_key);
	if(ret)
		memcpy(&fastboot_key, &default_fastboot_key, sizeof(FASTBOOT_KEY_ENV));

	if(fastboot_key.enable == 0)
		return -1;

	if(fastboot_key.powerkey == 1) {
		if(!(PMPB_VAL & BIT24))
			return -1;
	}

	s = getenv(ENV_FASTBOOT_CHECKTIME);
	if(s) {
		checktime = simple_strtoul(s, NULL, 0);
		printf("Manually set fastboot checktime: %u ms\n", checktime);
	} else
		checktime = CHECK_TIME_OF_PRESSING_KEY;

	checkcount = checktime / PERIOD_OF_CHECKING_PRESS_KEY;
	if(checkcount == 0)
		return -1;

	if(fastboot_key.active != 0)
		fastboot_key.active = 1;

	gpio_direction_input(fastboot_key.gpiono);
	gpio_setpull(fastboot_key.gpiono, fastboot_key.active? GPIO_PULL_DOWN : GPIO_PULL_UP);

	for(i = 0; i < checkcount; i++) {
		if(i != 0)
			udelay(PERIOD_OF_CHECKING_PRESS_KEY * 1000);

		if(fastboot_key.powerkey == 1) {
			if(!(PMPB_VAL & BIT24))
				return -1;
		}

		gpio_key_pressed = (gpio_get_value(fastboot_key.gpiono) == fastboot_key.active);
		if(gpio_key_pressed == 0)
			return -1;
	}

	udc_fastboot_exit();
	enter_fastboot_mode(&fastboot_key);

	return 0;
}

/*
* Function: enter_fastboot_by_special_file
* Purpose:
*     If the "fastboot.enter" file in the root directory of SD Card or U Disk,
*     and the USB Adapter is plugined, force enter fastboot
*
* Parameter:
*    in_sdcard: 1: the "fastboot.enter" file in SD Card
*               0: the "fastboot.enter" file in U Disk
*
* Return:
* 0 : success to enter fastboot
* -1: fail to enter fastboot
*
*/
int enter_fastboot_by_special_file(int in_sdcard)
{
	int len;
	char tmpbuf[50] = {0};

	if(usb_plugin() == 0)
		return -1;

	if(in_sdcard) {
		if(run_command(CMD_LOAD_FASTBOOT_FILE_FROM_SDCARD, 0) == -1)
			return -1;
	} else {
		if(run_command(CMD_LOAD_FASTBOOT_FILE_FROM_UDISK, 0) == -1)
			return -1;
	}

	len = sprintf(tmpbuf, "fastboot.enter in %s and USB Adapter Plugin", in_sdcard ? "SDCard" : "UDISK");
	tmpbuf[len] = 0;

	printf("%s\n", tmpbuf);
	printf("Enter Fastboot Mode\n");

	set_pwm_duty(30);

	show_2lines_text_to_screen(tmpbuf, "Enter Fastboot Mode", 0xFF00);

	udc_fastboot_exit();
	fastboot_loop(0);

	return 0;
}

