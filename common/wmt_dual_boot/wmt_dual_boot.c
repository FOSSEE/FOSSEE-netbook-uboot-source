/*++
	WMT Dual Boot Driver

	Copyright (c) 2014  WonderMedia Technologies, Inc.

	This program is free software: you can redistribute it and/or modify it under the
	terms of the GNU General Public License as published by the Free Software Foundation,
	either version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
	PARTICULAR PURPOSE.  See the GNU General Public License for more details.
	You should have received a copy of the GNU General Public License along with
	this program.  If not, see <http://www.gnu.org/licenses/>.

	WonderMedia Technologies, Inc.
	2014-2-25, HowayHuo, ShenZhen
--*/
/*--- History -------------------------------------------------------------------
*     DATE	    |	      AUTHORS	      |        DESCRIPTION
*   2014/2/25		    Howay Huo		  v1.0, First Release
*
*
*------------------------------------------------------------------------------*/

/*----------------------------------------- Logo Layout in Nand Flash ----------------------------------------
logo layout: android-logo <-> charging-logo <-> dualboot-android-logo <-> dualboot-ubuntu-logo <-> ubuntu-logo
1. android logo
2. charging logo
3. dualboot android logo
4. dualboot ubuntu logo
5. ubuntu logo

Merge logo:
  cat u-boot-logo.bmp charge-logo.bmp dualboot-android-logo.bmp dualboot-ubuntu-logo.bmp ubuntu-logo.bmp  > logo.out

Burn logo.out to Nand Flash
--------------------------------------------------------------------------------------------------------------*/

#include <common.h>
#include <command.h>
#include <bmp_layout.h>

#include "../wmt_display/minivgui.h"
#include "../../board/wmt/include/wmt_iomux.h"

#include "wmt_dual_boot.h"

/*********************************** Constant Macro **********************************/
#define DUAL_BOOT   "Dual Boot"

#define TEXT_NO_KEY          "No key to choose system"
#define TEXT_PRESS_POWER_KEY "Press power key to choose system"
#define TEXT_PRESS_VOLUME_KEY  "Press volume key to choose system"
#define TEXT_PRESS_POWER_VOLUME_KEY "Press power or volume key to choose system"
#define TEXT_DETAIL_COUNT_DOWN "The chose system will startup in seconds:  "
#define TEXT_BRIEF_COUNT_DOWN "System will startup in seconds:  "

/*********************************** External Variable ***************************/
extern int g_tf_boot;

/**************************** Data Type and Local Variable ***************************/
static int dualboot_logo_init_ok;
static int dualboot_key_init_ok;
static int mmc_init_ok;
static int text_is_display;
static int text_need_always_display;
static int text_display_level = 3;

static UBOOT_LOGO uboot_logo[LOGO_MAX_NUM];
static LOGO_TYPE current_logo_type = LOGO_MAX_NUM;
static KEY_SELECT key_select = {
	.use_power_key = 1,
	.use_volume_key = 1,
	.use_usb_kpad = 0,
	.countdown_seconds = 3,
	.volume_plus_gpiono = WMT_PIN_GP1_GPIO15,
	.volume_minus_gpiono = WMT_PIN_GP62_WAKEUP2,
	.volume_key_active = 0,
};

static TEXT_COORDINATE countdown_seconds_pos;

/*
* For example: setenv wmt.dualboot.key 1:1:0:3:8:10:0
*/
#define ENV_DUALBOOT_KEY "wmt.dualboot.key"

/*
* For example: setenv wmt.dualboot.os android
*              setenv wmt.dualboot.os ubuntu
*/
#define ENV_DUALBOOT_OS "wmt.dualboot.os"

/*
* For example: setenv wmt.dualboot.infolevel 3
*
* 0: don't display any text information to screen
* 1: display which key is pressed to choose system in the screen
* 2: display count down seconds to the screen
* 3: display all info to the screen
*/
#define ENV_DUALBOOT_INFOLEVEL  "wmt.dualboot.infolevel"
/*********************************** External Function ***************************/
extern void *arm_memset(void *s, int c, size_t count);
extern int display_show(int clearFB);
extern int WMTAccessNandEarier(unsigned long long naddr, unsigned int maddr,
	unsigned int size, int write);
extern int mv_le_to_cpu(char *buf, int size);

/********************************** Function declare *********************************/
#undef DEBUG
//#define DEBUG  //if you need see the debug info, please define it.

#undef DBG

#ifdef DEBUG
#define DBG(fmt, args...) printf("[" DUAL_BOOT "] " fmt , ## args)
#else
#define DBG(fmt, args...)
#endif

#define INFO(fmt, args...) printf("[" DUAL_BOOT "] " fmt , ## args)
#define ERROR(fmt,args...) printf("[" DUAL_BOOT "]Error: " fmt , ## args)
#define WARNING(fmt,args...) printf("[" DUAL_BOOT "]Warning: " fmt , ## args)

