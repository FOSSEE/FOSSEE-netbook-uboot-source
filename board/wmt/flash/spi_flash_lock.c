/*
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * Copyright (c) 2010 WonderMedia Technologies, Inc.
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

/* For UBOOT */
#include <common.h>
#include "spi_flash.h"


int spi_flash_wmt_protect(flash_info_t *info, long sector, int prot)
{
	int rc = 0, value;
	char *key = NULL;

	/*key = getenv("wmt.rsa.pem");
	if (!key) {
		return 1;
	}*/
	printf("real ");
	if (prot) {
		/* lock sf */
		if (sector == 0) {
			spi_flash_read_status(0, &value);
			if ((value&0x9C) != 0x9C)
				rc = spi_flash_write_status(0, 0x9C);
			else
				printf("chip 0 already lock\n");
		} else {
			spi_flash_read_status(1, &value);
			if ((value&0x9C) != 0x9C)
				rc = spi_flash_write_status(1, 0x9C);
			else
				printf("chip 1 already lock\n");
		}
		*(volatile unsigned char *)(GPIO_BASE_ADDR + 0xDF) &= ~0x4; /*gpio31 gpio out data*/
		*(volatile unsigned char *)(GPIO_BASE_ADDR + 0x9F) |= 0x4; /*gpio31 enable out enable*/
		*(volatile unsigned char *)(GPIO_BASE_ADDR + 0x5F) |= 0x4; /*gpio31 enable gpio mode*/
		/* printf("SF protect sec %d\n", sector); */
	} else {
		/* unlock sf */
		*(volatile unsigned char *)(GPIO_BASE_ADDR + 0xDF) |= 0x4; /*gpio31 gpio out data*/
		*(volatile unsigned char *)(GPIO_BASE_ADDR + 0x9F) |= 0x4; /*gpio31 enable out enable*/
		*(volatile unsigned char *)(GPIO_BASE_ADDR + 0x5F) |= 0x4; /*gpio31 enable gpio mode*/
		if (sector == 0) {
			spi_flash_read_status(0, &value);
			if ((value&0x9C) != 0)
				rc = spi_flash_write_status(0, 0);
			else
				printf("chip 0 already unlock\n");
		} else {
			spi_flash_read_status(1, &value);
			if ((value&0x9C) != 0)
				rc = spi_flash_write_status(1, 0);
			else
				printf("chip 1 already unlock\n");
		}
		if (rc) {
			*(volatile unsigned char *)(GPIO_BASE_ADDR + 0xDF) &= ~0x4; /*gpio31 gpio pull-high*/
			/* printf("SF unprotect fail\n", sector);*/
		}
	}
	return rc;
}



