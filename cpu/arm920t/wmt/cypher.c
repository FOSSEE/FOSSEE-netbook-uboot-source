/*++ 
Copyright (c) 2010 WonderMedia Technologies, Inc.

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software 
Foundation, either version 2 of the License, or (at your option) any later 
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details. You
should have received a copy of the GNU General Public License along with this
program. If not, see http://www.gnu.org/licenses/>.

WonderMedia Technologies, Inc.
10F, 529, Chung-Cheng Road, Hsin-Tien, Taipei 231, R.O.C.
--*/
/*--
Revision History:
-------------------------------------------------------------------------------

--*/

#include <malloc.h>

#include "cypher.h"

#ifdef DPRINTK
	#undef DPRINTK
#endif

//#define DEBUG_CYPHER
#ifdef DEBUG_CYPHER

	#define DPRINTK printf

    #define DPRINT_FUNC_IN() DPRINTK("--- Enter %s() ---\n", __FUNCTION__)
    #define DPRINT_FUNC_OUT() DPRINTK("=== Exit %s() ===\n", __FUNCTION__)
#else
	#define DPRINTK(a,...)
    #define DPRINT_FUNC_IN()
    #define DPRINT_FUNC_OUT()
#endif



/*****************************************************************/
u32 OUTPUT_buf[BUFFER_MAX];
u32 in_table_buf[40960], out_table_buf[10];
u32 KEY_buf[8], IV_buf[4], INC_buf;
u8 cipher_int_flag = 0;

/*****************************************************************/
#define AHB_NOP(str) // DPRINTK("wait %d nop...\n", str)
#ifndef AHB_NOP
void AHB_NOP(int count)
{
	while(count) {
		asm("nop");
		count--;
	}
}
#endif

/****************************************************************/
void cypher_isr(void)
{
	REG_SET32(CYPHER_DMA_INT_STATUS_REG, 0x00000001);   //clear interrupt bit
	cipher_int_flag = 1;
	DPRINTK("cipher interrupt mode done! \n");	
}

/*****************************************************************/
void cypher_enable(
	IN	cypher_algo_t algo_mode)
{
	DPRINT_FUNC_IN();

#ifdef CYPHER_INT_ENABLE
	// enable cipher DMA Interrupt
	REG_SET32(CYPHER_DMA_INT_ENABLE_REG, CYPHER_DMA_INT_ENABLE);
#endif

	// enable cipher Clock
	switch(algo_mode) {
		case CYPHER_ALGO_SHA256:
			REG_SET32(CYPHER_CLK_ENABLE_REG, CYPHER_SHA256_CLK_ENABLE);
			break;
		default:
			break;
	}

	// doing software reset to makesure ervrything is going well
	REG_SET32(CYPHER_SW_RESET_REG, 0x01);
	REG_SET32(CYPHER_SW_RESET_REG, 0x00);
	
	DPRINT_FUNC_OUT();
}

/*****************************************************************/
void cypher_disable(void)
{
	DPRINT_FUNC_IN();

#ifdef CYPHER_INT_ENABLE
	// disable cipher DMA Interrupt
	REG_SET32(CYPHER_DMA_INT_ENABLE_REG, CYPHER_DMA_INT_DISABLE);
#endif

	// disable cipher Clock
	REG_SET32(CYPHER_CLK_ENABLE_REG, CYPHER_CLK_DISABLE);
	
	DPRINT_FUNC_OUT();
}

/*****************************************************************/
void cypher_mode_set(
	IN	cypher_base_cfg_t *cypher_base_cfg)
{
	u32 sha256_variable = 0x00;

	DPRINT_FUNC_IN();
	
	if(cypher_base_cfg->algo_mode == CYPHER_ALGO_SHA256) {
		REG_SET32(CYPHER_MODE_REG, CYPHER_SHA256_MODE);					//Set cipher mode to SHA256

		/* set data length to bit25-16, write 1 to bit1 to swap buffer, write 1 to bit0 to enable SHA256 */
		sha256_variable = cypher_base_cfg->sha256_data_length;
		REG_SET32(CYPHER_SHA256_DATA_LEN_REG, sha256_variable);	

		REG_SET32(CYPHER_SHA256_START_REG, (0x01 | 0x02 << 2)); 
	}
	
		
	DPRINT_FUNC_OUT();
}



