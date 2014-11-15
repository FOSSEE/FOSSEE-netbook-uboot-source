/*++
	The command to read/write efuse

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
	2014-2-11, HowayHuo, ShenZhen
--*/

#include <common.h>
#include <command.h>
#include <malloc.h>

#include "wmt_efuse.h"

/*********************************** Constant Macro **********************************/
#define EFUSE_NAME   "efuse"

//#define EFUSE_WRITE_THROUGH_CMD
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

/******************************** Function implement *********************************/

static void efuse_rawdata_show(void)
{

	int i;
	unsigned char read_bytes[128];

	efuse_read_bytes(0, read_bytes, 128);

	printf("efuse raw data:");
	for(i = 0; i < 128; i++) {

		if(i % 16 == 0)
			printf("\n");

		if(i != 128 - 1)
			printf("0x%02X,", *(read_bytes + i));
		else
			printf("0x%02X\n", *(read_bytes + i));
	}
}

static void efuse_hammingdata_show(void)
{
	int i;
	unsigned int read_int[30] = {0}; // 120 / 4 = 30
	unsigned char ecc_error[30] = {0};

	efuse_hamming_read_int(8, read_int, ecc_error, 30);

	printf("\nefuse hamming data:\n");
	for(i = 0; i < 30; i++) {
		printf("Byte%3d ~ Byte%3d: 0x%08X",
			8 + i * 4, 8 + i * 4 + 3, read_int[i]);
		if(ecc_error[i] == 0)
			printf("\n");
		else
			printf(" ECC Error Detected\n");
	}

	printf("\n");
}

static int efuse_cmd_read_bytes(int start, int byte_num)
{
	int ret;
	unsigned char *p_read_bytes;

	if(byte_num == 0) {
		ERROR("read num can't be set to 0\n");
		return 1;
	}

	p_read_bytes = calloc(byte_num, 1);
	if(p_read_bytes == NULL) {
		ERROR("calloc fail\n");
		return 1;
	}

	ret = efuse_read_bytes(start, p_read_bytes, byte_num);
	if(ret > 0 && ret <= byte_num)
		print_write_read_bytes(1, NULL, p_read_bytes, ret);

	free(p_read_bytes);

	return 0;
}

static int efuse_cmd_read_hamming(int start, int int_num)
{
	int i, ret;
	unsigned int *p_read_int;
	unsigned char *p_ecc_error;

	if(int_num == 0) {
		ERROR("read num can't be set to 0\n");
		return 1;
	}

	p_read_int = calloc(int_num * 4, 1);
	if(p_read_int == NULL) {
		ERROR("calloc p_read_int buffer fail\n");
		return 1;
	}

	p_ecc_error = (unsigned char *)calloc(int_num * 4, 1);
	if(p_ecc_error == NULL) {
		ERROR("calloc p_ecc_error buffer fail\n");
		free(p_read_int);
		return 1;
	}

	ret = efuse_hamming_read_int(start, p_read_int, p_ecc_error, int_num);
	if(ret > 0 && ret <= int_num) {
		INFO("read %d integer\n", ret);
		for(i = 0; i < ret; i++)
			if(p_ecc_error[i] == 0)
			INFO("int%3d (Byte%3d ~ Byte%3d): 0x%08X\n",
				i, start + i * 4, start + i * 4 + 3, *(p_read_int + i));
		else
			INFO("int%3d (Byte%3d ~ Byte%3d): 0x%08X (ECC Error Detected)\n",
				i, start + i * 4, start + i * 4 + 3, *(p_read_int + i));
	}

	free(p_read_int);
	free(p_ecc_error);

	return 0;
}

static int efuse_cmd_read_otp(OTP_TYPE type, int byte_num)
{
	int ret;
	unsigned char *p_read_bytes;

	if(byte_num == 0) {
		ERROR("read num can't be set to 0\n");
		return 1;
	}

	p_read_bytes = calloc(byte_num, 1);
	if(p_read_bytes == NULL) {
		ERROR("calloc fail\n");
		return 1;
	}

	ret = efuse_read_otp(type, p_read_bytes, byte_num);

	free(p_read_bytes);

	if(ret == byte_num)
		return 1;
	else
		return 0;
}

#ifdef EFUSE_WRITE_THROUGH_CMD

