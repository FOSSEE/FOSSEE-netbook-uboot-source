#include <common.h>
#include <command.h>
#include <linux/ctype.h>
#include <asm/arch/common_def.h>
#include <asm/errno.h>

#include "../include/wmt_pmc.h"
#include "../include/wmt_spi.h"
#include "../include/wmt_clk.h"
#include "../include/wmt_gpio.h"

#define BIT0	0x00000001
#define BIT1	0x00000002
#define BIT2	0x00000004
#define BIT3	0x00000008
#define BIT4	0x00000010
#define BIT5	0x00000020
#define BIT6	0x00000040
#define BIT7	0x00000080
#define BIT8	0x00000100

#define VT1603_R00  0x00
#define VT1603_R01  0x01
#define VT1603_R02  0x02
#define VT1603_R03  0x03
#define VT1603_R04  0x04
#define VT1603_R05  0x05
#define VT1603_R06  0x06
#define VT1603_R07  0x07
#define VT1603_R08  0x08
#define VT1603_R09  0x09
#define VT1603_R0a  0x0a
#define VT1603_R0b  0x0b
#define VT1603_R0c  0x0c
#define VT1603_R0d  0x0d
#define VT1603_R0e  0x0e
#define VT1603_R0f  0x0f
#define VT1603_R10  0x10
#define VT1603_R11  0x11
#define VT1603_R12  0x12
#define VT1603_R13  0x13
#define VT1603_R15  0x15
#define VT1603_R19  0x19
#define VT1603_R1b  0x1b
#define VT1603_R1c  0x1c
#define VT1603_R1d  0x1d
#define VT1603_R20  0x20
#define VT1603_R21  0x21
#define VT1603_R23  0x23
#define VT1603_R24  0x24
#define VT1603_R25  0x25
#define VT1603_R28  0x28
#define VT1603_R29  0x29
#define VT1603_R2a  0x2a
#define VT1603_R2b  0x2b
#define VT1603_R2c  0x2c
#define VT1603_R2d  0x2d
#define VT1603_R40  0x40
#define VT1603_R41  0x41
#define VT1603_R42  0x42
#define VT1603_R47  0x47
#define VT1603_R51  0x51
#define VT1603_R52  0x52
#define VT1603_R53  0x53
#define VT1603_R5f  0x5f
#define VT1603_R60  0x60
#define VT1603_R61  0x61
#define VT1603_R62  0x62
#define VT1603_R63  0x63
#define VT1603_R64  0x64
#define VT1603_R65  0x65
#define VT1603_R66  0x66
#define VT1603_R67  0x67
#define VT1603_R68  0x68
#define VT1603_R69  0x69
#define VT1603_R6a  0x6a
#define VT1603_R6b  0x6b
#define VT1603_R6d  0x6d
#define VT1603_R6e  0x6e
#define VT1603_R70  0x70
#define VT1603_R71  0x71
#define VT1603_R72  0x72
#define VT1603_R73  0x73
#define VT1603_R77  0x77
#define VT1603_R79  0x79
#define VT1603_R7a  0x7a
#define VT1603_R7b  0x7b
#define VT1603_R7c  0x7c
#define VT1603_R82  0x82
#define VT1603_R87  0x87
#define VT1603_R88  0x88
#define VT1603_R8a  0x8a
#define VT1603_R8e  0x8e
#define VT1603_R90  0x90
#define VT1603_R91  0x91
#define VT1603_R92  0x92
#define VT1603_R93  0x93
#define VT1603_R95  0x95
#define VT1603_R96  0x96
#define VT1603_R97  0x97

extern int wmt_getsyspara(char *varname,char *varval, int *varlen);

static int vt1603_spi_write(u8 addr, const u8 data)
{
	u8 wbuf[3], rbuf[3];

	wbuf[0] = ((addr & 0xFF) | BIT7);
	wbuf[1] = ((addr & 0xFF) >> 7);
	wbuf[2] = data;

	spi_write_then_read_data(wbuf, rbuf, sizeof(wbuf),
				 SPI_MODE_3, 0);

	udelay(10);
	return 0;
}

static int vt1603_spi_read(u8 addr, u8 *data)
{
	u8 wbuf[5] = {0};
	u8 rbuf[5] = {0};

	memset(wbuf,0,sizeof(wbuf));
	memset(rbuf,0,sizeof(rbuf));

	wbuf[0] = ((addr & 0xFF) & (~BIT7));
	wbuf[1] = ((addr & 0xFF) >> 7);

	spi_write_then_read_data(wbuf, rbuf, sizeof(wbuf),
				 SPI_MODE_3, 0);

	if (0) {
		int i;
		for (i = 0; i < sizeof(rbuf); i++)
			printf("0x%02x ", rbuf[i]);
		printf("\n");
	}
	data[0] = rbuf[4];
	return 0;
}

