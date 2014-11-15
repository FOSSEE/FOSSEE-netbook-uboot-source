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

#include "common.h"
#include <malloc.h>

#include <stdarg.h>
#include "../../../board/wmt/include/wmt_clk.h"
#include "cypher.h"



//#define DEBUG_CYPHER
#ifdef DEBUG_CYPHER

	#define DPRINTK printf

    #define DPRINT_FUNC_IN() DPRINTK("--- Enter %s() ---\n", __FUNCTION__)
    #define DPRINT_FUNC_OUT() DPRINTK("=== Exit %s() ===\n", __FUNCTION__)
#else
    #define DPRINT_FUNC_IN()
    #define DPRINT_FUNC_OUT()
#endif

/****************************************************************/
extern int auto_pll_divisor(enum dev_id dev, enum clk_cmd cmd, int unit, int freq);



/* NOTICE: the input data length of SHA256 must be multiple of 4Bytes */
u8 Input_Plainttext[] = "abc";

u8 Output_Plainttext[3] = {0};




u8 Plaintext[DATA_MAX + 1];		// for output of decrypt

u8 Ciphertext[DATA_MAX + 1];		// for output of encrypt

int text_len;


 
/****************************************************************/
int Cypher_Action( IN OUT cypher_base_cfg_t *cypher_base );
void Clear_All_Buf(void);
void Set_Input_Buf( IN unsigned int src_addr, unsigned int src_size, unsigned int dec_enc );
void Get_Output_Buf( OUT unsigned int dest_addr, unsigned int dest_size, unsigned int dec_enc );
void Show_Register(int start, int end);
void cypher_isr(void);





/****************************************************************/
int cipher_enc_dec( IN OUT cypher_base_cfg_t *cipher_obj )
{
	DPRINT_FUNC_IN();

	Clear_All_Buf();
	Cypher_Action(cipher_obj);
	Get_Output_Buf(cipher_obj->output_addr, cipher_obj->text_length, cipher_obj->dec_enc);

	DPRINT_FUNC_OUT();

	return 0;
}



/****************************************************************/
void cipher_obj_clear( IN cypher_base_cfg_t *cipher_obj )
{
	DPRINT_FUNC_IN();

	cipher_obj->algo_mode = CYPHER_ALGO_SHA256;
	cipher_obj->op_mode = 0;
	cipher_obj->dec_enc = CYPHER_DECRYPT;
	cipher_obj->text_length = 0x0;
	cipher_obj->key_addr = CYPHER_ZERO;
	cipher_obj->IV_addr = CYPHER_ZERO;

	cipher_obj->input_addr = CYPHER_ZERO;  // address
	cipher_obj->output_addr = CYPHER_ZERO;	 // address
	cipher_obj->INC = 0x0;
	cipher_obj->sha1_data_length = 0x0;
	cipher_obj->sha256_data_length = 0x0;

	DPRINT_FUNC_OUT();
}




/****************************************************************/
int sha256_action( cypher_base_cfg_t *test_object, u32 input_buf, u32 output_buf)
{
	int ret = 0;

	test_object->algo_mode = CYPHER_ALGO_SHA256;
	test_object->op_mode = 0;
	
	// encrypt
	test_object->input_addr = (u32)input_buf;  // address
	test_object->output_addr = (u32)Ciphertext;	 		// address	

	test_object->dec_enc = CYPHER_ENCRYPT;
	cipher_enc_dec(test_object);

	memcpy((void*)output_buf, (void*)Ciphertext, 32);

	return ret;
}



/****************************************************************/
void cypher_initialization(void)
{
	auto_pll_divisor(DEV_SAE,CLK_ENABLE,0,0);
#ifdef CYPHER_INT_ENABLE
	/* Interrupt setting */
	set_irq_handlers(IRQ_SAE, cypher_isr);
	set_int_route(IRQ_SAE, 0);
	unmask_interrupt(IRQ_SAE);
#endif

}

void cipher_release(void)
{

	auto_pll_divisor(DEV_SAE,CLK_DISABLE,0,0);

} /* End of cipher_release() */


int cypher_encode(unsigned int buf_in, unsigned int in_len, unsigned int buf_out)
{
	cypher_base_cfg_t test_object;
	int ret = 1;

	cipher_obj_clear(&test_object);

	test_object.sha256_data_length = in_len;
	test_object.text_length = in_len;

	ret = sha256_action(&test_object, (unsigned int)buf_in, (unsigned int)buf_out);
	
	return ret;
}






