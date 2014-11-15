/*++ 
Copyright (c) 2012 WonderMedia Technologies, Inc.

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software 
Foundation, either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details. You
should have received a copy of the GNU General Public License along with this
program. If not, see http://www.gnu.org/licenses/>.

WonderMedia Technologies, Inc.
4F, 531, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C.
--*/

#include <config.h>
#include <common.h>
#include <command.h>
#include <version.h>
#include <stdarg.h>
#include <linux/types.h>
#include <devices.h>
#include <linux/stddef.h>
#include <asm/byteorder.h>
#include <mmc.h>
#include <nand.h>



int wmt_fs_read(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	unsigned long dev = 0;
	unsigned long addr = 0;
	unsigned long bytes = 0;
	int status = 0;


	if (argc < 5) {
		printf("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}

	if ((strcmp(argv[4], "wx-load.bin") != 0) &&
	    (strcmp(argv[4], "wmt-env.bin") != 0) &&
	    (strcmp(argv[4], "u-boot.bin") != 0)) {
	
		printf("<filename> is not correct.\n");
		return -1;
	}

	dev = simple_strtoul(argv[2], NULL, 16);
	addr = simple_strtoul(argv[3], NULL, 16);

	if (argc == 6)
		bytes = simple_strtoul(argv[5], NULL, 16);
	
	if (strcmp(argv[1], "mmc") == 0) {		
		status = mmc_wfs_read(dev, addr, argv[4], bytes);
		
	} else if (strcmp(argv[1], "nand") == 0) {
		// modified by howayhuo
		//status = wmt_nand_read(dev, addr, argv[4], bytes);
		status = 0;
	} else {
		printf("<interface> is not correct.\n");
	}
	

	return status;
}

int wmt_fs_write(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	unsigned long dev = 0;
	unsigned long addr = 0;
	unsigned long bytes = 0;
	int status = 0;


	if (argc < 6) {
		printf("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}


	if ((strcmp(argv[4], "wx-load.bin") != 0) &&
	    (strcmp(argv[4], "u-boot.bin") != 0) &&
	    (strcmp(argv[4], "wmt-env.bin") != 0)) {

		printf("<filename> is not correct.\n");
		return -1;
	} 
	
	dev = simple_strtoul(argv[2], NULL, 16);
	addr = simple_strtoul(argv[3], NULL, 16);
	bytes = simple_strtoul(argv[5], NULL, 16);

	if (strcmp(argv[1], "mmc") == 0) {
		status = mmc_wfs_write(dev, addr, argv[4], bytes);
		
	} else if (strcmp(argv[1], "nand") == 0) {
		// modified by howayhuo
		//status = wmt_nand_write(dev, addr, argv[4], bytes); 
		status = 0;
	} else {
		printf("<interface> is not correct.\n");
		return -1;
	}

	return status;

}

U_BOOT_CMD(
	wfsread,	6,	1,	wmt_fs_read,
	"wfsread - This command is used to read the files located in wmtfs partition.\n"
	"  <interface> <dev> <addr> <filename> [<bytes>]\n",
	
	"wfsread - This command is used to read the files located in wmtfs partition.\n"
	"  <interface> <dev> <addr> <filename> [<bytes>]\n"
	"  [S]<interface>:= The interface to read file including 'mmc' or 'nand'.\n"
	"  [H]<dev>:= The controller number.\n" 
	"     If <interface>='mmc', then 0 indicates sdc0, 1 indicated sdc1.\n"
	"     If <interface>='nand', then dev should be set to 0.\n"
	"  [H]<addr>:= The image memory location.\n"
	"  [S]<filename>:= filename to be read(Max filename size is 16 char).\n"
	"  [H][<bytes>]:= Optional. How many bytes to be read if specified.\n"
);

U_BOOT_CMD(
	wfswrite,	6,	1,	wmt_fs_write,
	"wfswrite - This command is used to write file to wmtfs partition.\n"
	"  <interface> <dev> <addr> <filename> <bytes>\n",
	
	"wfswrite - This command is used to write file to wmtfs partition.\n"
	"  <interface> <dev> <addr> <filename> <bytes>\n"
	"  [S]<interface>:= The interface to write file including 'mmc' or 'nand'.\n"
	"  [H]<dev>:= The controller number.\n"
	"     If <interface>='mmc', then 0 indicates sdc0, 1 indicated sdc1.\n"
	"     If <interface>='nand', then dev should be set to 0.\n"
	"  [H]<addr>:= The image memory location.\n"
	"  [S]<filename>:= filename to be write(Max filename size is 16 char).\n"
	"  [H]<bytes>:= How many bytes to be written."
);