static int init_dualboot_logo(int force, LOGO_TYPE logo_type)
{
	char *p_android_logosize, *p_ubuntu_logosize, *p_charging_logosize;
	char *p_dualboot_android_logosize, *p_dualboot_ubuntu_logosize;
	char *p_naddr_env, *p_maddr_env;
	unsigned char *p_maddr;
	int android_logosize = 0, ubuntu_logosize= 0, charging_logosize= 0;
	int dualboot_android_logosize = 0, dualboot_ubuntu_logosize = 0, totalsize = 0x800000;
	unsigned int naddr, maddr = 0x500000;
	bmp_header_t *header;
	int ret;

	if(!g_tf_boot) {
		/*
			Load All logo to Memory from Nand Flash
			Only Load All logo to memory once unless 'force' is set
		*/

		if(force) {
			dualboot_logo_init_ok = 0;
			arm_memset(uboot_logo, 0, sizeof(uboot_logo));
		}

		if(dualboot_logo_init_ok)
			return 0;

		if ((p_naddr_env = getenv("wmt.nfc.mtd.u-boot-logo")))
			naddr = simple_strtoul(p_naddr_env, NULL, 16);
		else {
			ERROR("wmt.nfc.mtd.u-boot-logo is not set\n");
			//show_text_to_screen("wmt.nfc.mtd.u-boot-logo is not set", 0xFF00);
			return -1;
		}

		if ((p_maddr_env = getenv("wmt.display.logoaddr")))
			maddr = simple_strtoul(p_maddr_env, NULL, 16);
		else {
			maddr = 0x500000;
			WARNING("wmt.display.logoaddr is not set\n");
			INFO("logoaddr default set to 0x500000");
		}

		if( (p_android_logosize = getenv("wmt.logosize.uboot"))
			&& (p_ubuntu_logosize = getenv("wmt.logosize.ubuntu"))
			&& (p_charging_logosize = getenv("wmt.logosize.charge"))
			&& (p_dualboot_android_logosize = getenv("wmt.logosize.dualboot.android"))
			&& (p_dualboot_ubuntu_logosize = getenv("wmt.logosize.dualboot.ubuntu")) ) {

			/*
				use the logosize which the u-boot parameter appointed
			*/
		  	android_logosize = simple_strtoul(p_android_logosize, NULL, 0);
			ubuntu_logosize = simple_strtoul(p_ubuntu_logosize, NULL, 0);
			charging_logosize = simple_strtoul(p_charging_logosize, NULL, 0);
			dualboot_android_logosize = simple_strtoul(p_dualboot_android_logosize, NULL, 0);
			dualboot_ubuntu_logosize = simple_strtoul(p_dualboot_ubuntu_logosize, NULL, 0);

			if(android_logosize != 0 && ubuntu_logosize != 0 && charging_logosize != 0
				&& dualboot_android_logosize != 0 && dualboot_ubuntu_logosize != 0) {

				totalsize = android_logosize + ubuntu_logosize + charging_logosize
					+ dualboot_android_logosize + dualboot_ubuntu_logosize;


			} else {
				if(android_logosize == 0) {
					ERROR("'wmt.logosize.uboot' is wrong\n");
					//show_text_to_screen("Can Not show logo. 'wmt.logosize.uboot' is wrong", 0xFF00);
					return -1;
				}

				if(ubuntu_logosize == 0) {
					ERROR("'wmt.logosize.ubuntu' is wrong\n");
					//show_text_to_screen("Can Not show logo. 'wmt.logosize.ubuntu' is wrong", 0xFF00);
					return -1;
				}

				if(charging_logosize == 0) {
					ERROR("'wmt.logosize.charge' is wrong\n");
					//show_text_to_screen("Can Not show logo. 'wmt.logosize.charge' is wrong", 0xFF00);
					return -1;
				}

				if(dualboot_android_logosize == 0) {
					ERROR("'wmt.logosize.dualboot.android' is wrong\n");
					//show_text_to_screen("Can Not show logo. 'wmt.logosize.dualboot.android' is wrong", 0xFF00);
					return -1;
				}

				if(dualboot_ubuntu_logosize == 0) {
					ERROR("'wmt.logosize.dualboot.ubuntu' is wrong\n");
					//show_text_to_screen("Can Not show logo. 'wmt.logosize.dualboot.android' is wrong", 0xFF00);
					return -1;
				}
			}
		} else {
			/*
				auto caculate the logo size
			*/
			INFO("Caculate logo size\n");
			android_logosize = 0;
			ubuntu_logosize = 0;
			charging_logosize = 0;
			dualboot_android_logosize= 0;
			dualboot_ubuntu_logosize = 0;
			totalsize = 0x800000;
		}

		/*
			Read all logo to memory from Nand Flash
		*/
		REG32_VAL(GPIO_BASE_ADDR + 0x200) &= ~(1 << 11); //PIN_SHARE_SDMMC1_NAND
		ret = WMTAccessNandEarier(naddr, maddr, totalsize, 0);
		if(ret) {
			ERROR("load logo from NAND Flash fail\n");
			//show_text_to_screen("Load logo fail from NAND Flash", 0xFF00);
			return -1;
		}

		p_maddr = (unsigned char *)maddr;

		/*
			Check android logo. The 1st logo.
		*/
		if(*p_maddr != 'B') {
			ERROR("android logo is Not BMP picture\n");
			//show_text_to_screen("android logo is Not BMP picture", 0xFF00);
			return -1;
		}

		if(android_logosize == 0) {
			android_logosize = (*(unsigned short *)(p_maddr + 4) << 16) + (*(unsigned short *)(p_maddr + 2));
			if(android_logosize == 0) {
				ERROR("android logo size is 0\n");
				//show_text_to_screen("Can Not show logo. android logo size is 0", 0xFF00);
				return -1;
			}
		}

		uboot_logo[LOGO_ANDROID].valid = 1;
		uboot_logo[LOGO_ANDROID].type = LOGO_ANDROID;
		uboot_logo[LOGO_ANDROID].maddr = (unsigned int)p_maddr;
		uboot_logo[LOGO_ANDROID].size = android_logosize;

		/*
			Check charging logo. The 2nd logo.
		*/
		p_maddr += android_logosize;

		if(*p_maddr != 'B') {
			ERROR("charging logo is Not BMP picture\n");
			//show_text_to_screen("charging logo is Not BMP picture", 0xFF00);
			return -1;
		}

		if(charging_logosize == 0) {
			charging_logosize = (*(unsigned short *)(p_maddr + 4) << 16) + (*(unsigned short *)(p_maddr + 2));
			if(charging_logosize == 0) {
				ERROR("charging logo size is 0\n");
				//show_text_to_screen("Can Not show logo. charging logo size is 0", 0xFF00);
				return -1;
			}
		}

		uboot_logo[LOGO_CHARGING].valid = 1;
		uboot_logo[LOGO_CHARGING].type = LOGO_CHARGING;
		uboot_logo[LOGO_CHARGING].maddr = (unsigned int)p_maddr;
		uboot_logo[LOGO_CHARGING].size = charging_logosize;

		/*
			Check dualboot android logo. The 3rd logo
		*/
		p_maddr += charging_logosize;

		if(*p_maddr != 'B') {
			ERROR("dualboot android logo is Not BMP picture\n");
			//show_text_to_screen("dualboot android logo is Not BMP picture", 0xFF00);
			return -1;
		}

		if(dualboot_android_logosize == 0) {
			dualboot_android_logosize = (*(unsigned short *)(p_maddr + 4) << 16) + (*(unsigned short *)(p_maddr + 2));
			if(dualboot_android_logosize == 0) {
				ERROR("dualboot android logo size is 0\n");
				//show_text_to_screen("Can Not show logo. dualboot android logo size is 0", 0xFF00);
				return -1;
			}
		}

		uboot_logo[LOGO_DUALBOOT_ANDROID].valid = 1;
		uboot_logo[LOGO_DUALBOOT_ANDROID].type = LOGO_DUALBOOT_ANDROID;
		uboot_logo[LOGO_DUALBOOT_ANDROID].maddr = (unsigned int)p_maddr;
		uboot_logo[LOGO_DUALBOOT_ANDROID].size = dualboot_android_logosize;

		header = (bmp_header_t *)p_maddr;
		uboot_logo[LOGO_DUALBOOT_ANDROID].height = mv_le_to_cpu((char *)&header->height, 4);

		/*
			Check dualboot ubuntu logo. the 4th logo
		*/
		p_maddr += dualboot_android_logosize;

		if(*p_maddr != 'B') {
			ERROR("dualboot ubuntu logo is Not BMP picture\n");
			//show_text_to_screen("dualboot ubuntu logo is Not BMP picture", 0xFF00);
			return -1;
		}

		if(dualboot_ubuntu_logosize == 0) {
			dualboot_ubuntu_logosize = (*(unsigned short *)(p_maddr + 4) << 16) + (*(unsigned short *)(p_maddr + 2));
			if(dualboot_ubuntu_logosize == 0) {
				ERROR("dualboot ubuntu logo size is 0\n");
				//show_text_to_screen("Can Not show logo. dualboot ubuntu logo size is 0", 0xFF00);
				return -1;
			}
		}

		uboot_logo[LOGO_DUALBOOT_UBUNTU].valid = 1;
		uboot_logo[LOGO_DUALBOOT_UBUNTU].type = LOGO_DUALBOOT_UBUNTU;
		uboot_logo[LOGO_DUALBOOT_UBUNTU].maddr = (unsigned int)p_maddr;
		uboot_logo[LOGO_DUALBOOT_UBUNTU].size = dualboot_ubuntu_logosize;

		header = (bmp_header_t *)p_maddr;
		uboot_logo[LOGO_DUALBOOT_UBUNTU].height = mv_le_to_cpu((char *)&header->height, 4);

		/*
			Check ubuntu logo. the 5th logo
		*/
		p_maddr += dualboot_ubuntu_logosize;

		if(*p_maddr != 'B') {
			ERROR("ubuntu logo is Not BMP picture\n");
			//show_text_to_screen("ubuntu logo is Not BMP picture", 0xFF00);
			return -1;
		}

		if(ubuntu_logosize == 0) {
			ubuntu_logosize = (*(unsigned short *)(p_maddr + 4) << 16) + (*(unsigned short *)(p_maddr + 2));
			if(ubuntu_logosize == 0) {
				ERROR("ubuntu logo size is 0\n");
				//show_text_to_screen("Can Not show logo. ubuntu logo size is 0", 0xFF00);
				return -1;
			}
		}

		uboot_logo[LOGO_UBUNTU].valid = 1;
		uboot_logo[LOGO_UBUNTU].type = LOGO_UBUNTU;
		uboot_logo[LOGO_UBUNTU].maddr = (unsigned int)p_maddr;
		uboot_logo[LOGO_UBUNTU].size = ubuntu_logosize;

		dualboot_logo_init_ok = 1;
	} else {
		/*
			Load every logo to Memory from SD Card
			Only load every logo to Memory Once unless 'force' is set
		*/
		char tmp[100] = {0};

		if(force) {
			uboot_logo[logo_type].valid = 0;
			mmc_init_ok = 0;
		}

		if(uboot_logo[logo_type].valid)
			return 0;

		if ((p_maddr_env = getenv("wmt.display.logoaddr")))
			maddr = simple_strtoul(p_maddr_env, NULL, 16);
		else {
			maddr = 0x500000;
			WARNING("wmt.display.logoaddr is not set\n");
			INFO("logoaddr default set to 0x500000");
		}

		if(mmc_init_ok == 0) {
			ret = run_command("mmcinit 0", 0);
			if(ret == -1) {
				ERROR("\"mmcinit 0\" failed\n");
				//show_text_to_screen("Can Not show logo. 'mmcinit 0' failed", 0xFF00);
				return -1;
			}

			mmc_init_ok = 1;
		}

		switch (logo_type) {
			case LOGO_ANDROID:
				uboot_logo[logo_type].maddr = maddr + 0xC00000;
				sprintf(tmp, "fatload mmc 0 0x%x u-boot-logo.bmp", uboot_logo[logo_type].maddr);
			break;

			case LOGO_UBUNTU:
				uboot_logo[logo_type].maddr = maddr + 0x1800000;
				sprintf(tmp, "fatload mmc 0 0x%x ubuntu-logo.bmp", uboot_logo[logo_type].maddr);
			break;

			case LOGO_CHARGING:
				uboot_logo[logo_type].maddr = maddr + 0x1E00000;
				sprintf(tmp, "fatload mmc 0 0x%x charge-logo.bmp", uboot_logo[logo_type].maddr);
			break;

			case LOGO_DUALBOOT_ANDROID:
				uboot_logo[logo_type].maddr = maddr;
				sprintf(tmp, "fatload mmc 0 0x%x dualboot-android-logo.bmp", maddr);
			break;

			case LOGO_DUALBOOT_UBUNTU:
				uboot_logo[logo_type].maddr = maddr + 0x600000;
				sprintf(tmp, "fatload mmc 0 0x%x dualboot-ubuntu-logo.bmp", uboot_logo[logo_type].maddr);
			break;

			default:
				ERROR("Not Supported Logo Type: %d\n", logo_type);
				//sprintf(tmp, "Not Supported Logo Type: %d", logo_type);
				//show_text_to_screen(tmp, 0xFF00);
			return -1;
		}

		ret = run_command(tmp, 0);
		if(ret != -1) {
			p_maddr = (unsigned char *)uboot_logo[logo_type].maddr;
			if(*p_maddr == 'B') {
				uboot_logo[logo_type].valid = 1;
				uboot_logo[logo_type].type = logo_type;
				if(logo_type == LOGO_DUALBOOT_ANDROID || logo_type == LOGO_DUALBOOT_UBUNTU)
				{
					header = (bmp_header_t *)p_maddr;
					uboot_logo[logo_type].height = mv_le_to_cpu((char *)&header->height, 4);
				}
			} else
				ERROR("logo is Not BMP picture\n");

		} else {
			ERROR("fatload logo failed. logo_type = %d\n", logo_type);
			//show_text_to_screen("Can Not show logo. fatload logo failed", 0xFF00);
			mmc_init_ok = 0;
		}
	}

	return 0;
}