static int efuse_cmd_write_bytes(int start, const char *p_byte_str)
{
	int i, ret, byte_num;
	unsigned long value;
	unsigned char *p_write_bytes;
	const char *p;
	char *endp;

	if(start > 127 || start < 8) {
		ERROR("invalid start = %d, start should be 8 ~ 127\n", start);
		return 1;
	}

	p_write_bytes = calloc(128, 1);
	if(p_write_bytes == NULL) {
		ERROR("calloc fail\n");
		return 1;
	}

	p = p_byte_str;
	byte_num = 0;

	for(i = 0; i < 128; i++) {
		if(*p < '0' || *p > '9') {
			ERROR("wrong input format. wrong str1 = \"%s\"\n", p);
			free(p_write_bytes);
			return 1;
		}

		value = simple_strtoul(p, &endp, 0);
		if(value > 0xFF) {
			ERROR("byte_val = 0x%lx > 0xFF. wrong str = \"%s\"\n", value,  p);
			free(p_write_bytes);
			return 1;
		}

		*(p_write_bytes + i) = value & 0xFF;

		byte_num++;

		if(*endp == '\0' || *endp == '\n')
			break;
		else if(*endp != ',') {
			ERROR("wrong input format. wrong str2 = \"%s\"\n", endp);
			free(p_write_bytes);
			return 1;
		}

		p = endp + 1;
	}

	if(start + byte_num > 128) {
		ERROR("start = %d, byte_num = %d, start + byte_num > 128\n", start, byte_num);
		free(p_write_bytes);
		return 1;
	}

	INFO("start: %d, byte_num = %d\n", start, byte_num);
	print_write_read_bytes(1, p_write_bytes, NULL, byte_num);

	ret = efuse_write_bytes(start, p_write_bytes, byte_num);
	if(ret == byte_num)
		INFO("eFuse Write Success !!!\n");

	free(p_write_bytes);

	return 0;
}

static int efuse_cmd_write_hamming(int start, const char *p_int_str)
{
	int i, int_num;
	const char *p;
	char *endp;
	unsigned long value;
	unsigned int *p_write_ints;

	if(start > 127 || start < 8) {
		ERROR("invalid start = %d, start should be 8 ~ 127\n", start);
		return 1;
	}

	p_write_ints = calloc(30 * 4, 1);
	if(p_write_ints == NULL) {
		ERROR("calloc fail\n");
		return 1;
	}

	p = p_int_str;
	int_num = 0;

	for(i = 0; i < 30; i++) {
		if(*p < '0' || *p > '9') {
			ERROR("wrong input format. wrong str1 = \"%s\"\n", p);
			free(p_write_ints);
			return 1;
		}

		value = simple_strtoul(p, &endp, 0);

		*(p_write_ints + i) = (unsigned int)value;

		int_num++;

		if(*endp == '\0' || *endp == '\n')
			break;
		else if(*endp != ',') {
			ERROR("wrong input format. wrong str2 = \"%s\"\n", endp);
			free(p_write_ints);
			return 1;
		}

		p = endp + 1;
	}

	INFO("start: %d, int_num = %d\n", start, int_num);

	efuse_hamming_write_int(start, p_write_ints, int_num);

	free(p_write_ints);

	return 0;
}

static int efuse_cmd_write_otp(OTP_TYPE type, const char *p_byte_str)
{
	int i, byte_num, offset = 0, max_byte_len = EFUSE_CPUID_MAX_BYTES_NUM;
	OTP_TYPE otp_type = OTP_CPUID;
	const char *p;
	char *endp;
	unsigned char *p_write_bytes;
	unsigned long value;

	switch(type) {
		case OTP_CPUID:
			otp_type = OTP_CPUID;
			max_byte_len = EFUSE_CPUID_MAX_BYTES_NUM;
			offset = 5;
		break;

		case OTP_UUID:
			otp_type = OTP_UUID;
			max_byte_len = EFUSE_UUID_MAX_BYTES_NUM;
			offset = 4;
		break;

		default:
			ERROR("wrong descriptor. format should be \"cpuid(uuid) val1,val2,...\"\n");
		return 1;
	}

	p_write_bytes = (unsigned char *)calloc(128, 1);
	if(p_write_bytes == NULL) {
		ERROR("write_otp_store: kzalloc fail\n");
		return 1;
	}

	p = p_byte_str;
	byte_num = 0;

	for(i = 0; i < 128; i++) {
		if(*p < '0' || *p > '9') {
			ERROR("wrong input format. wrong str1 = \"%s\"\n", p);
			free(p_write_bytes);
			return 1;
		}

		value = simple_strtoul(p, &endp, 0);
		if(value > 0xFF) {
			ERROR("byte_val = 0x%lx > 0xFF. wrong str = \"%s\"\n", value,  p);
			free(p_write_bytes);
			return 1;
		}

		*(p_write_bytes + i) = value & 0xFF;

		byte_num++;

		if(*endp == '\0' || *endp == '\n')
			break;
		else if(*endp != ',') {
			ERROR("wrong input format. wrong str2 = \"%s\"\n", endp);
			free(p_write_bytes);
			return 1;
		}

		p = endp + 1;
	}

	if(byte_num > max_byte_len) {
		ERROR("byte_num = %d, opt_type(%d) byte_num can't larger than %d\n",
			byte_num, otp_type, max_byte_len);
		free(p_write_bytes);
		return 1;
	}

	efuse_write_otp(otp_type, p_write_bytes, byte_num);

	free(p_write_bytes);

	return 0;
}

