/*++
	WM8880 eFuse char device driver

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
	2013-11-11, HowayHuo, ShenZhen
--*/
/*--- History -------------------------------------------------------------------
*     DATE          |         AUTHORS         |        DESCRIPTION
*   2014/2/11               Howay Huo             v1.0, First Release
*
*
*------------------------------------------------------------------------------*/

/*---------------------------- WM8880 eFuse Layout -------------------------------------------------------

     Type                  StartAddr   DataBytes   HammingECCBytes    OccupyBytes
Hardware Reserved              0          8              0                8
Bounding                       8          6              2                8
CPUID                          16         8              3                12
UUID                           28         24             8                32
Unused Space                   60         51             17               68

Explain:
OccupyBytes = 3aligned(DataBytes) + HammingECCBytes
eFuse Total 128 Bytes = 8 HardwareReservedBytes + 90 DataBytes + 30 ECCBytes
1 Block = 4 Bytes = 3 DataBytes + 1 ECCByte
The minimum unit of eFuse_ECC_Read/Write is 1 Block (4 Bytes)
eFuse Total 32 Block = 2 HardwareReservedBlocks + 30 AvailableBlock

---------------------------------------------------------------------------------------------------------*/

#include <common.h>
#include <malloc.h>

#include "../../board/wmt/include/common_def.h"
#include "../../board/wmt/include/wmt_iomux.h"

#include "wmt_efuse.h"

/*********************************** Constant Macro **********************************/
#define EFUSE_NAME   "efuse"

#define GPIO_BASE_ADDR                   0xD8110000

/**************************** Data type and local variable ***************************/
/*
*  WM8880 VDD25EFUSE pin is controlled by WM8880 GPIO via a MOS.
*  Pull High VDD25EFUSE pin to program eFuse. Pull Low VDD25EFUSE pin to read eFuse.
*/
static VDD25_GPIO vdd25_control_pin = {WMT_PIN_GP1_GPIO14, 1};
static int vdd25_control;

static const char *otp_type_str[] = {
	"bound",
	"cpuid",
	"uuid",
};

static int s_efuse_init;
/********************************** Function declare *********************************/
#undef DEBUG
//#define DEBUG  //if you need see the debug info, please define it.

#undef DBG

#ifdef DEBUG
#define DBG(fmt, args...) printf("[" EFUSE_NAME "] " fmt , ## args)
#else
#define DBG(fmt, args...)
#endif

#define INFO(fmt, args...) printf("[" EFUSE_NAME "] " fmt , ## args)
#define ERROR(fmt,args...) printf("[" EFUSE_NAME "]Error: " fmt , ## args)
#define WARNING(fmt,args...) printf("[" EFUSE_NAME "]Warning: " fmt , ## args)

extern int wmt_getsyspara(char *varname, char *varval, int *varlen);

/******************************** Function implement *********************************/
static void int_to_bin(unsigned int data, unsigned char *binary, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		binary[i] = data % 2;
		data = (data >> 1);
	}
}

static int bin_to_int(unsigned char *binary, unsigned int *p_data, int len)
{
	int i;
	unsigned char data = 0, *p;

	p = (unsigned char *)p_data;

	for(i = 0; i < len; i++) {
		if(binary[i] != 0 && binary[i] != 1) {
			ERROR("wrong binary[%d] = 0x%02X\n", i, binary[i]);
			return -1;
		}

		data += (binary[i] << (i % 8));
		//printk("binary[%d] = %d, data = %d\n", i, binary[i], data);

		if((i + 1) % 8 == 0) {
			*p = data;
			p++;
			data = 0;
		}
	}

	return 0;
}

/* matrix Inner product */
static unsigned char hamming_check(unsigned int data, unsigned int parity, int len)
{
	unsigned char *dst;
	unsigned char *src;
	int i, count = 0;

	dst = calloc(len, 1);
	src = calloc(len, 1);

	int_to_bin(data, dst, len);
	int_to_bin(parity, src, len);

	for (i = 0; i < len; i++) {
		if (dst[i] & src[i])
			count++;
	}

	free(dst);
	free(src);

	if (count % 2)
		return 1;
	else
		return 0;
}

