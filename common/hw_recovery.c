/*++
Copyright (c) 2010 WonderMedia Technologies, Inc.

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details. You
should have received a copy of the GNU General Public License along with this
program. If not, see http://www.gnu.org/licenses/>.

WonderMedia Technologies, Inc.
cheney chen mce Shenzhen china

Modifed by howayhuo to use gpio lib
--*/

#include <common.h>
#include <linux/mtd/nand.h>
#include <mmc.h>
#include <part.h>

#include "../board/wmt/include/wmt_pmc.h"
#include "../board/wmt/include/wmt_iomux.h"
#include "../board/wmt/include/common_def.h"

/*
* Format:
*     setenv wmt.hardware.recovery enable:gpiono:active:delay
* Example:
*     Press PowerKey and GPIO8(volume+) key in the same time to force enter fastboot
*     The GPIO8 is low level when pressing the volume+ key
*     Long press power key for 1 second to conform enter recovery
*
* setenv wmt.recovery.key 1:8:0:10
*/
#define ENV_RECOVERY_KEY "wmt.recovery.key"

#define ENV_RECOVERY_NAME "wmt.recovery.enable"

#define TIMEOUT_SECONDS4 40    //100ms * 40
#define TIMEOUT_SECONDS10 100    //100ms * 100
//#define HW_RECOVERY_DEBUG

#define ENV_MISC_PART "misc-TF_part"
#define ENV_MISC_OFFSET "misc-NAND_ofs"
#define WMT_BOOT_DEV "wmt.boot.dev"

struct recovery_key_env_t {
    int enable;   // 1: enable the function of pressing key to do recovery. 0: don't enable
    int gpiono;   // the gpio no of gpiokey
    int active;   // 1: the gpio level is 1 when the gpiokey is pressed. 0: the gpio level is 0 when the gpiokey is pressed
    int delay;    // how long time to press power key to confirm enter recovery: (delay x 100) ms
};

static struct recovery_key_env_t default_recovery_key = {
    //power and volume+ in default
    .enable   = 1,
    .gpiono   = 8,
    .active = 0,
    .delay    = 10,   // long press power key for 1 second to conform
};

struct bootloader_message {
    char command[32];
    char status[32];
    char recovery[1024];
};

extern int WMTSaveImageToNAND(struct nand_chip *nand, unsigned long long naddr, unsigned int dwImageStart,
        unsigned int dwImageLength, int oob_offs, unsigned int need_erase);
struct nand_chip* get_current_nand_chip(void);


static const int NAND_MISC_COMMAND_PAGE = 1;  // bootloader command is this page

static int nand_set_bootloader_message(const struct bootloader_message *in){
    char * ofsStr;
    unsigned long miscOff;
    struct nand_chip * nand = NULL;
    unsigned long nandOff;

    if(in == NULL){
        printf("invalid argument\n");
        return -1;
    }

    if((ofsStr = getenv(ENV_MISC_OFFSET)) == NULL){
        printf("%s is undefined.\n", ENV_MISC_OFFSET);
        return -1;
    }

    miscOff = simple_strtoul(ofsStr, NULL, 16);

    if(miscOff == 0){
        printf("Invalid misc offset %x(%s)\n", miscOff, ofsStr);
        return -1;
    }

    nand = get_current_nand_chip();
    if(nand == NULL){
        printf("Current nand chip is NULL\n");
        return -1;
    }

    nandOff = miscOff + NAND_MISC_COMMAND_PAGE * nand->dwPageSize;

    printf("set bootloader message , nand off = 0x%x(%x, %x), memory off = %p, size = 0x%x\n",
            nandOff, miscOff, nand->dwPageSize, in, sizeof(struct bootloader_message));

    return WMTSaveImageToNAND(nand, nandOff, (unsigned int)in, sizeof(struct bootloader_message), 0, 1);
}

extern block_dev_desc_t *get_dev (char*, int);

static  unsigned long mmc_part_offset =0;

