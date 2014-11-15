#include <common.h>
#include <command.h>
#include <environment.h>
#include <serial.h>
#include <linux/stddef.h>
#include <asm/byteorder.h>
#include <linux/aes.h>
#include <linux/rsa/base64.h>
#include <search.h>
#include <errno.h>
#include "../common/wmt_efuse/wmt_efuse.h"

DECLARE_GLOBAL_DATA_PTR;

#define CIPHER_IV \
{ \
  'W', 'O', 'N', 'D', \
  'E', 'R', 'M', 'E', \
  'D', 'I', 'A',   3, \
    7,   6, 'A', 'F'  \
}


#define THE_KEY \
{ \
  'W', 'M', 'T', 'K', \
  'E', 'Y',  1,   7,  \
    4,   4,  0,   3, \
    7,   6, 'A', 'F'  \
}

#define MAX_OTP_VAL_SIZE 260
#define MAX_OTP_VAL_BASE64_SIZE 512
#define MAX_NAME_SIZE (256)
#define MAX_VALUE_SIZE (4*1024)

extern env_t *env_ptr;
extern env_t *flash_addr;

extern env_t *env_ptr2 ;
extern env_t *flash_addr2 ;
extern struct hsearch_data env_htab ;

int encdec(int mode, unsigned char *key, size_t keybytes, unsigned char * data, size_t databytes, unsigned char *output){
    aes_context ctx;
	unsigned char iv[16]=CIPHER_IV;
	
	if(mode != AES_ENCRYPT && mode != AES_DECRYPT)
		return -1;

    if(mode == AES_ENCRYPT){
        if(aes_setkey_enc( &ctx, key, keybytes * 8) != 0){
             printf("invalid key size\n");
             return -1;
        }
    }else{
        if(aes_setkey_dec( &ctx, key, keybytes * 8) != 0){
             printf("invalid key size\n");
             return -1;                 
        }
    }
	
    if(aes_crypt_cbc(&ctx, mode, databytes, iv, data, output) != 0){
		return -1;
	}

    return 0;
}

//aes128
int genkey(unsigned char key[16]){
	//unsigned char *chipid = (unsigned char*)otp_getenv("otp.chipid");
    unsigned char chipid[8]={0};
	unsigned char chipidstr[17]={0};
	unsigned char id[16]={0};
	unsigned char thekey[16]=THE_KEY;
	int i = 0;
	
	efuse_read_otp(OTP_CPUID, chipid, sizeof(chipid));
	for(i = 0; i < sizeof(chipid); i++){
	    sprintf(chipidstr+i*2, "%02X", chipid[i]);
	}
    printf("chipidstr is %s\n", chipidstr);

    //if(chipid == NULL) return -1;
	
	int len = strlen((char*)chipidstr);
	if(len > sizeof(id)) len = sizeof(id);
	strncpy((char*)id, (char*)chipidstr, len);
	return encdec(AES_ENCRYPT, thekey, sizeof(thekey), id, sizeof(id), key);
}


int otp_sec_encode(uchar *in, int ilen, uchar *out, size_t *olen)
{
    //aes128
    unsigned char key[16]={0};
	unsigned char val[MAX_OTP_VAL_SIZE]={0};
	unsigned char cypval[MAX_OTP_VAL_SIZE]={0};
	unsigned char *p,*s ;
	size_t  vallen=0;
    
	if(genkey(key) < 0){
		printf("fail to gen key\n");
		return 1;
	}

    if(ilen > MAX_OTP_VAL_SIZE-1){
        printf("env value is too long\n");
	    return 1;
    }
    
    p = in;
    s = val;
    while((*s++ = *p++) != '\0')
        ;
	
    vallen = ilen;
	//16 bytes aligned
	vallen = ((vallen + 15) / 16) * 16;

	printf("val is %s, vallen is %d\n",in, vallen);
	if(encdec(AES_ENCRYPT, key, sizeof(key), val, vallen, cypval) < 0){
		printf("encode value fail\n");
		return 1;
	}

	if(base64_encode(out, olen, cypval, vallen) != 0){
        printf("base64 encode fail\n");
		return 1;
	}
    
    return 0;
   
}