static int hamming_encode_31_26(unsigned int data, unsigned int *p_hamming_data)
{
	int i, ret;
	unsigned int parity_num[5] = {0x2AAAD5B, 0x333366D, 0x3C3C78E, 0x3FC07F0, 0x3FFF800};
	unsigned int tmp;
	unsigned char encode_data[32];
	int len;
	char hamming_binary_buf[32];

	tmp = data;
	for (i = 0; i < 31; i++) {
		if (i == 0)
			encode_data[0] = hamming_check(data, parity_num[0], 26);
		else if (i == 1)
			encode_data[1] = hamming_check(data, parity_num[1], 26);
		else if (i == 3)
			encode_data[3] = hamming_check(data, parity_num[2], 26);
		else if (i == 7)
			encode_data[7] = hamming_check(data, parity_num[3], 26);
		else if (i == 15)
			encode_data[15] = hamming_check(data, parity_num[4], 26);
		else {
			if ((tmp << 31) >> 31)
				encode_data[i] = 1;
			else
				encode_data[i] = 0;
			tmp >>= 1;
		}
	}

	encode_data[31] = 0;

	len = 0;
	for(i = 30; i >= 0; i--) {
		if(encode_data[i] != 0 && encode_data[i] != 1) {
			ERROR("wrong hamming_encode_31_26, encode_data[%d] = 0x%02X\n", i, encode_data[i]);
			return -1;
		}
		len += sprintf(hamming_binary_buf + len, "%d", encode_data[i]);
	}
	hamming_binary_buf[31] = 0;

	ret = bin_to_int(encode_data, p_hamming_data, 32);
	if(ret)
		return -1;

	DBG("hamming_encode_31_26: 0x%08X ==> 0x%08X (%s)\n",
		data, *p_hamming_data, hamming_binary_buf);

	return 0;
}

static void efuse_vdd25_active(int active)
{
	if(vdd25_control == 0)
		return;

	if(active)
		gpio_direction_output(vdd25_control_pin.gpiono, vdd25_control_pin.active ? 1 : 0);
	else
		gpio_direction_output(vdd25_control_pin.gpiono, vdd25_control_pin.active ? 0 : 1);
}

static void __efuse_read_ready(void)
{
	unsigned int mode;
	unsigned int val = 0;

	/* VDD25 set low */
	efuse_vdd25_active(0);

	/* set idle*/
	mode = EFUSE_MODE_VAL;
	mode = (mode & 0xFFFFFFFC) | EFUSE_MODE_IDLE;
	EFUSE_MODE_VAL = mode;

	/* CSB set high */
	val |= EFUSE_DIR_CSB;

	/* VDDQ set high */
	val |= EFUSE_DIR_VDDQ;

	/* PGENB set low */
	val &= ~EFUSE_DIR_PGENB;

	/* STROBE set low */
	val &= ~EFUSE_DIR_STROBE;

	/* LOAD set low */
	val &= ~EFUSE_DIR_LOAD;

	EFUSE_DIR_CMD_VAL = val;

	DBG("__efuse_read_ready: vdd25 = %d, mode = 0x%08x, cmd = 0x%08x",
		vdd25_control ? gpio_get_value(vdd25_control_pin.gpiono) : -1, EFUSE_MODE_VAL, EFUSE_DIR_CMD_VAL);

	udelay(1);
}

static void __efuse_read_init(void)
{
	unsigned int mode;

	__efuse_read_ready();

	/* set DirectAccess */
	mode = EFUSE_MODE_VAL;
	mode = (mode & 0xFFFFFFFC) | EFUSE_MODE_DA;
	EFUSE_MODE_VAL = mode;

	/* VDD25 set low */
	efuse_vdd25_active(0);

	/* VDDQ set low */
	EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_VDDQ;
	udelay(1);

	/* CSB set low */
	EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_CSB;
	udelay(1);

	/* PGENB set high */
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_PGENB;
	udelay(1);

	/* LOAD set high */
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_LOAD;
	udelay(1);

	/* STROBE set low */
	//EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_STROBE;

	DBG("__efuse_read_init: vdd25 = %d, mode = 0x%08x, cmd = 0x%08x",
		vdd25_control ? gpio_get_value(vdd25_control_pin.gpiono) : -1, EFUSE_MODE_VAL, EFUSE_DIR_CMD_VAL);
}

static void __efuse_read_exit(void)
{
	unsigned int mode;

	/* LOAD set low */
	EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_LOAD;
	udelay(1);

	/* PGENB set low */
	EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_PGENB;
	udelay(1);

	/* CSB set high */
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_CSB;
	udelay(1);

	/* VDDQ set high */
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_VDDQ;
	udelay(1);

	/* VDD25 set low */
	efuse_vdd25_active(0);

	/* set idle*/
	mode = EFUSE_MODE_VAL;
	mode = (mode & 0xFFFFFFFC) | EFUSE_MODE_IDLE;
	EFUSE_MODE_VAL = mode;

	udelay(1);

	/* Standby mode */
	/* VDD25 set low */
	efuse_vdd25_active(0);

	/* VDDQ set low */
	EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_VDDQ;

	/* PGENB set high */
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_PGENB;

	/* LOAD set high */
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_LOAD;


	DBG("__efuse_read_exit\n");
}