int mmc_register_device_recovery(block_dev_desc_t *dev_desc, int part_no)
{
    unsigned char buffer[0x200];
    disk_partition_t info;

    if (!dev_desc->block_read)
        return -1;

    /* check if we have a MBR (on floppies we have only a PBR) */
    if (dev_desc->block_read (dev_desc->dev, 0, 1, (ulong *) buffer) != 1) {
        printf ("** Can't read from device %d **\n", dev_desc->dev);
        return -1;
    }

    if(!get_partition_info(dev_desc, part_no, &info)) {
            mmc_part_offset = info.start;
            printf("part_offset : %x, cur_part : %x\n", mmc_part_offset, part_no);
    } else {
#if 1
        printf ("** Partition %d not valid on device %d **\n",part_no,dev_desc->dev);
        return -1;
#else

        /* FIXME we need to determine the start block of the
         * partition where the boot.img partition resides. This can be done
         * by using the get_partition_info routine. For this
         * purpose the libpart must be included.
         */
        part_offset=32;
        cur_part = 1;
#endif
    }
    return 0;
}

static int mmc_set_bootloader_message(const struct bootloader_message *in){
    char *partStr;
    unsigned long partNo=0;
    unsigned long deviceId;
    unsigned long writeSize, writeCnt;
    block_dev_desc_t *devDesc=NULL;
    char *ep;
    int ret;

    if((partStr = getenv(ENV_MISC_PART)) == NULL){
        printf("%s is undefined.\n", ENV_MISC_PART);
        return -1;
    }

    deviceId = simple_strtoul(partStr, &ep, 16);

    if (deviceId < 0 || deviceId > 3){
        printf("Invalid device Id %d\n", deviceId);
        return -1;
    }

    /*get mmc dev descriptor*/
    devDesc=get_dev("mmc", deviceId);
    if (devDesc == NULL) {
        printf("\n** Invalid boot device **\n");
        return 1;
    }

    if (*ep) {
        if (*ep != ':') {
            puts ("\n** Invalid boot device, use `dev[:part]' **\n");
            return -1;
        }
        partNo = (int)simple_strtoul(++ep, NULL, 16);
    }

    /* init mmc controller */
    if (mmc_init(1, deviceId)) {
        printf("mmc init failed?\n");
        return -1;
    }
    if (mmc_register_device_recovery(devDesc, partNo) != 0) {
        printf ("\n** Unable to use mmc %d:%d for fatload **\n", deviceId, partNo);
        return -1;
    }

    writeSize = sizeof(struct bootloader_message);
    writeCnt = writeSize/512 + (writeSize % 512)?1:0;

    ret = mmc_bwrite(deviceId, mmc_part_offset, writeCnt, (ulong*)in);
    printf("set bootloader message, deviceId = %d, partId = %d, part_off=0x%x, write block cnt=%d, memory off = %p, ret = %d\n",
           deviceId, partNo, mmc_part_offset, writeCnt, in, ret);

    return ret;
}


static int set_bootloader_message(const struct bootloader_message *in){
    char *bootDev;
    if((bootDev = getenv(WMT_BOOT_DEV)) == NULL){
         printf("%s is undefined.\n", WMT_BOOT_DEV);
         return -1;
    }

    if(strcmp(bootDev, "NAND") == 0){
        return nand_set_bootloader_message(in);
    }else if(strcmp(bootDev, "TF") == 0){
       return mmc_set_bootloader_message(in);
    }

    printf("boot %s is not supported.\n", bootDev);
    return -1;
}


static void boot_into_recovery(void){
    struct bootloader_message msg;

    memset(&msg, 0xff, sizeof(struct bootloader_message));
    strcpy(msg.command, "boot-recovery");
    strcpy(msg.recovery, "recovery\n--wipe_data\n--locale=en_US\n--execute_script=/system/.restore/restore.sh");


    if(set_bootloader_message(&msg) >=0 ){
        run_command("textout 10 65 \"Boot into system recovery mode... \" 0xff0000;", 0);
        run_command("run boot-nand-ota-recovery", 0);
        run_command("run boot-kernel", 0);
    }
}

