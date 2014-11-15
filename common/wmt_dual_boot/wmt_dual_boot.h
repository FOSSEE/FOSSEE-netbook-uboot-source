/*++
	The Header file of WMT Dual Boot Driver

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

#ifndef __WMT_DUALBOOT_H
#define __WMT_DUALBOOT_H

/******************************************************************************
 *
 * Type definitions
 *
 ******************************************************************************/
typedef enum _LOGO_TYPE
{
	LOGO_ANDROID,
	LOGO_CHARGING,
	LOGO_DUALBOOT_ANDROID,
	LOGO_DUALBOOT_UBUNTU,
	LOGO_UBUNTU,
	LOGO_MAX_NUM
} LOGO_TYPE;

typedef struct _UBOOT_LOGO
{
	int valid;
	LOGO_TYPE type;
	int width;
	int height;
	int size;
	unsigned int maddr;
} UBOOT_LOGO;

typedef enum _KEY_VALUE
{
	KEY_LEFT,
	KEY_RIGHT,
	KEY_ENTER,
	KEY_VALUE_MAX
} KEY_VALUE;

typedef enum _KEY_STATUS
{
	KEY_RELEASED,
	KEY_PRESSED,
	KEY_STATUS_MAX
} KEY_STATUS;

typedef struct _KEY_TRACE {
	KEY_STATUS old_status;
	KEY_STATUS new_status;
} KEY_TRACE;

typedef struct _KEY_SELECT {
	int use_power_key;        // 1: use power key.  0: don't use power key
	int use_volume_key;       // 1: use volume key. 0: don't use volume key
	int use_usb_kpad;         // 1: use usb keypad  0: don't use usb keypad
	int countdown_seconds;    // seconds to count down for startup system
	int volume_plus_gpiono;   // volume+
	int volume_minus_gpiono;  // volume-
	int volume_key_active;    // 1: high level means the key is pressed. 0: low level means the key is pressed.
} KEY_SELECT;

typedef struct _TEXT_COORDINATE {
	int x;
	int y;
} TEXT_COORDINATE;

/******************************************************************************
 *
 * External functions
 *
 ******************************************************************************/
int show_dualboot_logo(int show_text);
int choose_dualboot_system(void);

#endif