void print_write_read_bytes(int have_title, unsigned char *p_wbuf, unsigned char *p_rbuf, int count)
{
	int i;
	unsigned char *p;

	/* print Write bytes */
	if(p_wbuf != NULL) {
		if(have_title)
			INFO("write bytes: ");

		p = p_wbuf;

		for(i = 0; i < count; i++) {

			if(have_title) {
				if(i % 16 == 0)
					printf("\n");
			} else {
				if(i != 0 && i % 16 == 0)
					printf("\n");
			}

			if(i != count - 1)
				printf("0x%02x,", *(p + i));
			else
				printf("0x%02x\n", *(p + i));
		}
	}

	/* print Read bytes */
	if(p_rbuf != NULL) {
		if(have_title)
			INFO("read bytes: ");

		p = p_rbuf;

		for(i = 0; i < count; i++) {

			if(have_title) {
				if(i % 16 == 0)
					printf("\n");
			} else {
				if(i != 0 && i % 16 == 0)
					printf("\n");
			}

			if(i != count - 1)
				printf("0x%02x,", *(p + i));
			else
				printf("0x%02x\n", *(p + i));
		}
	}
}

void print_write_read_integers(int have_title, unsigned int *p_wbuf, unsigned int *p_rbuf, int count)
{
	int i;
	unsigned int *p;

	/* print Write integers */
	if(p_wbuf != NULL) {
		if(have_title)
			INFO("write integers: ");

		p = p_wbuf;

		for(i = 0; i < count; i++) {

			if(have_title) {
				if(i % 4 == 0)
					printf("\n");
			} else {
				if(i != 0 && i % 4 == 0)
					printf("\n");
			}

			if(i != count - 1)
				printf("0x%08x,", *(p + i));
			else
				printf("0x%08x\n", *(p + i));
		}
	}

	/* print Read integers */
	if(p_rbuf != NULL) {
		if(have_title)
			INFO("read integers: ");

		p = p_rbuf;

		for(i = 0; i < count; i++) {

			if(have_title) {
				if(i % 4 == 0)
					printf("\n");
			} else {
				if(i != 0 && i % 4 == 0)
					printf("\n");
			}

			if(i != count - 1)
				printf("0x%08x,", *(p + i));
			else
				printf("0x%08x\n", *(p + i));
		}
	}
}

int efuse_read_bytes(int start, unsigned char *p_read_bytes, int count)
{
	int i, read_count;
	unsigned char *p;

	memset(p_read_bytes, 0, count);

	if(start > 127 || start < 0) {
		ERROR("efuse_read_bytes: start = %d is invalid, it should be 0 ~ 127\n", start);
		return -1;
	}

	if(start + count > 128)
		read_count = 128 - start;
	else
		read_count = count;

	__efuse_read_init();

	p  = p_read_bytes;

	for(i = start; i < start + read_count; i++) {
		/* set read address */
		EFUSE_ADR_VAL = i;

		/* STROBE set high to read data */
		EFUSE_DIR_CMD_VAL |= EFUSE_DIR_STROBE;
		udelay(1);

		/* STROBE set low */
		EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_STROBE;
		udelay(1);

		/* read data */
		*p = EFUSE_RD_DATA_VAL & 0xFF;

		p++;
	}

	__efuse_read_exit();

#ifdef DEBUG
	print_write_read_bytes(1, NULL, p_read_bytes, count);
#endif

	if(read_count != count) {
		ERROR("Need read %d bytes. In fact read %d bytes\n", count, read_count);
		print_write_read_bytes(1, NULL, p_read_bytes, count);
	}

	return read_count;
}

static void __efuse_write_ready(void)
{
	unsigned int mode;
	unsigned int val = 0;

	/* VDD25 set low */
	efuse_vdd25_active(0);

	/* set idle*/
	mode = EFUSE_MODE_VAL;
	mode = (mode & 0xFFFFFFFC) | EFUSE_MODE_IDLE;
	EFUSE_MODE_VAL = mode;

	/* CSB set high */
	val |= EFUSE_DIR_CSB;

	/* VDDQ set low */
	val &= ~EFUSE_DIR_VDDQ;

	/* PGENB set high */
	val |= EFUSE_DIR_PGENB;

	/* STROBE set low */
	val &= ~EFUSE_DIR_STROBE;

	/* LOAD set high */
	val |= EFUSE_DIR_LOAD;

	EFUSE_DIR_CMD_VAL = val;

	DBG("__efuse_write_ready: vdd25 = %d, mode = 0x%08x, cmd = 0x%08x",
		vdd25_control ? gpio_get_value(vdd25_control_pin.gpiono) : -1, EFUSE_MODE_VAL, EFUSE_DIR_CMD_VAL);

	udelay(1);
}

static void __efuse_write_init(void)
{
	unsigned int mode;

	__efuse_write_ready();

	/* set DirectAccess */
	mode = EFUSE_MODE_VAL;
	mode = (mode & 0xFFFFFFFC) | EFUSE_MODE_DA;
	EFUSE_MODE_VAL = mode;

	/* PGENB set low */
	EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_PGENB;
	udelay(1);

	/* VDD25 set high */
	efuse_vdd25_active(1);

	/* VDDQ set high*/
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_VDDQ;
	udelay(1);

	/* LOAD set low */
	EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_LOAD;
	udelay(1);

	/* CSB set low */
	EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_CSB;
	udelay(1);

	DBG("__efuse_write_init: vdd25 = %d, mode = 0x%08x, cmd = 0x%08x",
		vdd25_control ? gpio_get_value(vdd25_control_pin.gpiono) : -1, EFUSE_MODE_VAL, EFUSE_DIR_CMD_VAL);
}

