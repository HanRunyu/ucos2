/**************************************************************************
*                                                                         *
*   PROJECT     : ARM port for UCOS-II                                    *
*                                                                         *
*   MODULE      : RELOCATE.c                                              *
*                                                                         *
*	AUTHOR		: HanRunyu												  *
*				  URL  : http://github.com/HanRunyu 					  *
*																		  *
*	PROCESSOR	: s3c2440  (32 bit ARM920T RISC core from Samsung)		  *
*																		  *
*	TOOLCHAIN	: arm-linux-gnueabi-gcc(Linaro 7.5.0)					  *
*                                                                         *
*   DESCRIPTION :                                                         *
*   Nand flash driver and code copy and bss segment clearing program.     *
*                                                                         *
**************************************************************************/



#include "s3c2440.h"
#include "def.h"


void nand_init(void)
{
	/* ����ʱ�� */
	rNFCONF = (0<<12)|(1<<8)|(0<<4);
	/* ʹ��NAND Flash������, ��ʼ��ECC, ��ֹƬѡ */
	rNFCONT = (1<<4)|(1<<1)|(1<<0);	
}

static void nand_select(void)
{
	rNFCONT &= ~(1<<1);	
}

static void nand_deselect(void)
{
	rNFCONT |= (1<<1);	
}

static void nand_cmd(S8 cmd)
{
	volatile U32 i;
	rNFCMD = cmd;
	for (i = 0; i < 10; i++);
}

static void nand_addr(U32 addr)
{
	U32 col  = addr % 2048;
	U32 page = addr / 2048;
	volatile U32 i;

	rNFADDR = col & 0xff;
	for (i = 0; i < 10; i++);		/* K9F2G08U0CоƬtwc��СΪ25ns��400MHzһ��ʱ��Ϊ2.5ns */
	rNFADDR = (col >> 8) & 0xff;
	for (i = 0; i < 10; i++);
	
	rNFADDR  = page & 0xff;
	for (i = 0; i < 10; i++);
	rNFADDR  = (page >> 8) & 0xff;
	for (i = 0; i < 10; i++);
	rNFADDR  = (page >> 16) & 0xff;
	for (i = 0; i < 10; i++);	
}

static void nand_page(U32 page)
{
	volatile U32 i;
	
	rNFADDR  = page & 0xff;
	for (i = 0; i < 10; i++);
	rNFADDR  = (page >> 8) & 0xff;
	for (i = 0; i < 10; i++);
	rNFADDR  = (page >> 16) & 0xff;
	for (i = 0; i < 10; i++);	
}

static void nand_col(U32 col)
{
	volatile U32 i;

	rNFADDR = col & 0xff;
	for (i = 0; i < 10; i++);
	rNFADDR = (col >> 8) & 0xff;
	for (i = 0; i < 10; i++);
}

static void nand_wait_ready(void)
{
	while (!(rNFSTAT & 1));
}

static S8 nand_data(void)
{
	return rNFDATA;
}

static U32 nand_bad(U32 addr)
{
	U32 col  = 2048;
	U32 page = addr / 2048;
	S8 val;

	/* 1. ѡ�� */
	nand_select();
	
	/* 2. ����������00h */
	nand_cmd(0x00);
	
	/* 3. ������ַ(��5������) */
	nand_col(col);
	nand_page(page);
	
	/* 4. ����������30h */
	nand_cmd(0x30);
	
	/* 5. �ж�״̬ */
	nand_wait_ready();

	/* 6. ������ */
	val = nand_data();
	
	/* 7. ȡ��ѡ�� */		
	nand_deselect();

	/* ��ȡblock�ĵ�2048���ֽ�,ÿһ��block�ļ�����ݴ����OOB��(2048�ֽڴ�)��
	   ������blockΪ���飬��2048�ֽڴ�������������0xff */
	if (val != 0xff)
		return 1;  /* bad blcok */
	else
		return 0;
}

void nand_read(U32 addr, S8 *buf, U32 len)
{
	U32 col = addr % 2048;
	U32 i = 0;
		
	while (i < len)
	{

		if (!(addr & 0x1FFFF) && nand_bad(addr)) /* һ��blockֻ�ж�һ�� */
		{
			addr += (128*1024);  /* ������ǰblock */
			continue;
		}

		/* 1. ѡ�� */
		nand_select();
		
		/* 2. ����������00h */
		nand_cmd(0x00);

		/* 3. ������ַ(��5������) */
		nand_addr(addr);

		/* 4. ����������30h */
		nand_cmd(0x30);

		/* 5. �ж�״̬ */
		nand_wait_ready();

		/* 6. ������ */
		for (; (col < 2048) && (i < len); col++)
		{
			buf[i] = nand_data();
			i++;
			addr++;
		}
		col = 0;

		/* 7. ȡ��ѡ�� */		
		nand_deselect();
		
	}
}

U32 isBootFromNorFlash(void)
{
	// OM[1:0]ֻ������״̬01����00,00��ʾNAND������01��ʾ16bitNOR����
	if(rBWSCON & 0x6)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void copy2sdram(void)
{
	/* Ҫ��lds�ļ��л�� __code_start, __bss_start
	 * Ȼ���0��ַ�����ݸ��Ƶ�__code_start
	 */
	extern U32 __code_start, __bss_start;

	volatile U32 *dest = (volatile U32 *)&__code_start;
	volatile U32 *end = (volatile U32 *)&__bss_start;
	volatile U32 *src = (volatile U32 *)0;
	U32 len = (U32)(&__bss_start) - (U32)(&__code_start);

	if (isBootFromNorFlash())
	{
		while (dest < end)
		{
			*dest++ = *src++;
		}
	}
	else
	{
		nand_init();
		nand_read((U32)src, (S8 *)dest, len);
	}
}

void clean_bss(void)
{
	/* Ҫ��lds�ļ��л�� __bss_start, __end */
	extern U32 __bss_start,__end;

	volatile U32 *start = (volatile U32 *)&__bss_start;
	volatile U32 *end = (volatile U32 *)&__end;


	while (start <= end)
	{
		*start++ = 0;
	}
}