static int init_dualboot_text(void)
{
	char *p;
	char * endp;
	unsigned long value;

	p = getenv(ENV_DUALBOOT_INFOLEVEL);
	if (!p)
        	return -1;

	value = simple_strtoul(p, &endp, 0);
	if(value <= 2)
		text_display_level = value;
	else
		text_display_level = 3;

	return 0;
}

static void display_dualboot_text(int show_text, LOGO_TYPE logo_type)
{
	int no;
	mv_surface *s;
	mv_Rect rect;
	char r, g, b;
	unsigned int rgb = 0xFF00; //green
	int text_width, text_height, logo_height = 0;
	int text_pos_x, text_pos_y, logo_pos_y;

	if(text_display_level == 0)
		return;

	if(key_select.use_power_key == 0 && key_select.use_volume_key == 0 && key_select.use_usb_kpad == 0)
		return;
	/*
	* show count down text only when dualboot_andorid and dualboot_ubuntu display
	*/
	if(logo_type == LOGO_DUALBOOT_ANDROID || logo_type == LOGO_DUALBOOT_UBUNTU)
		logo_height = uboot_logo[logo_type].height;
	 else if(logo_type < LOGO_MAX_NUM){
	 	text_is_display = 0;
		return;
	 } else
	 	return;

	if(text_need_always_display)
		text_is_display = 0;

	if(show_text) {
		if(text_is_display)
			return;
	} else {
		if(!text_is_display)
			return;
	}

	switch (text_display_level) {
		case 0:
		return;

		case 1:
			if(key_select.use_power_key && key_select.use_volume_key)
 				text_width = strlen(TEXT_PRESS_POWER_VOLUME_KEY) * CHAR_WIDTH;
			else if(key_select.use_power_key)
				text_width = strlen(TEXT_PRESS_POWER_KEY) * CHAR_WIDTH;
			else if(key_select.use_volume_key)
				text_width = strlen(TEXT_PRESS_VOLUME_KEY) * CHAR_WIDTH;
			else
				text_width = strlen(TEXT_NO_KEY) * CHAR_WIDTH;

			text_height = CHAR_HEIGHT;  // 1 line text
		break;

		case 2:
			text_width = strlen(TEXT_BRIEF_COUNT_DOWN) * CHAR_WIDTH;
			text_height = CHAR_HEIGHT; // 1 line text
		break;

		default:
			text_width = strlen(TEXT_DETAIL_COUNT_DOWN) * CHAR_WIDTH;
			text_height = 2 * CHAR_HEIGHT;  // 2 line text
		break;
	}

	text_pos_x = CHAR_WIDTH;
	text_pos_y = CHAR_HEIGHT;
	r = (rgb >> 16) & 0xFF;
	g = (rgb >> 8)	& 0xFF;
	b = rgb & 0xFF;

	for(no = 0; no < 2; no++) {
		if(no == 1 && g_vpp.virtual_display == 0)
			break;

		s = mv_getSurface(no);
		if(s->startAddr) {
			if(logo_height > 0) {
				if(g_display_direction == 0 || g_display_direction == 2) {
					if (text_width > s->height)
						WARNING("no = %d, text_width(%d) > LCD(%d)\n", no, text_width, s->height);
					else
						text_pos_x = (s->height - text_width) / 2;

					if (logo_height > s->width)
						WARNING("no = %d, logo_height(%d) > LCD(%d)\n", no, logo_height, s->width);
					else {
						logo_pos_y = (s->width - logo_height) / 2;
						text_pos_y = (logo_pos_y - text_height) / 2;
						if(text_pos_y < 0) {
							//WARNING("text_pos_y = %d, countdown alway show\n", text_pos_y);
							text_pos_y = CHAR_HEIGHT;
							text_need_always_display = 1;
						}
					}
				} else {
					if (text_width > s->width)
						WARNING("no = %d, text_width(%d) > LCD(%d)\n", no, text_width, s->width);
					else
						text_pos_x = (s->width - text_width) / 2;

					if (logo_height > s->height)
						WARNING("no = %d, logo_height(%d) > LCD(%d)\n", no, logo_height, s->height);
					else {
						logo_pos_y = (s->height - logo_height) / 2;
						text_pos_y = (logo_pos_y - text_height) / 2;
						if(text_pos_y < 0) {
							//WARNING("text_pos_y = %d, logo_height = %d, lcd_height = %d, text_height = %d, logo_pos_y = %d, countdown alway show\n",
							//	text_pos_y, logo_height, s->height, text_height, logo_pos_y);
							text_pos_y = CHAR_HEIGHT;
							text_need_always_display = 1;
						}
					}
				}
			} else {
				text_pos_x = CHAR_WIDTH;
				text_pos_y = CHAR_HEIGHT;
				text_need_always_display = 1;
			}

			//printf("text_pos_x = %d, text_pos_y = %d\n", text_pos_x, text_pos_y);

			if(show_text) {
				switch (text_display_level) {
					case 0:
					return;

					case 1:
						if(key_select.use_power_key && key_select.use_volume_key)
							mv_textOut(no, text_pos_x, text_pos_y, TEXT_PRESS_POWER_VOLUME_KEY, r, g, b);
						else if(key_select.use_power_key)
							mv_textOut(no, text_pos_x, text_pos_y, TEXT_PRESS_POWER_KEY, r, g, b);
						else if(key_select.use_volume_key)
							mv_textOut(no, text_pos_x, text_pos_y, TEXT_PRESS_VOLUME_KEY, r, g, b);
						else
							mv_textOut(no, text_pos_x, text_pos_y, TEXT_NO_KEY, r, g, b);
					break;

					case 2:
						mv_textOut(no, text_pos_x, text_pos_y, TEXT_BRIEF_COUNT_DOWN, r, g, b);
						/*
 						* The TEXT_COUNT_DOWN reverse a char at last for calculating the TEXT_COUNT_DOWN's x coordinate
						*/
						countdown_seconds_pos.x = text_pos_x + text_width - CHAR_WIDTH;
						countdown_seconds_pos.y = text_pos_y;
					break;

					default:
						if(key_select.use_power_key && key_select.use_volume_key)
							mv_textOut(no, text_pos_x, text_pos_y, TEXT_PRESS_POWER_VOLUME_KEY, r, g, b);
						else if(key_select.use_power_key)
							mv_textOut(no, text_pos_x, text_pos_y, TEXT_PRESS_POWER_KEY, r, g, b);
						else if(key_select.use_volume_key)
							mv_textOut(no, text_pos_x, text_pos_y, TEXT_PRESS_VOLUME_KEY, r, g, b);
						else
							mv_textOut(no, text_pos_x, text_pos_y, TEXT_NO_KEY, r, g, b);

						mv_textOut(no, text_pos_x, text_pos_y + CHAR_HEIGHT, TEXT_DETAIL_COUNT_DOWN, r, g, b);

						/*
 						* The TEXT_COUNT_DOWN reverse a char at last for calculating the TEXT_COUNT_DOWN's x coordinate
						*/
						countdown_seconds_pos.x = text_pos_x + text_width - CHAR_WIDTH;
						countdown_seconds_pos.y = text_pos_y + CHAR_HEIGHT; // seconds in the second line

					break;
				}

				text_is_display = 1;
			} else {
				rect.left = 0;
				rect.top = text_pos_y;
				rect.right = s->width;
				rect.bottom = text_pos_y + text_height;
				if(g_display_direction == 0 || g_display_direction == 2) //the screen is portrait
					rect.right = s->height;

				mv_fillRect(no, &rect, 0, 0, 0);

				text_is_display = 0;
			}
		}
	}
}