static void __efuse_write_exit(void)
{
	int mode;

	/* CSB set high */
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_CSB;
	udelay(1);

	/* LOAD set high */
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_LOAD;
	udelay(1);

	/* VDD25 set low */
	efuse_vdd25_active(0);

	/* VDDQ set low */
	EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_VDDQ;
	udelay(1);

	/* PGENB set high */
	EFUSE_DIR_CMD_VAL |= EFUSE_DIR_PGENB;

	udelay(1);

	/* set idle*/
	mode = EFUSE_MODE_VAL;
	mode = (mode & 0xFFFFFFFC) | EFUSE_MODE_IDLE;
	EFUSE_MODE_VAL = mode;

	DBG("__efuse_write_exit\n");
}

int efuse_write_bytes(int start, unsigned char *p_write_bytes, int count)
{
	int i, j;
	int write_addr;
	unsigned char binary[8];
	unsigned char *p, *pbuf;

	if(start < 8 || start > 127) {
		ERROR("efuse_write_bytes: start = %d is invalid, it should be 8 ~ 127\n", start);
		return -1;
	}

	if(start + count > 128) {
		ERROR("efuse_write_bytes: byte_no range[%d - %d], it should be in range[0 - 127]\n",
			start, start + count -1);
		return -1;
	}

	pbuf = (unsigned char *)calloc(count, 1);
	if(pbuf == NULL) {
		ERROR("efuse_write_bytes: calloc buf fail\n");
		return -2;
	}

	DBG("========== read byte before write ==========\n");

	efuse_read_bytes(start, pbuf, count);

	for(i = 0; i < count; i++) {
		if(*(pbuf + i) != 0) {
			ERROR("eFuse has been programmed. Byte%d = 0x%02X. Exit.\n",
				start + i, *(pbuf + i));
			print_write_read_bytes(1, NULL, pbuf, count);
			free(pbuf);
			return -3;
		}
	}

	DBG("========== start write efuse ==========\n");

	p = p_write_bytes;

	__efuse_write_init();

	for(i = start; i < start + count; i++) {
		int_to_bin(*p, binary, 8);
		DBG("binary: %d%d%d%d%d%d%d%d\n", binary[7], binary[6], binary[5], binary[4],
			binary[3], binary[2], binary[1], binary[0]);
		for(j = 0; j < 8; j++) {
			if(binary[j]) {
				/* <data location:3 bits><eFuse address:7 bits> */
				write_addr = ((j << 7) | i );

				/* address */
				EFUSE_ADR_VAL = write_addr;

				/* STROBE set high to write data */
				EFUSE_DIR_CMD_VAL |= EFUSE_DIR_STROBE;

				/* wait 12us */
				udelay(12);

				/* STROBE set low */
				EFUSE_DIR_CMD_VAL &= ~EFUSE_DIR_STROBE;

				udelay(1);
			}
		}

		p++;
	}

	__efuse_write_exit();

	DBG("========== read byte after write ==========\n");

	efuse_read_bytes(start, pbuf, count);

	p = p_write_bytes;
	for(i = 0; i < count; i++) {
		if(*(pbuf + i) != *(p + i)) {
			ERROR("=========> eFuse Write Failed !!!\n");
			ERROR("efuse_write_bytes: Byte%d wrtie 0x%02X, But read back 0x%02X\n",
				start + i, *(p + i), *(pbuf + i));
#ifdef DEBUG
			print_write_read_bytes(1, p_write_bytes, pbuf, count);
#endif
			free(pbuf);
			return -4;
		}
	}

	DBG("=========> eFuse Write Success !!!\n");

#ifdef DEBUG
	print_write_read_bytes(1, p_write_bytes, pbuf, count);
#endif
	free(pbuf);

	return count;
}

int efuse_hamming_write_int(int start, unsigned int *p_write_int, int count)
{
	int i, ret;
	unsigned int *p_hamming_data;
	unsigned char *p_write_bytes;

	if(start > 127 || start < 8) {
		ERROR("efuse_hamming_write_int: invalid start = %d, start should be 8 ~ 127\n", start);
		return -1;
	}

	if(start % 4 != 0) {
		ERROR("efuse_hamming_write_int: invalid start = %d, start should be 4 align\n", start);
		return -1;
	}

	if(count < 1) {
		ERROR("efuse_hamming_write_int: count = %d, it must >= 1\n", count);
		return -1;
	}

	if((start + count * 4) > 128) {
		ERROR("efuse_hamming_write_int: start + count * 4 = %d, it shoud <= 128\n", start + count * 4);
		return -1;
	}

	p_hamming_data = (unsigned int *)calloc(count * 4, 1);
	if (p_hamming_data == NULL) {
		ERROR("efuse_hamming_write_int: calloc fail\n");
		return -1;
	}

	for(i = 0; i < count; i++) {
		ret = hamming_encode_31_26(*(p_write_int + i), p_hamming_data + i);
		if(ret) {
			free(p_hamming_data);
			return -1;
		}
/*
		{
			unsigned int tmp;
			tmp = *(p_hamming_data + i);
			*(p_hamming_data + i) &= 0xFF0FFFFF;
			INFO("Modify: 0x%08X ==> 0x%08X\n", tmp, *(p_hamming_data + i));
		}
*/
	}

#ifdef DEBUG
	INFO("write %d hamming data:\n", count);
	print_write_read_integers(0, p_hamming_data, NULL, count);
#endif
	p_write_bytes = (unsigned char *)p_hamming_data;

	ret = efuse_write_bytes(start, p_write_bytes, count * 4);
	if(ret != count * 4) {
		free(p_hamming_data);
		return -1;
	}

	free(p_hamming_data);

	DBG("hamming write Byte%d ~ Byte%d (%d Bytes) success\n", start, start + count * 4 - 1, count * 4);

	return count;
}