static int parse_recovery_key_env(char *name, struct recovery_key_env_t *p_env)
{
    enum
    {
        idx_enable,
        idx_gpiono,
        idx_active,
        idx_delay,
        idx_max
    };

    char *p;
    long ps[idx_max] = {0};
    char *endp;
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
    p_env->gpiono = ps[1];
    p_env->active = ps[2];
    p_env->delay = ps[3];

    if(i != 4) {
        printf("parse_recovery_key_env: wrong env num in %s\n", name);
        return -1;
    }

    if(p_env->enable == 0)
    return 0;

    if(gpio_is_valid(p_env->gpiono))
        return 0;
    else {
        printf("parse_recovery_key_env: wrong gpio no in %s\n", name);
        return -1;
    }
}

int hw_recovery(void)
{
    int i;
    char *s;
    int timeout = 0;
    int power_btn_press = 0;
    int gpio_btn_press = 0;
    int times = 0;
    int ret = 0;
    struct recovery_key_env_t recovery_key;

    //decrease the delay time for system boot
    if((s = getenv(ENV_RECOVERY_NAME)) != NULL && !strcmp(s, "0"))
    {
        printf("No recovery.\n");
        return ret;
    }

    ret = parse_recovery_key_env(ENV_RECOVERY_KEY, &recovery_key);
    if(ret)
        memcpy(&recovery_key, &default_recovery_key, sizeof(struct recovery_key_env_t));

    if(recovery_key.enable == 0)
        return -1;

    if(!(PMPB_VAL & BIT24))
            return -1;

    if(recovery_key.active != 0)
        recovery_key.active = 1;

    if(recovery_key.delay > 30) {
        recovery_key.delay = 30;
        printf("limit recovery_key.delay to 30\n");
    }

    gpio_direction_input(recovery_key.gpiono);
    gpio_setpull(recovery_key.gpiono, recovery_key.active? GPIO_PULL_DOWN : GPIO_PULL_UP);

    //commbined key, power button and vol+ in default
    power_btn_press = PMPB_VAL & BIT24;
    gpio_btn_press = (gpio_get_value(recovery_key.gpiono) == recovery_key.active);

    if(!power_btn_press || !gpio_btn_press)
        return -1;

    //check press commbined key for 1 seconds
    for(i = 0; i < 10; i++) {
        if(i != 0)
            udelay(100000);

        power_btn_press = PMPB_VAL & BIT24;
        if(!power_btn_press)
            return -1;

        gpio_btn_press = (gpio_get_value(recovery_key.gpiono) == recovery_key.active);
        if(!gpio_btn_press)
            return -1;
        }

    timeout = 0;
    times = 0;

    run_command("textout 10 5 \"Are you sure system recovery?\" 0xff0000", 0);
    run_command("textout 10 25 \"Long press power button again to enter system recovery.\" 0xff0000", 0);

    while(timeout < TIMEOUT_SECONDS10) {
        power_btn_press = PMPB_VAL & BIT24;
        gpio_btn_press = (gpio_get_value(recovery_key.gpiono) == recovery_key.active);

        timeout++;

#ifdef HW_RECOVERY_DEBUG
        printf("times = %d, timeout = %d\n", times, timeout);
#endif
        //for user's a mistake
        if(timeout >= TIMEOUT_SECONDS10) {
            //clean the text
            run_command("textout 10 5 \" \" 0xff0000", 0);
            run_command("textout 10 25 \" \" 0xff0000", 0);
        }

        if(gpio_btn_press) {
            udelay(100000);
            continue;
        }

        if(power_btn_press)
            times++;

        if(times >= recovery_key.delay) { // 100ms * recovery_key.delay = xx seconds
            run_command("textout 10 45 \"Recovery confirmed.\" 0xffff00;", 0);
            boot_into_recovery();
            return 0;
        }
        udelay(100000);
    }

    return 0;
}