#endif

static int wmt_efuse_main (cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	int start, num;
	char *endp;

	switch (argc) {
		case 0:
		case 1:
		break;

		case 2:
			if (strncmp(argv[1], "init", 1) == 0) {
				//for example: efuse init
				wmt_efuse_init(0);
				return 0;
			} else if (strncmp(argv[1], "reg", 3) == 0) {
				//for example: efuse reg
				efuse_register_dump();
				return 0;
			}
		break;

		default: /* at least 3 args */
			if (strncmp(argv[1], "init", 1) == 0) {
				if (strcmp(argv[2], "force") == 0) {
					wmt_efuse_init(1);
					return 0;
				}
			} else if (strncmp(argv[1], "r", 1) == 0) {
				if (strncmp(argv[2], "raw", 1) == 0) {
					//for example: efuse r raw
					efuse_rawdata_show();
					return 0;
				} else if (strncmp(argv[2], "ham", 1) == 0) {
					if(argc < 5) {
						//for example: efuse r ham
						efuse_hammingdata_show();
						return 0;
					} else {
						/*
						for example: efuse r ham 60 5
						read 5 integers from byte60 to bytes79
						*/
						start = simple_strtoul(argv[3], &endp, 0);
						num = simple_strtoul(argv[4], &endp, 0);

						return efuse_cmd_read_hamming(start, num);

					}
				} else if (strncmp(argv[2], "byte", 1) == 0) {
					if(argc >= 5) {
						/*
						for example: efuse r byte 33 12
						read 12 bytes from byte33 to bytes44
						*/
						start = simple_strtoul(argv[3], &endp, 0);
						num = simple_strtoul(argv[4], &endp, 0);

						return efuse_cmd_read_bytes(start, num);
					}
				} else if(strcmp(argv[2], "cpuid") == 0) {
					if(argc >= 4) {
						/*
						for example: efuse r cpuid 3
						read 3 bytes cpuid
						*/
						num = simple_strtoul(argv[3], &endp, 0);
						return efuse_cmd_read_otp(OTP_CPUID, num);
					}
				} else if (strcmp(argv[2], "uuid") == 0) {
					if(argc >= 4) {
						/*
						for example: efuse r uuid 4
						read 4 bytes uuid
						*/
						num = simple_strtoul(argv[3], &endp, 0);
						return efuse_cmd_read_otp(OTP_UUID, num);
					}
				} else if (strncmp(argv[2], "bound", 5) == 0) {
					/*
					for example: efuse r bound 4
					read 4 bytes bound
					*/
					num = simple_strtoul(argv[3], &endp, 0);
					return efuse_cmd_read_otp(OTP_BOUND, num);
				}
		}
#ifdef EFUSE_WRITE_THROUGH_CMD
		else if (strncmp(argv[1], "w", 1) == 0) {
			if (strncmp(argv[2], "byte", 1) == 0) {
				if(argc >= 5) {
					/*
					for example: efuse w byte 92 0x42,0x87
					write 2 bytes [0x42,0x87] to byte92 ~ bytes93
					*/
					start = simple_strtoul(argv[3], &endp, 0);
					return efuse_cmd_write_bytes(start, argv[4]);
				}

			} else if (strncmp(argv[2], "ham", 1) == 0) {
				if(argc >= 5) {
					/*
					for example: efuse w ham 96 0x12345678,0x89abcdef
					write 2 integers [0x12345678,0x89abcdef] to byte96 ~ bytes103
					*/
					start = simple_strtoul(argv[3], &endp, 0);
					return efuse_cmd_write_hamming(start, argv[4]);
				}
			} else if (strcmp(argv[2], "cpuid") == 0) {
				/*
				for example: efuse w cpuid 0x92,0xc8,0x3d
				write 3 bytes cpuid
				*/
				return efuse_cmd_write_otp(OTP_CPUID, argv[3]);
			} else if (strcmp(argv[2], "uuid") == 0) {
				/*
				for example: efuse w uuid 0x2b,0xc3,0x65
				write 3 bytes uuid
				*/
				return efuse_cmd_write_otp(OTP_UUID, argv[3]);
			}
		}
#endif
		break;
	}

	printf ("Usage:\n%s\n", cmdtp->usage);
	return 1;
}


U_BOOT_CMD(
	efuse,	5,	1,	wmt_efuse_main,
	"efuse  - Efuse sub-system\n",
	NULL
);