int efuse_hamming_read_int(int start, unsigned int *p_read_int, unsigned char *p_ecc_error, int count)
{
	unsigned int mode;
	int i, index;
	const int timeout = 500;
	int read_count = count;

	if(start > 127 || start < 8) {
		ERROR("efuse_hamming_read_int: start = %d is invalid, it should be 8 ~ 127\n", start);
		return -1;
	}

	if(start % 4 != 0) {
		ERROR("efuse_hamming_read_int: invalid byte_no = %d, byte_no should be 4 aligned\n", start);
		return -1;
	}

	if(read_count < 1) {
		ERROR("efuse_hamming_read_int: read_count = %d, it must >= 1\n", read_count);
		return -1;
	}

	if((start + read_count * 4) > 128) {
		WARNING("efuse_hamming_read_int: start + read_count * 4 = %d, it shoud <= 128\n", start + read_count * 4);
		while ((start + read_count * 4) > 128)
			read_count--;
	}

	/* VDD25 set low */
	efuse_vdd25_active(0);

	udelay(1);

	/* set idle*/
	mode = EFUSE_MODE_VAL;
	mode = (mode & 0xFFFFFFFC) | EFUSE_MODE_IDLE;
	EFUSE_MODE_VAL = mode;

	udelay(1);

	/* Make sure EFUSE_MODE[1:0] = 2'b00 */
	i = 0;
	while(i < timeout) {
		if((EFUSE_MODE_VAL & 0xFF) == EFUSE_MODE_IDLE)
			break;

		i++;
		DBG("efuse_hamming_read_int: wait %d ms for EFUSE_MODE idle\n", i);
		udelay(1000);
	}

	if(i == timeout) {
		ERROR("efuse_hamming_read_int fail, EFUSE_MODE couldn't be set to IDLE.\n");
		return -1;
	}

	/* Program EFUSE_MODE[30] = 1'b1 */
	EFUSE_MODE_VAL |= EFUSE_ECC_EN;
	udelay(1);

	/* Program EFUSE_MODE[31] = 1'b1 */
	EFUSE_MODE_VAL |= EFUSE_ECC_READ;
	udelay(1);

	/* Wait EFUSE_MODE[31] return back to 1'b0 */
	i = 0;
	while(i < timeout) {
		if((EFUSE_MODE_VAL & EFUSE_ECC_READ) == 0)
			break;

		i++;
		udelay(1000);
		DBG("efuse_hamming_read_int: wait %d ms for read completed\n", i);
	}

	if(i == timeout) {
		ERROR("efuse_hamming_read_int timeout\n");
		return -1;
	}

	DBG("EFUSE_ECC_STATUS_0_VAL = 0x%08X\n", EFUSE_ECC_STATUS_0_VAL);
	DBG("EFUSE_ECC_STATUS_1_VAL = 0x%08X\n", EFUSE_ECC_STATUS_1_VAL);

	index = start / 4 - 2;

	for(i = 0; i < read_count; i++) {
		if((EFUSE_ECC_STATUS_0_VAL >> index) & 0x01) {
			WARNING("ECC Error Detected in Addr %d ~ Addr %d\n", (index + 2) * 4, (index + 2) * 4 + 3);
			*(p_ecc_error + i) = 1;
		} else
			*(p_ecc_error + i) = 0;

		/* Program EFUSE_ECCSRAM_ADR. */
		EFUSE_ECCSRAM_ADR_VAL = (start + i * 4 - 8) / 4;

		/* Note: Program EFUSE_ECCSRAM_ADR again. Otherwise, the  EFUSE_ECCSRAM_RDPORT_VAL isn't updated */
		EFUSE_ECCSRAM_ADR_VAL = (start + i * 4 - 8) / 4;

		udelay(1);

		/* Read the EFUSE_ECCSRAM_RDPORT to retrieve the ECCSRAM content */
		*(p_read_int + i) = EFUSE_ECCSRAM_RDPORT_VAL & 0x3FFFFFF;

		index++;
	}

	if(read_count != count) {
		ERROR("Need read %d integers. In fact read %d integers\n", count, read_count);
		print_write_read_integers(1, NULL, p_read_int, count);
	}

	return read_count;
}