static void display_countdown_seconds(int seconds, int force_update)
{
	int no, len;
	mv_surface *s;
	int max_seconds_char_num = 3; // 0 - 999
	int screen_width;
	mv_Rect rect;
	char r, g, b;
	unsigned int rgb = 0xFF00; //green
	char tmpbuf[20];
	static int last_seconds = -1;

	if(text_is_display == 0 || text_display_level < 2) {
		INFO("CountDown: %d\n", seconds);
		return;
	}

	if(force_update)
		last_seconds = -1;

	if(seconds == last_seconds)
		return;

	for(no = 0; no < 2; no++) {
		if(no == 1 && g_vpp.virtual_display == 0)
			break;

		s = mv_getSurface(no);
		if(s->startAddr) {
			if(g_display_direction == 0 || g_display_direction == 2)
				screen_width = s->height;
			else
				screen_width = s->width;

			if((countdown_seconds_pos.x + max_seconds_char_num) > (screen_width - 1)) {
				ERROR("No = %d, the seconds diaplay range ( %d ~ %d ) is larger than in screen display width (0 ~ %d)\n",
					no, countdown_seconds_pos.x, countdown_seconds_pos.x + max_seconds_char_num, screen_width - 1);
				return;
			}

			rect.left = countdown_seconds_pos.x;
			rect.top = countdown_seconds_pos.y;
			rect.right = s->width;
			switch(text_display_level) {
				case 0:
				case 1:
					return;

				default:
					rect.bottom = countdown_seconds_pos.y + CHAR_HEIGHT;
				break;
			}

			if(g_display_direction == 0 || g_display_direction == 2)
				rect.right = s->height;

			mv_fillRect(no, &rect, 0, 0, 0);

			r = (rgb >> 16) & 0xFF;
			g = (rgb >> 8)	& 0xFF;
			b = rgb & 0xFF;

			len = sprintf(tmpbuf, "%2d", seconds);
			tmpbuf[len] = 0;

			mv_textOut(no, countdown_seconds_pos.x, countdown_seconds_pos.y, tmpbuf, r, g, b);

			INFO("CountDown: %d\n", seconds);

			last_seconds = seconds;
		}
	}
}

