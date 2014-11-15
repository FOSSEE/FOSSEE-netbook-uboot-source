/*++
	The WM8880 header file of eFuse driver

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

#ifndef __WMT_EFUSE_H
#define __WMT_EFUSE_H

/******************************************************************************
 *
 * Define the register access macros.
 *
 * Note: Current policy in standalone program is using register as a pointer.
 *
 ******************************************************************************/

/******************************************************************************
 *
 * WM8880 GPIO Base Address.
 *
 ******************************************************************************/
//#define GPIO_BASE_ADDR                   0xD8110000

/******************************************************************************
 *
 * WM8880 eFuse Controller Base Address.
 *
 ******************************************************************************/
#define __EFUSE_BASE_ADDR                0xD83E0000

/******************************************************************************
 *
 * WM8880 eFuse Controller registers.
 *
 * Registers Abbreviations:
 *
 * EFUSE_MODE_REG             eFuse Mode Register.
 *
 * EFUSE_ADR_REG              eFuse Address Register.
 *                            It is used on Direct and Embedded R/W
 *
 * EFUSE_DIR_CMD_REG          eFuse Direct Access Command Register.
 *
 * EFUSE_RD_DATA_REG          eFuse Read Out Data Register.
 *
 * EFUSE_ECCSRAM_ADR_REG      eFuse Read Address of the 30 x 26  SRAM Register.
 *                            It stores ECC decoded eFuse content
 *
 * EFUSE_ECCSRAM_RDPORT_REG   eFuse ECCSRAM Read Out Data register.
 *
 * EFUSE_ECC_STATUS_0_REG     eFuse ECC Error Detected and Corrected Status 0 Register.
 *
 * EFUSE_ECC_STATUS_1_REG     eFuse ECC Error Detected and Corrected Status 1 Register.
 *
 ******************************************************************************/
/******************************************************************************
 *
 * Address constant for each register.
 *
 ******************************************************************************/
#define EFUSE_MODE_ADDR              (__EFUSE_BASE_ADDR + 0x0000)
#define EFUSE_ADR_ADDR               (__EFUSE_BASE_ADDR + 0x0004)
#define EFUSE_DIR_CMD_ADDR           (__EFUSE_BASE_ADDR + 0x0008)
#define EFUSE_RD_DATA_ADDR           (__EFUSE_BASE_ADDR + 0x000C)
#define EFUSE_ECCSRAM_ADR_ADDR       (__EFUSE_BASE_ADDR + 0x0010)
#define EFUSE_ECCSRAM_RDPORT_ADDR    (__EFUSE_BASE_ADDR + 0x0014)
#define EFUSE_ECC_STATUS_0_ADDR      (__EFUSE_BASE_ADDR + 0x0018)
#define EFUSE_ECC_STATUS_1_ADDR      (__EFUSE_BASE_ADDR + 0x001C)

/******************************************************************************
 *
 * Register pointer.
 *
 ******************************************************************************/
#define EFUSE_MODE_REG               (REG32_PTR(EFUSE_MODE_ADDR))             /* 0x00 */
#define EFUSE_ADR_REG                (REG32_PTR(EFUSE_ADR_ADDR))              /* 0x04 */
#define EFUSE_DIR_CMD_REG            (REG32_PTR(EFUSE_DIR_CMD_ADDR))          /* 0x08 */
#define EFUSE_RD_DATA_REG            (REG32_PTR(EFUSE_RD_DATA_ADDR))          /* 0x0C */
#define EFUSE_ECCSRAM_ADR_REG        (REG32_PTR(EFUSE_ECCSRAM_ADR_ADDR))      /* 0x10 */
#define EFUSE_ECCSRAM_RDPORT_REG     (REG32_PTR(EFUSE_ECCSRAM_RDPORT_ADDR))   /* 0x14 */
#define EFUSE_ECC_STATUS_0_REG       (REG32_PTR(EFUSE_ECC_STATUS_0_ADDR))     /* 0x18 */
#define EFUSE_ECC_STATUS_1_REG       (REG32_PTR(EFUSE_ECC_STATUS_1_ADDR))     /* 0x1C */

/******************************************************************************
 *
 * Register value.
 *
 ******************************************************************************/
#define EFUSE_MODE_VAL               (REG32_VAL(EFUSE_MODE_ADDR))              /* 0x00 */
#define EFUSE_ADR_VAL                (REG32_VAL(EFUSE_ADR_ADDR))               /* 0x04 */
#define EFUSE_DIR_CMD_VAL            (REG32_VAL(EFUSE_DIR_CMD_ADDR))           /* 0x08 */
#define EFUSE_RD_DATA_VAL            (REG32_VAL(EFUSE_RD_DATA_ADDR))           /* 0x0C */
#define EFUSE_ECCSRAM_ADR_VAL        (REG32_VAL(EFUSE_ECCSRAM_ADR_ADDR))       /* 0x10 */
#define EFUSE_ECCSRAM_RDPORT_VAL     (REG32_VAL(EFUSE_ECCSRAM_RDPORT_ADDR))    /* 0x14 */
#define EFUSE_ECC_STATUS_0_VAL       (REG32_VAL(EFUSE_ECC_STATUS_0_ADDR))      /* 0x18 */
#define EFUSE_ECC_STATUS_1_VAL       (REG32_VAL(EFUSE_ECC_STATUS_1_ADDR))      /* 0x1C */