int bytes_3aligned(int bytes_num)
{
	int aligned_bytes;

	// 3 aligned
	if(bytes_num % 3)
		aligned_bytes = bytes_num + (3 - bytes_num % 3);
	else
		aligned_bytes = bytes_num;

	return aligned_bytes;
}

static int caculate_need_bytes(int available_bytes)
{
	int need_bytes;

	// 4 aligned
	need_bytes = bytes_3aligned(available_bytes) / 3 * 4;

	//INFO("caculate_need_bytes = %d\n", need_bytes);

	return need_bytes;
}

/*------------------------------------------------------------------------------
 *
 * Function: efuse_write_otp()
 * Param:
 *	type: CPUID,UUID,...,etc
 *	wbuf: write bytes
 *	wlen: how many bytes need to write
 * Return:
 *      return the write bytes number
 *      If the write bytes number is equal to the excepted bytes number, success.
 *	Otherwise, fail
 *
 *------------------------------------------------------------------------------*/
int efuse_write_otp(OTP_TYPE type, unsigned char *wbuf, int wlen)
{
	int i, ret, start, byte_len, int_len, max_byte_len;
	unsigned char *tmpbuf;
	unsigned int *p_write_int;

	wmt_efuse_init(0);

	// data length should be 3 aligned
	byte_len = bytes_3aligned(wlen);

	int_len = byte_len / 3;

	switch(type) {
		case OTP_CPUID:
			max_byte_len = EFUSE_CPUID_MAX_BYTES_NUM;
			start = EFUSE_AVAILABLE_START_BYTE + caculate_need_bytes(EFUSE_BOUND_MAX_BYTES_NUM);
		break;

		case OTP_UUID:
			max_byte_len = EFUSE_UUID_MAX_BYTES_NUM;
			start = EFUSE_AVAILABLE_START_BYTE + caculate_need_bytes(EFUSE_BOUND_MAX_BYTES_NUM)
				+ caculate_need_bytes(EFUSE_CPUID_MAX_BYTES_NUM);
		break;

		default:
			ERROR("efuse_write_otp: Not support otp type %s\n", otp_type_str[type]);
		return -1;
	}

	if(wlen > max_byte_len) {
		ERROR("efuse_write_otp(type: %s): length = %d, it should be <= %d\n",
			otp_type_str[type], wlen, max_byte_len);
		return -1;
	}

	if(start + int_len * 4 > 128) {
		ERROR("efuse_write_otp(type: %s): start + int_len * 4 = %d, it should be <= 128\n",
			otp_type_str[type], start + int_len * 4);
		return -1;
	}

#ifdef DEBUG
	INFO("%s: write %d bytes:\n", otp_type_str[type], wlen);
	print_write_read_bytes(0, wbuf, NULL, wlen);
#endif
	tmpbuf = (unsigned char *)calloc(int_len * 4, 1);
	if (tmpbuf == NULL) {
		ERROR("efuse_write_otp(type: 0x%s): calloc tmpbuf fail\n", otp_type_str[type]);
		return -1;
	}

	// check whether the efuse is programmed
	ret = efuse_read_bytes(start, tmpbuf, int_len * 4);
	if(ret != int_len * 4) {
		ERROR("efuse_write_otp(type: %s): efuse_read_bytes fail\n", otp_type_str[type]);
		free(tmpbuf);
		return -1;
	}

	for(i = 0; i < int_len * 4; i++) {
		if(tmpbuf[i] != 0) {
			ERROR("efuse_write_otp(type: %s): eFuse has been programmed. Byte%d = 0x%02X. Exit.\n",
				otp_type_str[type], start + i, *(tmpbuf + i));
			INFO("Efuse Byte%d ~ Byte%d (%d Bytes) as follows:\n",
				start, start + int_len * 4 - 1, int_len * 4);
			print_write_read_bytes(0, NULL, tmpbuf, int_len * 4);
			free(tmpbuf);
			return -1;
		}
	}

	// convert bytes to integer
	memset(tmpbuf, 0, int_len * 4);
	memcpy(tmpbuf, wbuf, wlen);

	p_write_int = (unsigned int *)calloc(int_len * 4, 1);
	if (p_write_int == NULL) {
		ERROR("efuse_write_otp(type: %s): calloc p_write_int buffer fail\n", otp_type_str[type]);
		free(tmpbuf);
		return -1;
	}

	for(i = 0; i < int_len; i++)
		p_write_int[i] = tmpbuf[i * 3 + 2] << 16 | (tmpbuf[i * 3 + 1] << 8) | tmpbuf[i * 3];

#ifdef DEBUG
	INFO("write %d raw integers:\n", int_len);
	print_write_read_integers(0, p_write_int, NULL, int_len);
#endif
	// write otp
	ret = efuse_hamming_write_int(start, p_write_int, int_len);
	if(ret != int_len) {
		ERROR("efuse_write_otp(type: %s): efuse_hamming_write_int fail\n", otp_type_str[type]);
		free(tmpbuf);
		free(p_write_int);
		return -1;
	}

	free(tmpbuf);
	free(p_write_int);

	DBG("efuse_write_otp(type: %s) success\n", otp_type_str[type]);

	return wlen;
}