static int display_dualboot_logo(LOGO_TYPE logo_type, int display_text)
{
	unsigned int backup_img_phy;

	if(logo_type == current_logo_type)
		return 0;

	init_dualboot_logo(0, logo_type);

	if(uboot_logo[logo_type].valid) {
		g_logo_x = -1;
		g_logo_y = -1;
		backup_img_phy = g_img_phy;
		g_img_phy = uboot_logo[logo_type].maddr;
		/*
			if logo switch between dualboot_android_logo and dualboot_ubuntu_logo,
			don't clear framebuffer
		*/
		if((logo_type == LOGO_DUALBOOT_ANDROID && current_logo_type == LOGO_DUALBOOT_UBUNTU)
			|| (logo_type == LOGO_DUALBOOT_UBUNTU&& current_logo_type == LOGO_DUALBOOT_ANDROID))
			display_show(0);
		else
			display_show(1);

		g_img_phy = backup_img_phy;

		current_logo_type = logo_type;

		if(display_text)
			display_dualboot_text(1, logo_type);
	} else {
		switch (logo_type) {
			case LOGO_DUALBOOT_ANDROID:
				show_text_to_screen("DualBoot-Android-Logo.bmp", 0xFF00);
			break;

			case LOGO_DUALBOOT_UBUNTU:
				show_text_to_screen("DualBoot-Ubuntu-Logo.bmp", 0xFF00);
			break;

			case LOGO_ANDROID:
				show_text_to_screen("Android-Logo.bmp", 0xFF00);
			break;

			case LOGO_CHARGING:
				show_text_to_screen("Charging-Logo.bmp", 0xFF00);
			break;

			case LOGO_UBUNTU:
				show_text_to_screen("Ubuntu-Logo.bmp", 0xFF00);
			break;

			case LOGO_MAX_NUM:
			default:

			break;
		}

		current_logo_type = logo_type;

		if(display_text)
			display_dualboot_text(1, logo_type);
	}

	return 0;
}

