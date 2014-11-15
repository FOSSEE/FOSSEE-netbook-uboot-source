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


/*
 * RSA encode/decode Utilities
 */

#include <common.h>
#include <command.h>
#include <linux/aes.h>

int do_aescbc(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[]){
    int rcode = 1;
    int i = 0;
    int mode = AES_ENCRYPT;
    aes_context ctx;
    ulong image_addr, image_size;
    ulong key_addr, key_size;
    ulong out_addr;
    char *output;
    unsigned char iv[16]={0};
    
	if(argc >= 6){
        if(!strncmp(argv[1], "enc", 3)){
           mode = AES_ENCRYPT;
        }else if(!strncmp(argv[1], "dec", 3)){
           mode = AES_DECRYPT;
        }else{
           printf("invalid command\n");
           return 1;
        }
        
        image_addr = simple_strtoul(argv[2], NULL, 16);
        image_size = simple_strtoul(argv[3], NULL, 16);
        key_addr = simple_strtoul(argv[4], NULL, 16);
        key_size = simple_strtoul(argv[5], NULL, 16);        
        if(argc > 6) out_addr = simple_strtoul(argv[6], NULL, 16);        
        
        printf("image(%x, %x), key(%x, %x)\n", image_addr, image_size, key_addr, key_size);
        
        if(key_size != 16 && key_size != 24 && key_size != 32){
            printf("keysize should be 16 bytes(128bit) or 24 bytes(192bit) or 32 bytes(256bit)\n");
            return 1;
        }
        
        if(image_size % 16){
            printf("image size should be 16 bytes aligned\n");
            return 1;
        }
        
        if(mode == AES_ENCRYPT){
            if(aes_setkey_enc( &ctx, key_addr, key_size * 8) != 0){
                 printf("invalid key. is size correct?\n");
                 return 1;
            }
        }else{
            if(aes_setkey_dec( &ctx, key_addr, key_size * 8) != 0){
                 printf("invalid key. is size correct?\n");
                 return 1;                 
            }
        }
        output = malloc(image_size);
        memset(output, 0, image_size);
        printf("%s...\n", mode==AES_ENCRYPT?"encrypting":"decrypting");
        printf("ctx->nr is %x\n", ctx.nr);
        if(aes_crypt_cbc( &ctx, mode, image_size, iv, image_addr, output) == 0){
            printf("Output:\n");
            for(i=0; i < image_size; i++){
                printf("%02x ",  output[i]);
            }        
            if(argc > 6){
                printf("\ncopy to %x\n", out_addr);
                memcpy(out_addr, output, image_size);
            }
            free(output);
            return 0;            
        }else{
            printf("error\n");
            free(output);
            return 1;
        }
    }
	return 1;
}

U_BOOT_CMD(
	aescbc, 7, 1, do_aescbc, 
	"aescbc  - aes cbc encypher/decypher\n",
	"<enc|dec>              encode / decode\n"
	"<image_mem_addr>       image address in memory\n"
	"<image_size>           image size\n"
	"<key_mem_addr>         key address in memory\n"
	"<key_size>             key size\n"   
    "<output>               output address in memory\n"    
);