static inline void i2s_pin_config(void)
{
	/* disable GPIO and Pull Down mode */
	GPIO_CTRL_GP10_I2S_BYTE_VAL &= ~0xFF;
	GPIO_CTRL_GP11_I2S_BYTE_VAL &= ~(BIT0 | BIT1 | BIT2);

	PULL_EN_GP10_I2S_BYTE_VAL &= ~0xFF;
	PULL_EN_GP11_I2S_BYTE_VAL &= ~(BIT0 | BIT1 | BIT2);

	/* set to 2ch input, 2ch output */
	PIN_SHARING_SEL_4BYTE_VAL &= ~(BIT13 | BIT14 | BIT15 | BIT17 | BIT19 | BIT20 | BIT22);
	PIN_SHARING_SEL_4BYTE_VAL |= (BIT1 | BIT16 | BIT18 | BIT21);
}

static inline void i2s_clk_config(void)
{
	/* set to 11.288MHz */
	auto_pll_divisor(DEV_I2S, CLK_ENABLE , 0, 0);
	auto_pll_divisor(DEV_I2S, SET_PLLDIV, 1, 11288);

	/* Enable BIT4:ARFP clock, BIT3:ARF clock */
	PMCEU_VAL |= (BIT4 | BIT3);

	/* Enable BIT2:AUD clock */
	PMCE3_VAL |= BIT2;
}

void vt1603_snd_init(void)
{
	u8 data = 0;
	int ret = 0;
	char buf[512] = {0};
	int buflen = 512;
	unsigned long val = 0;
	int need_on = 0;
	printf("vt1603_snd_init need on!!\n");
	if(wmt_getsyspara("wmt.vt1603.out_on", buf, &buflen) == 0) {
		val = simple_strtoul(buf, NULL, 10);
		
		need_on = (int)val;
		
	}
	else
		need_on = 0;
	if (!need_on)
		return;
	
	i2s_pin_config();
	i2s_clk_config();
	
	printf("vt1603_snd_init need on!!\n");	
#if 0
	u8 tmp = 0;
	vt1603_spi_read(VT1603_R68, &data);
	vt1603_spi_read(VT1603_R68, &tmp);
	
	int i = 0;
	for (i=0; i<12; i++)
	{
		vt1603_spi_write(VT1603_R68, i);
		vt1603_spi_read(VT1603_R68, &tmp);
		
		printf("write %d, read back %d\n", i, tmp);
	}
#endif	
	//hp on
	ret = vt1603_spi_read(VT1603_R68, &data);
	data &= ~(1<<4);
	vt1603_spi_write(VT1603_R68, data);
	
	printf("<<<<%s write %d,",__func__,data);
	ret = vt1603_spi_read(VT1603_R68, &data);
	printf("read back %d\n", data);
	
	ret = vt1603_spi_read(VT1603_R69, &data);
	data |= 1<<2;
	vt1603_spi_write(VT1603_R69, data);
	
	printf("<<<<%s write %d,",__func__,data);
	ret = vt1603_spi_read(VT1603_R69, &data);
	printf("read back %d\n", data);
	
	ret = vt1603_spi_read(VT1603_R69, &data);
	data |= 1<<5;
	vt1603_spi_write(VT1603_R69, data);
	
	printf("<<<<%s write %d,",__func__,data);
	ret = vt1603_spi_read(VT1603_R69, &data);
	printf("read back %d\n", data);
	
	//spk on
	ret = vt1603_spi_read(VT1603_R25, &data);
	data |= 1<<1;
	vt1603_spi_write(VT1603_R25, data);
	
	printf("<<<<%s write %d,",__func__,data);
	ret = vt1603_spi_read(VT1603_R25, &data);
	printf("read back %d\n", data);
	
	ret = vt1603_spi_read(VT1603_R90, &data);
	data |= 1<<5;
	vt1603_spi_write(VT1603_R90, data);
	
	printf("<<<<%s write %d,",__func__,data);
	ret = vt1603_spi_read(VT1603_R90, &data);
	printf("read back %d\n", data);
	
	ret = vt1603_spi_read(VT1603_R90, &data);
	data |= 1<<3;
	vt1603_spi_write(VT1603_R90, data);
	
	printf("<<<<%s write %d,",__func__,data);
	ret = vt1603_spi_read(VT1603_R90, &data);
	printf("read back %d\n", data);
	
	
	
	
}