static int parse_key_param(char *name, KEY_SELECT *p_key_select, int *p_param_num)
{
	enum
	{
		idx_use_power_key,
		idx_use_volume_key,
		idx_use_usb_kpad,
		idx_countdown_seconds,
		idx_volume_plus_gpiono,
		idx_volume_minus_gpiono,
		idx_volume_key_active,
		idx_max
	};

	char *p;
	long ps[idx_max] = {0};
	char * endp;
	int i = 0;

	p = getenv(name);
	if (!p) {
        	return -1;
	}

   	while (i < idx_max) {
		ps[i++] = simple_strtoul(p, &endp, 0);

        	if (*endp == '\0')
            		break;
        	p = endp + 1;

        	if (*p == '\0')
			break;
	}

	p_key_select->use_power_key = ps[0];
	p_key_select->use_volume_key = ps[1];
	p_key_select->use_usb_kpad = ps[2];
	p_key_select->countdown_seconds = ps[3];
	p_key_select->volume_plus_gpiono = ps[4];
	p_key_select->volume_minus_gpiono = ps[5];
	p_key_select->volume_key_active = ps[6];

	*p_param_num = i;

	return 0;
}

static int init_dualboot_key(void)
{
	KEY_SELECT key;
	int ret, num = 0;

	if(dualboot_key_init_ok)
		return 0;

	dualboot_key_init_ok = 1;

	ret = parse_key_param(ENV_DUALBOOT_KEY, &key, &num);
	if(ret)
		return -1;

	if(num == 7) {
		if(key.use_volume_key) {
			/*
			* Check whether the volume key's gpio is valid when use volume key
			*/
			if(key.volume_plus_gpiono >= WMT_PIN_GP0_GPIO0
				&& key.volume_plus_gpiono <= WMT_PIN_GP63_SD2CD
				&& key.volume_minus_gpiono >= WMT_PIN_GP0_GPIO0
				&& key.volume_minus_gpiono >= WMT_PIN_GP63_SD2CD) {
					memcpy(&key_select, &key, sizeof(KEY_SELECT));
			} else
				WARNING("wrong %s. volume+ = %d. volume- = %d. gpio_no range should be in %d ~ %d\n",
					ENV_DUALBOOT_KEY, key.volume_plus_gpiono, key.volume_minus_gpiono,
					WMT_PIN_GP0_GPIO0, WMT_PIN_GP63_SD2CD);
		} else
			memcpy(&key_select, &key, sizeof(KEY_SELECT));

	} else
		WARNING("wrong %s. The param's num = %d. It should be equal to 7\n",
			ENV_DUALBOOT_KEY, num);

	if(key_select.use_volume_key) {
		gpio_direction_input(key_select.volume_plus_gpiono);
		gpio_direction_input(key_select.volume_minus_gpiono);

		if(key_select.volume_key_active) {
			gpio_setpull(key_select.volume_plus_gpiono, GPIO_PULL_DOWN);
			gpio_setpull(key_select.volume_minus_gpiono, GPIO_PULL_DOWN);
		} else {
			gpio_setpull(key_select.volume_plus_gpiono, GPIO_PULL_UP);
			gpio_setpull(key_select.volume_minus_gpiono, GPIO_PULL_UP);
		}
	}

	if(key_select.use_power_key) {
		if(REG32_VAL(0xD8130014) & BIT14)
			REG32_VAL(0xD8130014) |= BIT14;
	}

	return 0;
}

/*
* return 1: key pressed.  0: key is Not pressed
*/
static KEY_STATUS volume_key_pressed(int key_gpiono)
{
	int key_val;

	if(key_select.use_volume_key) {
		key_val = gpio_get_value(key_gpiono);
		if(key_select.volume_key_active)
			return key_val ? KEY_PRESSED : KEY_RELEASED;
		else
			return key_val ? KEY_RELEASED : KEY_PRESSED;
	}

	return KEY_STATUS_MAX;
}

static KEY_STATUS power_key_pressed(void)
{
	if(key_select.use_power_key) {
		if(REG32_VAL(0xD8130054) & BIT24)
			return KEY_PRESSED;

		if(REG32_VAL(0xD8130014) & BIT14) {
			REG32_VAL(0xD8130014) |= BIT14;
			return KEY_PRESSED;
		}

		return KEY_RELEASED;
	}

	return KEY_STATUS_MAX;
}