/*------------------------------------------------------------------------------
 *
 * Function: efuse_read_otp()
 * Param:
 *	type: CPUID,UUID,...,etc
 *	rbuf: readback bytes
 *	rlen: how many bytes need to read
 * Return:
 *	return the read bytes number
 *      If the read bytes number is equal to the excepted bytes number, success.
 *	Otherwise, fail
 *
 *------------------------------------------------------------------------------*/
int efuse_read_otp(OTP_TYPE type, unsigned char *rbuf, int rlen)
{
	int i, ret, start, byte_len, int_len, max_byte_len;
	unsigned int *p_read_int;
	unsigned char *p_ecc_error;

	wmt_efuse_init(0);

	DBG("[%s] read %d bytes\n", otp_type_str[type], rlen);

	// data length should be 3 aligned
	byte_len = bytes_3aligned(rlen);

	int_len = byte_len / 3;

	switch(type) {
		case OTP_BOUND:
			max_byte_len = EFUSE_BOUND_MAX_BYTES_NUM;
			start = EFUSE_AVAILABLE_START_BYTE;
		break;

		case OTP_CPUID:
			max_byte_len = EFUSE_CPUID_MAX_BYTES_NUM;
			start = EFUSE_AVAILABLE_START_BYTE + caculate_need_bytes(EFUSE_BOUND_MAX_BYTES_NUM);
		break;

		case OTP_UUID:
			max_byte_len = EFUSE_UUID_MAX_BYTES_NUM;
			start = EFUSE_AVAILABLE_START_BYTE + caculate_need_bytes(EFUSE_BOUND_MAX_BYTES_NUM)
				+ caculate_need_bytes(EFUSE_CPUID_MAX_BYTES_NUM);
		break;

		default:
			ERROR("efuse_read_otp: Not support otp type %s\n", otp_type_str[type]);
		return -1;
	}

	if(rlen > max_byte_len) {
		ERROR("efuse_read_otp(type: %s): length = %d, it should be <= %d\n",
			otp_type_str[type], rlen, max_byte_len);
		return -1;
	}

	if(start + int_len * 4 > 128) {
		ERROR("efuse_read_otp(type: %s): start + int_len * 4 = %d, it should be <= 128\n",
			otp_type_str[type], start + int_len * 4);
		return -1;
	}

	p_read_int = (unsigned int *)calloc(int_len * 4, 1);
	if (p_read_int == NULL) {
		ERROR("efuse_read_otp(type: %s): calloc p_read_int buffer fail\n", otp_type_str[type]);
		return -1;
	}

	p_ecc_error = (unsigned char *)calloc(int_len * 4, 1);
	if(p_ecc_error == NULL) {
		ERROR("efuse_read_otp(type: %s): calloc p_ecc_error buffer fail\n", otp_type_str[type]);
		free(p_read_int);
		return -1;
	}

	ret = efuse_hamming_read_int(start, p_read_int, p_ecc_error, int_len);
	if(ret != int_len) {
		ERROR("efuse_read_otp(type: %s): efuse_hamming_read_int fail\n", otp_type_str[type]);
		free(p_read_int);
		free(p_ecc_error);
		return -1;
	}

#ifdef DEBUG
	INFO("efuse_read_otp: read %d integer\n", int_len);
	for(i = 0; i < int_len; i++)
		if(p_ecc_error[i] == 0)
			INFO("int%3d (Byte%3d ~ Byte%3d): 0x%08X\n",
				i, start + i * 4, start + i * 4 + 3, *(p_read_int + i));
		else
			INFO("int%3d (Byte%3d ~ Byte%3d): 0x%08X (ECC Error Detected)\n",
				i, start + i * 4, start + i * 4 + 3, *(p_read_int + i));
#endif
	for(i = 0; i < rlen; i++)
		rbuf[i] = (p_read_int[i / 3] >> ((i % 3) * 8)) & 0xFF;

#ifdef DEBUG
	print_write_read_bytes(1, NULL, rbuf, rlen);
#endif

	free(p_read_int);
	free(p_ecc_error);

	return rlen;
}