/*****************************************************************/
void cypher_prd_table(
	IN int prd_tbl_addr, 
	IN int phy_base_addr, 
	IN int byte_count)
{
	int edt_byte_cnt, edt_bit;
	int j, prd_bytecount;

	DPRINT_FUNC_IN();

	for (j = 0, edt_bit = 0; byte_count > 0; j++) {
		if (byte_count > PRD_BYTE_COUNT_MAX) {
			prd_bytecount = PRD_BYTE_COUNT_MAX;
			byte_count -= PRD_BYTE_COUNT_MAX ;	
		}
		else {
			prd_bytecount = byte_count;
			edt_bit = 1;
			byte_count = 0;
		}

		edt_byte_cnt = (edt_bit << 31) + prd_bytecount;

		REG_SET32((prd_tbl_addr + (j * PRD_TABLE_BOUNDARY)), (phy_base_addr + (j * PRD_BYTE_COUNT_MAX)));	// Set memory read base address
		REG_SET32((prd_tbl_addr + (j * PRD_TABLE_BOUNDARY) + 4), edt_byte_cnt);								// Set EDT, Byte count
	}

#if 0
DPRINTK("addr = %x \n", REG_GET32(prd_tbl_addr));
DPRINTK("addr+4 = %x \n", REG_GET32(prd_tbl_addr+4));
#endif

	DPRINT_FUNC_OUT();
}

/*****************************************************************/
void cypher_dma_start(void)
{
#ifndef CYPHER_INT_ENABLE
	int r_value;
#endif
 
	DPRINT_FUNC_IN();
	
	cipher_int_flag = 0;
	REG_SET32(CYPHER_DMA_START_REG, 0x01);           /*Set the DMA start*/

	while(1) {
#ifndef CYPHER_INT_ENABLE
		r_value = REG_GET32(CYPHER_DMA_INT_STATUS_REG);

		if( (r_value & 0x01) == 1 ) {
			REG_SET32(CYPHER_DMA_INT_STATUS_REG, 0x00000001);   //clear interrupt bit
			cipher_int_flag = 1;
			DPRINTK("cipher polling mode done! \n");	
		}
#endif

		if (cipher_int_flag)
			break;
	}
	
	DPRINT_FUNC_OUT();
}

/****************************************************************/
unsigned long cypher_transform(IN unsigned long temp) 
{
	int i;
	u32 buf;
	buf = 0;
	
	for (i = 0; i < 4; i++) {
		buf += ((temp << 8 * (3 - i)) & 0xff000000) >> (8 * i);
	}
	
	return (buf);
}


/*****************************************************************/
void Get_Output_Buf( OUT u32 dest_addr, u32 dest_size, u32 dec_enc )
{
	int patterns;
	int total_u32;

	DPRINT_FUNC_IN();
	
	if (dest_size % DMA_BOUNDARY)
		patterns = (dest_size / DMA_BOUNDARY) + 1;
	else
		patterns = (dest_size / DMA_BOUNDARY);

	memset((void*)dest_addr, CYPHER_ZERO, dest_size * sizeof(u8));

	total_u32 = patterns * (DMA_BOUNDARY / sizeof(u32));
	memcpy((void*)dest_addr, OUTPUT_buf, total_u32 * sizeof(u32));	

#if 0
#ifdef DEBUG
{
	int idx;
	
	if (dec_enc == CYPHER_ENCRYPT) {
		DPRINTK("ciphertext: ");
	}
	else {
		DPRINTK("plaintext:  ");
	}

	for (idx = 0; idx < total_u32; idx++) {
		DPRINTK("%08x ", (uint)cypher_transform(OUTPUT_buf[idx]));
	}
	DPRINTK(" [%d]\n", total_u32);
}
#endif
#endif

	DPRINT_FUNC_OUT();
}

/*****************************************************************/
void Count_Patterns(
	IN  int  patterns,
	OUT int *in_bytecount,
	OUT int *out_bytecount,
	OUT int *total_patterns )
{
	DPRINT_FUNC_IN();
	
#if 1
	*in_bytecount = patterns * DMA_BOUNDARY;
	*out_bytecount = patterns * DMA_BOUNDARY;
	*total_patterns = patterns * (DMA_BOUNDARY / sizeof(u32));
#else
	if(algo_mode == CYPHER_ALGO_DES || algo_mode == CYPHER_ALGO_TDES)
		patterns = cypher_base->patterns * 2;

	if(algo_mode == CYPHER_ALGO_AES || algo_mode == CYPHER_ALGO_RC4)
	{
		in_bytecount = patterns*128/8;  //read data byte count
		out_bytecount = patterns*128/8; //write data byte count
	}
	else if(algo_mode == CYPHER_ALGO_DES || algo_mode == CYPHER_ALGO_TDES)
	{
		in_bytecount = patterns*64/8;  //read data byte count
		out_bytecount = patterns*64/8; //write data byte count
	}

	//Compute total words of data
	if(algo_mode == CYPHER_ALGO_AES || algo_mode == CYPHER_ALGO_RC4)
		total_patterns = 4 * patterns;
	else if(algo_mode == CYPHER_ALGO_DES || algo_mode == CYPHER_ALGO_TDES)
		total_patterns = 2 * patterns;
#endif

	DPRINTK("in_bytecount: %d \n", *in_bytecount);
	DPRINTK("out_bytecount: %d \n", *out_bytecount);
	DPRINTK("total_patterns: %d \n", *total_patterns);
	
	DPRINT_FUNC_OUT();
}