static int save_os_param(LOGO_TYPE logo_type)
{
	char *p;
	int ret;
	int need_save_env = 0;

	p = getenv(ENV_DUALBOOT_OS);
	if(!p)
		return -1;

	if(strnicmp(p, "android", 7) == 0) {
		if(logo_type == LOGO_UBUNTU || logo_type == LOGO_DUALBOOT_UBUNTU) {
			ret = setenv(ENV_DUALBOOT_OS, "ubuntu");
			if(ret == 0) {
				INFO("set wmt.dualboot.os to ubuntu\n");
				need_save_env = 1;
			}
			else {
				ERROR("fail to set wmt.dualboot.os to ubuntu\n");
				return -1;
			}
		}

	}
	else if(strnicmp(p, "ubuntu", 6) == 0) {
		if(logo_type == LOGO_ANDROID || logo_type == LOGO_DUALBOOT_ANDROID) {
			ret = setenv(ENV_DUALBOOT_OS, "android");
			if(ret == 0) {
				INFO("set wmt.dualboot.os to android\n");
				need_save_env = 1;
			}
			else {
				ERROR("fail to set wmt.dualboot.os to android\n");
				return -1;
			}
		}
	} else {
		ERROR("Wrong %s = %s\n", ENV_DUALBOOT_OS, p);
		return -1;
	}

	if(need_save_env) {
		ret = saveenv();
		if(ret == 0)
			INFO("save env success\n");
		else {
			ERROR("save env fail\n");
			return -1;
		}
	}

	return 0;
}

#define ANDROID_BOOT_NAND_OTA_NORMAL "nandrw boot ${boot-NAND_ofs} ${load-addr-kernel} ${load-addr-initrd} filesize"
#define ANDROID_SET_RFS_RAM_OTA "setenv bootargs mem=${memtotal} root=/dev/ram0 rw initrd=${load-addr-initrd},0x${filesize} console=ttyS0,115200n8 ${ubi-addon} init=/init androidboot.serialno=${androidboot.serialno}"
#define UBUNTU_BOOT_NAND_OTA_NORMAL "nandrw boot ${boot-NAND_ofs_ubuntu} ${load-addr-kernel} ${load-addr-initrd} filesize"
#define UBUNTU_SET_RFS_RAM_OTA "setenv bootargs elevator=noop mem=${memtotal} initrd=${load-addr-initrd},0x${filesize} ubi.mtd=14 root=ubi0_0 rw rootfstype=ubifs console=ttyS0,115200n8"

static int set_android_bootargs(void)
{
	char *p;
	int ret;

	p = getenv("boot-nand-ota-normal");
	if(!p || strcmp(p, ANDROID_BOOT_NAND_OTA_NORMAL)) {
		ret = setenv("boot-nand-ota-normal", ANDROID_BOOT_NAND_OTA_NORMAL);
		if(ret == 0)
			INFO("set android's boot-nand-ota-normal success\n");
		else {
			ERROR("fail to set android's boot-nand-ota-normal\n");
			return -1;
		}
	}

	p = getenv("set-rfs-ram-ota");
	if(!p || strcmp(p, ANDROID_SET_RFS_RAM_OTA)) {
		ret = setenv("set-rfs-ram-ota", ANDROID_SET_RFS_RAM_OTA);
		if(ret == 0)
			INFO("set android's set-rfs-ram-ota success\n");
		else {
			ERROR("fail to set android's set-rfs-ram-ota\n");
			return -1;
		}
	}

	return 0;
}

static int set_ubuntu_bootargs(void)
{
	char *p;
	int ret;

	p = getenv("boot-nand-ota-normal");
	if(!p || strcmp(p, UBUNTU_BOOT_NAND_OTA_NORMAL)) {
		ret = setenv("boot-nand-ota-normal", UBUNTU_BOOT_NAND_OTA_NORMAL);
		if(ret == 0)
			INFO("set ubuntu's boot-nand-ota-normal success\n");
		else {
			ERROR("fail to set ubuntu's boot-nand-ota-normal\n");
			return -1;
		}
	}

	p = getenv("set-rfs-ram-ota");
	if(!p || strcmp(p, UBUNTU_SET_RFS_RAM_OTA)) {
		ret = setenv("set-rfs-ram-ota", UBUNTU_SET_RFS_RAM_OTA);
		if(ret == 0)
			INFO("set ubuntu's set-rfs-ram-ota success\n");
		else {
			ERROR("fail to set ubuntu's set-rfs-ram-ota\n");
			return -1;
		}
	}
}

#define PERIOD_CHECK           10    		// 10 ms    //how long time to check
#define PERIOD_SHOW_SECONDS    1000    		// 1s       //how long time to show seconds

int show_dualboot_logo(int show_text)
{
	char *p;

	p = getenv(ENV_DUALBOOT_OS);
	if (!p)
        	return -1;

	init_dualboot_key();
	init_dualboot_text();

	if(strnicmp(p, "android", 7) == 0)
		display_dualboot_logo(LOGO_DUALBOOT_ANDROID, show_text);
	else if(strnicmp(p, "ubuntu", 6) == 0)
		display_dualboot_logo(LOGO_DUALBOOT_UBUNTU, show_text);
	else {
		ERROR("Wrong %s = %s\n", ENV_DUALBOOT_OS, p);
		return -1;
	}

	INFO("show_dualboot_logo OK\n");

	return 0;
}