int otp_sec_decode(const char *val, uchar * theval)
{
	if( val != NULL && theval  != NULL){
		unsigned char key[16]={0};
		unsigned char cypval[MAX_OTP_VAL_SIZE]={0};
        size_t vallen = MAX_OTP_VAL_SIZE; 
        
		if(base64_decode(cypval, &vallen, val, strlen(val)) != 0){
			printf("base64 decode fail\n");
			return -1;			
		}		
        
		if(genkey(key) < 0){
			printf("fail to gen key\n");
			return -1;
		}

		if(vallen % 16){
            printf("invalid value len\n");
			return -1;
		}

		if(encdec(AES_DECRYPT, key, sizeof(key), cypval, vallen, theval) < 0){
			printf("encode value fail\n");
			return -1;
		}		
		return 0;
	}
	return -1;
}

static int write_env(int index, uchar* buf, int size)
{
	int rc;
	ulong end_addr,flash_sect_addr;
    ulong start,end;
	int rcode = 0;

    if(size > CFG_ENV_SIZE)
        size = CFG_ENV_SIZE;
    
	flash_sect_addr = (ulong)flash_addr;
	end_addr =(flash_sect_addr+0x20000-1);

    //protect 2 sectors
	if (flash_sect_protect (0, flash_sect_addr, end_addr))
		return 1;
    
    start = (ulong)flash_addr+(index*CFG_ENV_SIZE);
    end = (ulong)flash_addr+(index*CFG_ENV_SIZE)+CFG_ENV_SIZE-1;
    
	printf ("Erasing env%d...",index+1);
	if (flash_sect_erase (start, end)){
		rcode = 1;
		goto Done;
    }

	printf ("Writing to env%d... ",index+1);

	if (rc = flash_write(buf,start, size))
	{
		flash_perror (rc);
		rcode = 1;
	}else{
        printf("done\n");
	}

Done:
	/* try to re-protect */
	(void) flash_sect_protect (1, flash_sect_addr, end_addr);
	return rcode;
}

static int is_persist(char *name)
{
    int i, len;
    char *persistlist[]={"otp.", "ethaddr", "wmt.ethaddr.persist", "androidboot.serialno", 
                    "btaddr", "wmt.btaddr.persist","pcba.serialno","serialnum","persist.", NULL};

    for(i=0; persistlist[i] != NULL; i++){
        len = strlen(persistlist[i]);
        if(!strncmp(name, persistlist[i], len))
            return 0;       
    }

    return -1;    
}

static int sync_persist_env(struct hsearch_data *htab, uchar *env2)
{   
    int len,i;
    int updated=0;
    uchar name[MAX_NAME_SIZE]={0};
    uchar *val=NULL,*valbuf=NULL;   
    uchar *s,*res;
	env_t env_new;
    ENTRY e, *ep;    

    if(!htab)
        return 1;

    valbuf = malloc(MAX_VALUE_SIZE);
    
    for(s=env2; s < (env2+ENV_SIZE) && *s!='\0'; ){
        
        if(is_persist(s) == 0){
            i=0;
            while(*s != '=' && *s != '\0' && i < (sizeof(name)-1)) 
                name[i++] = *s++;            

            name[i] = '\0';
            
            i=0;
            s++;//skip '='            
            val = valbuf;
            while(*s != '\0' ) 
                val[i++] = *s++;
            
            val[i] = '\0';
            s++;
            //printf("env2:%s=%s\n",name,val);
            
    		e.key = (char*)name;
    		e.data = (char*)NULL;
    		hsearch_r(e, FIND, &ep, htab);
            //if(ep) printf("env1:%s-%s\n",ep->key,ep->data);
            /* otp.xx exist in env2, but not exist in env1,copy it to env1 */
            if(!ep){
                e.key = (char*)name;
    		    e.data = (char*)val;
                printf("insert %s=%s to env1\n",e.key,e.data);
    		    hsearch_r(e, ENTER, &ep, htab);
                if (!ep) {
		            printf("## Error inserting \"%s\" variable\n",name);
                    free(valbuf);
		            return 1;
	            }
                updated ++;
            }
        }
        else{        
            len = strlen(s)+1;
            s += len;
        }
    }

    //printf("sync %d otps to env1\n",updated);
    res = (char *)&(env_new.data);
	len = hexport_r(htab, '\0', &res, ENV_SIZE, 0, NULL);
	if (len < 0) {
		printf("Cannot export environment\n");
        free(valbuf);
		return 1;
	}
	env_new.crc = crc32(0, env_new.data, ENV_SIZE);  
    write_env(0,(uchar*)&env_new, CFG_ENV_SIZE);
    write_env(1,(uchar*)&env_new, CFG_ENV_SIZE);
    free(valbuf);
    
    return 0;
}