/*****************************************************************/
int Cypher_Action( IN OUT cypher_base_cfg_t *cypher_base )
{
	int i;
	int in_bytecount;
	int out_bytecount;
	int total_patterns;
	int patterns;

	DPRINT_FUNC_IN();

	//memset(in_table_buf, CYPHER_ZERO, 1000 * sizeof(u32));
	//memset(out_table_buf, CYPHER_ZERO, 1000 * sizeof(u32));
	
	if (cypher_base->text_length % DMA_BOUNDARY)
		patterns = (cypher_base->text_length / DMA_BOUNDARY) + 1;
	else
		patterns = (cypher_base->text_length / DMA_BOUNDARY);
	
	if (cypher_base->algo_mode > CYPHER_ALGO_BASE_END)
		return 1;
	

	//Compute total words of data
	Count_Patterns(patterns, &in_bytecount, &out_bytecount, &total_patterns);


	/* actuality output bytes of SHA256 encrypt always = 32 Bytes */
	if (cypher_base->algo_mode == CYPHER_ALGO_SHA256) {
		out_bytecount = 32;
	}

	DPRINTK("*******************************************\n");


	cypher_enable(cypher_base->algo_mode);
	cypher_mode_set(cypher_base);


	/****************************
	Write PRD CIPHER_IN Control register
	****************************/
	cypher_prd_table((int)in_table_buf, cypher_base->input_addr, in_bytecount);
	//cypher_prd_table((int)in_table_buf, (int)INPUT_buf, in_bytecount);
	REG_SET32(CYPHER_DMA_SOUR_ADDR_REG, (int)in_table_buf);		// Set PRD read point
	
	/****************************
	Write PRD CIPHER_OUT Control register
	****************************/
	cypher_prd_table((int)out_table_buf, (int)OUTPUT_buf, out_bytecount);
	REG_SET32(CYPHER_DMA_DEST_ADDR_REG, (int)out_table_buf);	// Set PRD write point

	DPRINTK("in_table_buf=0x%08x \n", (uint)in_table_buf);
	//DPRINTK("INPUT_buf=0x%08x \n", (uint)INPUT_buf);
	DPRINTK("out_table_buf=0x%08x \n", (uint)out_table_buf);
	DPRINTK("OUTPUT_buf=0x%08x \n", (uint)OUTPUT_buf);

	/*if (total_patterns <= 32) {
		DPRINTK("INPUT_buf: \n");
		for(i = 0; i < total_patterns; i++)
			DPRINTK("%08x ", (uint)INPUT_buf[i]);
		DPRINTK(" [%d]\n", total_patterns);
	}*/


	//START CIPHER DMA
	cypher_dma_start();


	/*Now, text_length should be output bytes of SHA256 encrypt always = 32 Bytes */
	if (cypher_base->algo_mode == CYPHER_ALGO_SHA256) {
		cypher_base->text_length = 32;
	}

	if (total_patterns <= 32) {
		DPRINTK("OUTPUT_buf: \n");
		for(i = 0; i < total_patterns; i++)
			DPRINTK("%08x ", (uint)OUTPUT_buf[i]);
		DPRINTK(" [%d]\n", total_patterns);
	}

	DPRINT_FUNC_OUT();

	cypher_disable();
	return 0;
}


/*****************************************************************/
void Clear_All_Buf(void)
{
	//memset(INPUT_buf, CYPHER_ZERO, BUFFER_MAX * sizeof(u32));
	//memset(OUTPUT_buf, CYPHER_ZERO, BUFFER_MAX * sizeof(u32));
	memset(KEY_buf, CYPHER_ZERO, 6 * sizeof(u32));
	memset(IV_buf, CYPHER_ZERO, 4 * sizeof(u32));
	INC_buf = CYPHER_ZERO;
}

/*****************************************************************/
void Show_Register(int start, int end)
{
	int idx;
	u32 value;
	
	DPRINTK("Register [ 0x%x ~ 0x%x ]\n", start, end);
	for (idx = start; idx <= end; idx += 4) {
		value = REG_GET32(CYPHER_REG_BASE + idx);
		DPRINTK("[0x%02x] %08x \n", idx, (uint)value);
	}

}