int choose_dualboot_system(void)
{
	char *p;
	int loop = 0;
	const int period_show_seconds = (PERIOD_SHOW_SECONDS) / (PERIOD_CHECK);
	int rising_edge_detected;
	int except_seconds, remain_seconds;
	int cancel = 0;
	LOGO_TYPE logo_type = LOGO_MAX_NUM;

	KEY_TRACE power_key_trace = {
		.old_status = KEY_STATUS_MAX,
		.new_status = KEY_STATUS_MAX
	};

	KEY_TRACE volume_plus_key_trace = {
		.old_status = KEY_STATUS_MAX,
		.new_status = KEY_STATUS_MAX
	};

	KEY_TRACE volume_minus_key_trace = {
		.old_status = KEY_STATUS_MAX,
		.new_status = KEY_STATUS_MAX
	};


	p = getenv(ENV_DUALBOOT_OS);
	if (!p)
        	return -1;

	init_dualboot_key();
	init_dualboot_text();

	if(strnicmp(p, "android", 7) == 0) {
		display_dualboot_logo(LOGO_DUALBOOT_ANDROID, 1);
		logo_type = LOGO_DUALBOOT_ANDROID;
	} else if(strnicmp(p, "ubuntu", 6) == 0) {
		display_dualboot_logo(LOGO_DUALBOOT_UBUNTU, 1);
		logo_type = LOGO_DUALBOOT_UBUNTU;
	} else {
		ERROR("Wrong %s = %s\n", ENV_DUALBOOT_OS, p);
		return -1;
	}

	display_dualboot_text(1, logo_type);

	except_seconds = key_select.countdown_seconds;

	if(except_seconds <= 0) {
		display_countdown_seconds(0, 1);
		mdelay(100);
		return 0;
	}

	remain_seconds = except_seconds;
	display_countdown_seconds(remain_seconds, 1);

	if(key_select.use_power_key)
		power_key_trace.old_status = power_key_pressed();
	if(key_select.use_volume_key) {
		volume_plus_key_trace.old_status = volume_key_pressed(key_select.volume_plus_gpiono);
		volume_minus_key_trace.old_status = volume_key_pressed(key_select.volume_minus_gpiono);
	}

	while(1) {
		if (tstc()) {//we got a key press
			printf("we got a key press. Exit test.\n");
			//Check if the key is 'Enter' which ascii code is 13
			// If the key is 'Enter', exit
			if (getc() == 13) {
				cancel = 1;
				break;
			}
		}

		rising_edge_detected = 0;

		if(loop % period_show_seconds == 0) {
			display_countdown_seconds(remain_seconds, 0);
			if(remain_seconds == 0) {
				mdelay(100);
				break;
			}
			remain_seconds--;
		}

		udelay(PERIOD_CHECK * 1000);

		if(key_select.use_power_key) {
			power_key_trace.new_status = power_key_pressed();
			/*
			*  detected a rising edge
			*/
			if(power_key_trace.old_status == KEY_RELEASED && power_key_trace.new_status == KEY_PRESSED)
				rising_edge_detected = 1;
			power_key_trace.old_status = power_key_trace.new_status;
		}

		if(key_select.use_volume_key) {
			volume_plus_key_trace.new_status = volume_key_pressed(key_select.volume_plus_gpiono);
			if(volume_plus_key_trace.old_status == KEY_RELEASED && volume_plus_key_trace.new_status == KEY_PRESSED)
				rising_edge_detected = 1;
			volume_plus_key_trace.old_status = volume_plus_key_trace.new_status;

			volume_minus_key_trace.new_status = volume_key_pressed(key_select.volume_minus_gpiono);
			if(volume_minus_key_trace.old_status == KEY_RELEASED && volume_minus_key_trace.new_status == KEY_PRESSED)
				rising_edge_detected = 1;
			volume_minus_key_trace.old_status = volume_minus_key_trace.new_status;
		}

		if(rising_edge_detected) {
			if(logo_type == LOGO_DUALBOOT_UBUNTU) {
				display_dualboot_logo(LOGO_DUALBOOT_ANDROID, 1);
				logo_type = LOGO_DUALBOOT_ANDROID;
			} else if(logo_type == LOGO_DUALBOOT_ANDROID) {
				display_dualboot_logo(LOGO_DUALBOOT_UBUNTU, 1);
				logo_type = LOGO_DUALBOOT_UBUNTU;
			}

			remain_seconds = except_seconds;
			display_countdown_seconds(remain_seconds, 0);
			remain_seconds--;
			loop = 0;
		}

		loop++;
	}

	if(logo_type == LOGO_DUALBOOT_UBUNTU) {
		display_dualboot_logo(LOGO_UBUNTU, 1);
		logo_type = LOGO_UBUNTU;
	}
	else if(logo_type == LOGO_DUALBOOT_ANDROID) {
		display_dualboot_logo(LOGO_ANDROID, 1);
		logo_type = LOGO_ANDROID;
	}

	if(cancel == 0)
		save_os_param(logo_type);

	if(logo_type == LOGO_UBUNTU || logo_type == LOGO_DUALBOOT_UBUNTU)
		set_ubuntu_bootargs();
	else if(logo_type == LOGO_ANDROID || logo_type == LOGO_DUALBOOT_ANDROID)
		set_android_bootargs();

	return 0;
}

static void dualboot(void)
{
	show_dualboot_logo(0);
	choose_dualboot_system();
}

static int dualboot_main (cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	LOGO_TYPE logo_type;
	unsigned long value;
	char *endp;

	switch (argc) {
		case 0:
		break;

		case 1:
			dualboot();
			return 0;
		break;

		case 2:

		break;

		default: /* at least 3 args */
			if (strncmp(argv[1], "logo", 1) == 0) {
				value = simple_strtoul(argv[2], &endp, 0);
				if (value >= LOGO_ANDROID && value < LOGO_MAX_NUM) {
					logo_type = (LOGO_TYPE)value;
					display_dualboot_logo(logo_type, 1);
					return 0;
				}
			}
		break;
	}

	printf ("Usage:\n%s\n", cmdtp->usage);
	return 1;
}

U_BOOT_CMD(
	dualboot,	5,	1,	dualboot_main,
	"dualboot  - WMT Dual Boot sub-system\n",
	NULL
);