/* buf shloud to be clean*/
static int cpyenv(uchar *buf, uchar *addr, int len)
{
    int blk=0;
    uchar *src,*dest;

    src = addr;
    dest = buf;

    memcpy(dest, src, 0x400);//1K
    while( (dest[0x3fe]|dest[0x3ff]) != '\0' && blk++ < 63){
        src += 0x400;
        dest += 0x400;
        memcpy(dest, src, 0x400);        
    }
    
    return 0;
}

int save_env( int idx)
{
    int len;
    uchar *res;
	env_t env_new;

   if (idx ==1||idx ==2){    
        res = (char *)&(env_new.data);
    	len = hexport_r(&env_htab, '\0', &res, ENV_SIZE, 0, NULL);
    	if (len < 0) {
    		printf("Cannot export environment\n");
    		return 1;
    	}
    	env_new.crc = crc32(0, env_new.data, ENV_SIZE);  

        write_env(idx-1, (uchar*)&env_new, CFG_ENV_SIZE);
    }else{
        uchar env2_buf[CFG_ENV_SIZE]={0};
        cpyenv(env2_buf, (uchar*)env_ptr2, CFG_ENV_SIZE);
        sync_persist_env(&env_htab,&env2_buf[4]);
    }

    return 0;
}


int esync(void)
{
    int ret;
    u32 crc1,crc2;
    uchar env1_buf[CFG_ENV_SIZE]={0};
    uchar env2_buf[CFG_ENV_SIZE]={0};

    /* sync SF env partitions */    

    cpyenv(env1_buf, (uchar*)env_ptr, CFG_ENV_SIZE);
    cpyenv(env2_buf, (uchar*)env_ptr2, CFG_ENV_SIZE);

    crc1 = crc32(0, &env1_buf[4], ENV_SIZE);
    crc2 = crc32(0, &env2_buf[4], ENV_SIZE);

    //printf("crc1:%08lx,%08lx; crc2:%08lx,%08lx\n", env_ptr->crc,crc1,env_ptr2->crc,crc2);
    /* env1 and env2 ok,and env1==env2 */
	if (crc1 == env_ptr->crc && crc2 == env_ptr2->crc && crc1 == crc2) {
        printf("env1==env2\n");        
	}
    /* env1 is invalid,env2 is ok */
    else if(crc1 != env_ptr->crc && crc2 == env_ptr2->crc){
        printf("env2->env1\n");
        //memcpy(env_buf,env_ptr2,CFG_ENV_SIZE);
        write_env(0, env2_buf, CFG_ENV_SIZE);
    }
    /* env2 is invalid, env1 is ok */
    else if(crc2 != env_ptr2->crc && crc1 == env_ptr->crc){
        printf("env1->env2\n");
        //memcpy(env_buf,env_ptr,CFG_ENV_SIZE);
        write_env(1, env1_buf, CFG_ENV_SIZE);
    }
    /* env2 env1 ok,but env1!=env2 */
    else if(crc2 == env_ptr2->crc && crc1 == env_ptr->crc && crc1 != crc2){
        printf("env1<-> env2\n");
        struct hsearch_data env_htab1 ={0};
        if (himport_r(&env_htab1, (char *)&env1_buf[4], ENV_SIZE, '\0', 0)==0) {
            printf("env1 hash table error!\n");
	        return 1;
        }
        sync_persist_env(&env_htab1, &env2_buf[4]);
        hdestroy_r(&env_htab1);
    }
    /* env2 is invalid, env1 is invalid */
    else{
        printf("crc1:%08lx,%08lx; crc2:%08lx,%08lx\n", env_ptr->crc,crc1,env_ptr2->crc,crc2);
        printf("both env invalid\n");

        return 1;
    }
    
    return 0;    

}

int do_esync(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
    int i=0;

    if(argc < 2){
        esync();
    }else{
        i = simple_strtoul(argv[1], NULL, 10);
        if(i==1){
            /* save cache to env1 */
            save_env(1);
        }else if(i==2){
            /* save cache to env2 */
            save_env(2);
        }else{
            ;
        }
    }
    return 0;
}

U_BOOT_CMD(
    esync,    3,  1,  do_esync,
	"esync     - sync uboot env\n",
	"esync  \n"
);