void efuse_register_dump(void)
{
	printf("VDD25 Level:                   %d\n", vdd25_control ? gpio_get_value(vdd25_control_pin.gpiono) : -1);
	printf("EFUSE_MODE:                    0x%08X\n", EFUSE_MODE_VAL);
	printf("EFUSE_ADR:                     0x%08X\n", EFUSE_ADR_VAL);
	printf("EFUSE_DIR_CMD:                 0x%08X\n", EFUSE_DIR_CMD_VAL);
	printf("EFUSE_RD_DATA:                 0x%08X\n", EFUSE_RD_DATA_VAL);
	printf("EFUSE_ECCSRAM_ADR:             0x%08X\n", EFUSE_ECCSRAM_ADR_VAL);
	printf("EFUSE_ECCSRAM_RDPORT:          0x%08X\n", EFUSE_ECCSRAM_RDPORT_VAL);
	printf("EFUSE_ECC_STATUS_0:            0x%08X\n", EFUSE_ECC_STATUS_0_VAL);
	printf("EFUSE_ECC_STATUS_1:            0x%08X\n", EFUSE_ECC_STATUS_1_VAL);

	printf("\n");

	if(vdd25_control) {
		if(vdd25_control_pin.gpiono <= WMT_PIN_GP0_GPIO7) {
			printf("GPIO_GP0_INPUT_DATA:           0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0000));
			printf("GPIO_GP0_ENABLE:               0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0040));
			printf("GPIO_GP0_OUTPUT_ENABLE:        0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0080));
			printf("GPIO_GP0_OUTPUT_DATA:          0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x00C0));
			printf("GPIO_GP0_PULL_ENABLE:          0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0480));
			printf("GPIO_GP0_PULL_UP:              0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x04C0));
		} else if(vdd25_control_pin.gpiono <= WMT_PIN_GP1_GPIO15) {
			printf("GPIO_GP1_INPUT_DATA:           0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0001));
			printf("GPIO_GP1_ENABLE:               0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0041));
			printf("GPIO_GP1_OUTPUT_ENABLE:        0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0081));
			printf("GPIO_GP1_OUTPUT_DATA:          0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x00C1));
			printf("GPIO_GP1_PULL_ENABLE:          0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0481));
			printf("GPIO_GP1_PULL_UP:              0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x04C1));
		} else if(vdd25_control_pin.gpiono <= WMT_PIN_GP2_GPIO19) {
			printf("GPIO_GP2_INPUT_DATA:           0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0002));
			printf("GPIO_GP2_ENABLE:               0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0042));
			printf("GPIO_GP2_OUTPUT_ENABLE:        0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0082));
			printf("GPIO_GP2_OUTPUT_DATA:          0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x00C2));
			printf("GPIO_GP2_PULL_ENABLE:          0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x0482));
			printf("GPIO_GP2_PULL_UP:              0x%02X\n", REG8_VAL(GPIO_BASE_ADDR + 0x04C2));
		}
	}
}


static int parse_efuse_gpio_param(char *name, VDD25_GPIO *p_gpio_pin, int *p_param_num)
{
	enum
	{
		idx_gpiono,
		idx_active,
		idx_max
	};

	char *p;
	long ps[idx_max] = {0};
	char * endp;
	int i = 0;

	p = getenv(name);
	if (!p) {
		*p_param_num = 0;
        	return -1;
	}

    	//printf("parse_efuse_gpio_param: %s\n", p);

   	while (i < idx_max) {
		ps[i++] = simple_strtoul(p, &endp, 0);

        	if (*endp == '\0')
            		break;
        	p = endp + 1;

        	if (*p == '\0')
			break;
	}

	p_gpio_pin->gpiono = ps[0];
	p_gpio_pin->active = ps[1];

	*p_param_num = i;

	return 0;
}

int wmt_efuse_init(int force)
{
	int num = 0;
	VDD25_GPIO vdd25_gpio;

	if(force)
		s_efuse_init = 0;

	if(s_efuse_init == 1)
		return 0;

	DBG("wmt_efuse_init\n");

	if(parse_efuse_gpio_param(ENV_EFUSE_GPIO, &vdd25_gpio, &num) == 0) {
		if(num == 2) {
			if(vdd25_gpio.gpiono >= WMT_PIN_GP0_GPIO0
				&& vdd25_gpio.gpiono <= WMT_PIN_GP63_SD2CD) {
				vdd25_control_pin.gpiono = vdd25_gpio.gpiono;
				vdd25_control_pin.active = vdd25_gpio.active;
				vdd25_control = 1;
			} else {
				WARNING("wrong %s. gpio_no = %d. gpio_no range should be in %d ~ %d\n",
					ENV_EFUSE_GPIO, vdd25_gpio.gpiono, WMT_PIN_GP0_GPIO0, WMT_PIN_GP63_SD2CD);
			}
		} else {
			WARNING("wrong %s. The param's num = %d. It should be equal to 2\n",
				ENV_EFUSE_GPIO, num);
		}
	}

	if(vdd25_control) {
		efuse_vdd25_active(0);
		if(vdd25_control_pin.active)
			gpio_setpull(vdd25_control_pin.gpiono, GPIO_PULL_DOWN);
		else
			gpio_setpull(vdd25_control_pin.gpiono, GPIO_PULL_UP);
	}

	/* Disable Hardware ECC */
	//EFUSE_MODE_VAL &= ~EFUSE_ECC_EN;

	s_efuse_init = 1;

	return 0;
}