/******************************************************************************
 *
 * EFUSE_MODE_REG     eFuse Mode Register bits definitions.
 *
 ******************************************************************************/
#define EFUSE_ECC_READ               BIT31          /* ECC Read */
#define EFUSE_ECC_EN                 BIT30          /* ECC Enable */
#define EFUSE_MODE_IDLE              0x00           /* eFuse Controller IDLE */
#define EFUSE_MODE_DA                0x01           /* Direct Access Mode */
#define EFUSE_MODE_ER                0x02           /* Embedded Read */
#define EFUSE_MODE_EW                0x03           /* Embedded Write */

/******************************************************************************
 *
 * EFUSE_DIR_CMD_REG     eFuse Direct Access Command Register bits definitions.
 *
 ******************************************************************************/
#define EFUSE_DIR_VDDQ               0x01
#define EFUSE_DIR_PGENB              0x02
#define EFUSE_DIR_CSB                0x04
#define EFUSE_DIR_LOAD               0x08
#define EFUSE_DIR_STROBE             0x10

/******************************************************************************
 *
 * Miscellaneous definitions
 *
 ******************************************************************************/
#define ENV_EFUSE_GPIO "wmt.efuse.gpio"

#define EFUSE_AVAILABLE_START_BYTE   8
#define EFUSE_AVAILABLE_BYTES_NUM        ((128 - EFUSE_AVAILABLE_START_BYTE) / 4 * 3) // (120 / 4) * 3

/*
* Note: Max bytes number should be 3 aligned
*/
#define EFUSE_BOUND_MAX_BYTES_NUM       6
#define EFUSE_CPUID_MAX_BYTES_NUM       8
#define EFUSE_UUID_MAX_BYTES_NUM        24

/******************************************************************************
 *
 * Type definitions
 *
 ******************************************************************************/
typedef struct _EFUSE_REG_ {
	volatile unsigned int eFuse_Mode;	    /* [Rx00 - 03] eFuse Mode Register */
	volatile unsigned int eFuse_Adr;	    /* [Rx04 - 07] eFuse Address Register */
	volatile unsigned int eFuse_DIR_Cmd;	    /* [Rx08 - 0B] eFuse Direct Access Command Register */
	volatile unsigned int eFuse_RD_Data;	    /* [Rx0C - 0F] eFuse Read Out Data Register */
	volatile unsigned int eFuse_ECCSRAM_Adr;    /* [Rx10 - 13] eFuse Read Address of the 30 x 26 SRAM Register */
	volatile unsigned int eFuse_ECCSRAM_Rdport; /* [Rx14 - 17] eFuse ECCSRAM Read Out Data register */
	volatile unsigned int eFuse_ECC_Sts_0;	    /* [Rx18 - 1B] eFuse ECC Status 0 Register */
	volatile unsigned int eFuse_ECC_Sts_1;	    /* [Rx1C - 1F] eFuse ECC Status 1 Register */
} EFUSE_REG, *PEFUSE_REG;

typedef struct _VDD25_GPIO_ {
	int gpiono;
	int active;
} VDD25_GPIO;

typedef enum _OTP_TYPE
{
	OTP_BOUND,   // 6 bytes, need 8 bytes
	OTP_CPUID,  //  8 bytes, need 12 bytes
	OTP_UUID,   //  24 bytes, need 32 bytes
} OTP_TYPE;

/******************************************************************************
 *
 * External functions
 *
 ******************************************************************************/
extern int wmt_efuse_init(int force);
extern int efuse_read_bytes(int start, unsigned char *p_read_bytes, int count);
extern int efuse_write_bytes(int start, unsigned char *p_write_bytes, int count);
extern int efuse_hamming_read_int(int start, unsigned int *p_read_int, unsigned char *p_ecc_error, int count);
extern int efuse_hamming_write_int(int start, unsigned int *p_write_int, int count);
extern void print_write_read_bytes(int have_title, unsigned char *p_wbuf, unsigned char *p_rbuf, int count);
extern void print_write_read_integers(int have_title, unsigned int *p_wbuf, unsigned int *p_rbuf, int count);
extern void efuse_register_dump(void);
extern int bytes_3aligned(int bytes_num);
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
extern int efuse_read_otp(OTP_TYPE type, unsigned char *rbuf, int rlen);

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
extern int efuse_write_otp(OTP_TYPE type, unsigned char *wbuf, int wlen);

#endif

