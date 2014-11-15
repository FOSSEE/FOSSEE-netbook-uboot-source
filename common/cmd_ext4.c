/*
 * (C) Copyright 2011 - 2012 Samsung Electronics
 * EXT4 filesystem implementation in Uboot by
 * Uma Shankar <uma.shankar@samsung.com>
 * Manjunatha C Achar <a.manjunatha@samsung.com>
 *
 * Ext4fs support
 * made from existing cmd_ext2.c file of Uboot
 *
 * (C) Copyright 2004
 * esd gmbh <www.esd-electronics.com>
 * Reinhard Arlt <reinhard.arlt@esd-electronics.com>
 *
 * made from cmd_reiserfs by
 *
 * (C) Copyright 2003 - 2004
 * Sysgo Real-Time Solutions, AG <www.elinos.com>
 * Pavel Bartusek <pba@sysgo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */

/*
 * Changelog:
 *	0.1 - Newly created file for ext4fs support. Taken from cmd_ext2.c
 *	        file in uboot. Added ext4fs ls load and write support.
 */

#include <common.h>
#include <part.h>
#include <config.h>
#include <command.h>
#include <image.h>
#include <linux/ctype.h>
#include <asm/byteorder.h>
#include <ext_common.h>
#include <ext4fs.h>
#include <linux/stat.h>
#include <malloc.h>

#if defined(CONFIG_CMD_USB) && defined(CONFIG_USB_STORAGE)
#include <usb.h>
#endif

extern int ext4_format(void);
int do_ext4_load(cmd_tbl_t *cmdtp, int flag, int argc,
						char *const argv[])
{
	if (do_ext_load(cmdtp, flag, argc, argv))
		return -1;

	return 0;
}

int do_ext4_ls(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	if (do_ext_ls(cmdtp, flag, argc, argv))
		return -1;

	return 0;
}


int do_ext4_write(cmd_tbl_t *cmdtp, int flag, int argc,
				char *const argv[])
{
	const char *filename = "/";
	int dev, part;
	char *ep;
	unsigned long ram_address;
	unsigned long file_size;
	disk_partition_t info;
	block_dev_desc_t *dev_desc;
	struct ext_filesystem *fs = get_fs();

	if (argc < 6) {
		printf ("Usage:\n%s\n", cmdtp->help);
		return(1);
	}
	dev = (int)simple_strtoul (argv[2], &ep, 16);
	dev_desc=get_dev(argv[1],dev);

	if (dev_desc == NULL) {
		printf ("\n** Block device %s %d not supported\n", argv[1], dev);
		return(1);
	}

	if (*ep) {
		if (*ep != ':') {
			puts ("\n** Invalid boot device, use `dev[:part]' **\n");
			return(1);
		}
		part = (int)simple_strtoul(++ep, NULL, 16);
	}


	/* get the filename */
	filename = argv[4];

	/* get the address in hexadecimal format (string to int) */
	ram_address = simple_strtoul(argv[3], NULL, 16);

	/* get the filesize in base 10 format */
	file_size = simple_strtoul(argv[5], NULL, 16);

	
	if (part == 0) {
		/* disk doesn't use partition table */
		info.start = 0;
		info.size = dev_desc->lba;
		info.blksz = dev_desc->blksz;
	} else {
		if (get_partition_info
		    (dev_desc, part, &info)) {
		}
	}

	
	if (info.size == 0) {
		printf ("** Bad partition - %s %d:%d **\n",  argv[1], dev, part);
		ext4fs_close();
		return(1);
	}

	dev = dev_desc->dev;
	/* set the device as block device */
	ext4fs_set_blk_dev(dev_desc, &info);
	fs->dev_desc = dev_desc;

	/* mount the filesystem */
	if (!ext4fs_mount(info.size)) {
		printf("Bad ext4 partition %s %d:%lu\n", argv[1], dev, part);
		goto fail;
	}

	/* start write */
	if (ext4fs_write(filename, (unsigned char *)ram_address, file_size)) {
		printf("** Error ext4fs_write() **\n");
		goto fail;
	}
	ext4fs_close();

	return 0;

fail:
	ext4fs_close();

	return 1;

}

int do_ext4_format(cmd_tbl_t *cmdtp, int flag, int argc,
				char *const argv[])
{	
	int dev=0;
	int part=1;
	int partition=1;
	char *ep;
	block_dev_desc_t *dev_desc=NULL;
	disk_partition_t info;
	struct ext_filesystem *fs = get_fs();

	if (argc < 3) {
		printf ("Usage:\n%s\n", cmdtp->help);
		return(1);
	}
	dev = (int)simple_strtoul (argv[2], &ep, 16);
	dev_desc=get_dev(argv[1],dev);

	if (dev_desc == NULL) {
		printf ("\n** Block device %s %d not supported\n", argv[1], dev);
		return(1);
	}
	
	if (*ep) {
		if (*ep != ':') {
			puts ("\n** Invalid boot device, use `dev[:part]' **\n");
			return(1);
		}
		part = (int)simple_strtoul(++ep, NULL, 16);
		
	}

	
	if (part == 0) {
		/* disk doesn't use partition table */
		info.start = 0;
		info.size = dev_desc->lba;
		info.blksz = dev_desc->blksz;
	} else {
		if (get_partition_info
		    (dev_desc, part, &info)) {
		}
	}
	
	
	if (info.size == 0) {
		printf ("** Bad partition - %s %d:%d **\n",  argv[1], dev, part);
		ext4fs_close();
		return(1);
	}

	dev = dev_desc->dev;
	/* set the device as block device */
	ext4fs_set_blk_dev(dev_desc, &info);
	fs->dev_desc = dev_desc;

	return ext4_format();
	
}



U_BOOT_CMD(mkfsext4, 3, 1, do_ext4_format,
	   "format the partition to ext4",
       "<interface> <dev:part> \n"
       "       -format a partition to ext4");

U_BOOT_CMD(ext4write, 6, 1, do_ext4_write,
	"create a file in the root directory",
	"<interface> <dev:part> <Address(hex)> <Absolute filename path> <sizebytes(hex)>\n"
	"	  - create a file in / directory");



U_BOOT_CMD(ext4ls,	4,	1,	do_ext4_ls,
	"list files in a directory (default /)",
	"<interface> <dev:part> [directory]\n"
	"	   - list files from 'dev' on 'interface' in a 'directory'");


U_BOOT_CMD(ext4load, 6, 0, do_ext4_load,
	   "load binary file from a Ext4 filesystem",
	   "<interface> <dev:part> <addr> <filename> [bytes]\n"
	   "	  - load binary file 'filename' from 'dev' on 'interface'\n"
	   "		 to address 'addr' from ext4 filesystem");