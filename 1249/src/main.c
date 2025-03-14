/*
 * Copyright (C) 2009 - 2019 Xilinx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include "xparameters.h"
#include "platform.h"
#include "fat/ff.h"			/* Declarations of FatFs API */
#include "fat/diskio.h"		/* Declarations of device I/O functions */
#include "fat/ffconf.h"
#include "nhc_amba.h"
#include "fifo.h"
#include "xil_cache.h"
#include "xil_io.h"
#include "cmd.h"
#include "xuartlite.h"
#include "Ring_Buffer/ringbuffer_u8.h"

extern StructMsg			CurMsg;
extern StructMsgQuery		MsgQuery;
extern uint32_t Stop_flag;
extern uint8_t flag_1x;
extern uint8_t flag_tcp;
extern XUartLite UartLite;
uint8_t sts=0;
uint8_t FinishFLAG=0;
uint32_t slotNum=0;
uint32_t equipNum=0;
uint32_t temper_power=0;
extern uint8_t rxflag;
extern uint8_t Stop_write;
#define CLEAN_PARAM   do{flag_1x=0;flag_tcp=0;Stop_write=0;}while(0)

uint64_t file_cap=0;
u32 DestinationBuffer[MAX_DATA_BUFFER_SIZE * WORD_SIZE];
u32 DestinationBuffer_1[MAX_DATA_BUFFER_SIZE * WORD_SIZE];
/********************文件系统格式化与挂载有关参数*******************/
static BYTE Buff[4096];  //与格式化空间大小有关
FATFS fs;
FRESULT fr;
FILINFO fno;
FIL file;
FIL wfile;
FIL rfile;
uint64_t REMAIN_SPACE=0;
/************************************************************/
int main()
{
		int ret,br=0,bw=0,i=0;
		xil_printf("UCAS Project 1249!\r\n");
#if   1
		init_platform();
		MsgQueryInit();
		XLLFIFO_SysInit();
		UartLiteIntr();
		SendToMcu(&UartLite,0xA5);//获取槽位号的指令
		while(FinishFLAG==0x0)
		{
			i++;
			if(i>0x200000) break;  //超时退出
		}
		SimpleTcpDmaInit();
		Simple1xDmaInit();

		DiskInit();
		IpSet(equipNum,slotNum);
//		write_data();
//		read_data();
#if     10
//		Xil_ICacheDisable();
//		Xil_DCacheDisable();
		// 执行文件系统操作
#if     0//format the filesysterm
		ret = f_mkfs(
			"",	/* Logical drive number */
			0,			/* Format option  FM_EXFAT*/
			Buff,			/* Pointer to working buffer (null: use heap memory) */
			sizeof Buff			/* Size of working buffer [byte] */
			);
		if (ret != FR_OK) {
			xil_printf("f_mkfs  Failed! ret=%d\n", ret);
			return 0;
		}
#endif //format the filesysterm

/********* mount filesysterm *********/
#if     10
    	ret = f_mount (&fs, "", 1);
		if (ret != FR_OK)
		{
			xil_printf("f_mount  Failed! ret=%d\n", ret);
			//format the filesysterm
			ret = f_mkfs(
				"",	/* Logical drive number */
				0,			/* Format option  FM_EXFAT*/
				Buff,			/* Pointer to working buffer (null: use heap memory) */
				sizeof Buff			/* Size of working buffer [byte] */
				);
			if (ret != FR_OK) {
				xil_printf("f_mkfs  Failed! ret=%d\n", ret);
				return 0;
			}
		}
		xil_printf(" Init All ok!\r\n");
#endif

//		run_cmd_d205_8x();
//		Xil_ICacheEnable();
//		Xil_DCacheEnable();
//		uint32_t  buf = (void *)(0x80000000);
//		ret = f_open(&wfile,"D", FA_CREATE_ALWAYS | FA_WRITE |FA_READ);
//		if (ret != FR_OK)
//		{
//			xil_printf("f_open Failed! ret=%d\r\n", ret);
//			return ret;
//		}
//		for(int i=0;i<3;i++)
//		{
//			FLAG=1;
//			ret = f_write1(
//				&wfile,			/* Open file to be written */
//				buf,			/* Data to be written */
//				0x2000000,			/* Number of bytes to write */
//				&bw				/* Number of bytes written */
//			);
//			if (ret != FR_OK)
//			{
//				 xil_printf(" f_write Failed! %d\r\n",ret);
//				 f_close(&wfile);
//				 return ret;
//			}
//
//			buf=buf+0x2000000;
//		}



	/* receive and process packets */
	while(1)
	{
//		if((rxflag==1))
//		{
//			cmd_parse();
//			rxflag=0;
//		}
//		run_cmd_d203(0);
//		cmd_reply_a203(0x0,0xA2,0x1,0x11);
//		run_cmd_f201(0x0);
#if  1
		memset(&CurMsg,0,sizeof(CurMsg));
		GetMessage(&CurMsg);
		switch(CurMsg.HandType)
		{
				case 0XA2:
					xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
					switch(CurMsg.HandId)
					{
						case 0x1:
							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
							xil_printf("------Start executing commands!------\r\n");
							ret=run_cmd_a201(&CurMsg);
							if(ret!=0)
							{
								xil_printf("------commands executing failed!------ ret=%d\r\n",ret);
								cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x10);
								break;
							}
							cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x11);
							xil_printf("------commands executing complete!------\r\n");
						break;

						case 0x2:
							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
							xil_printf("------Start executing commands!------\r\n");
							ret=run_cmd_a202(&CurMsg);
							if(ret!=0)
							{
								xil_printf("------commands executing failed!------ ret=%d\r\n",ret);
								break;
							}
							xil_printf("------commands executing complete!------\r\n");
						break;

						case 0x4:
							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
							xil_printf("------Start executing commands!------\r\n");
							ret=run_cmd_a204(&CurMsg);
							if(ret!=0)
							{
								xil_printf("------commands executing failed!------ ret=%d\r\n",ret);
								cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x10);
								break;
							}
							cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x11);
							xil_printf("------commands executing complete!------\r\n");
						break;

						case 0x5:
							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
							xil_printf("------Start executing commands!------\r\n");
							ret=run_cmd_a205(&CurMsg);
							if(ret!=0)
							{
								xil_printf("------commands executing failed!------ ret=%d\r\n",ret);
								cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x10);
								break;
							}
							cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x11);
							xil_printf("------commands executing complete!------\r\n");
						break;

						default:
						break;
					}
				break;

				case 0XB2:
					switch(CurMsg.HandId)
					{
 						case 0x1:
							xil_printf("%s %d  CurMsg.HandType:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType);
							xil_printf("------Start executing commands!------\r\n");
							ret=run_cmd_b201(&CurMsg);
							if(ret!=0)
							{
								xil_printf("------commands executing failed!------ ret=%d\r\n",ret);
								cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x10);
								break;
							}
							cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x11);
							xil_printf("------commands executing complete!------\r\n");
						break;

 						case 0x2:
							xil_printf("%s %d  CurMsg.HandType:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType);
							xil_printf("------Start executing commands!------\r\n");
							ret=run_cmd_b202(&CurMsg);
							if(ret!=0)
							{
								xil_printf("------commands executing failed!------ ret=%d\r\n",ret);
//								cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x10);
								break;
							}
//							cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x11);
							xil_printf("------commands executing complete!------\r\n");
						break;

 						default:
 							break;
					}
				break;

				case 0XD2:
					switch(CurMsg.HandId)
					{
						case 0x1:
							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
							xil_printf("------Start executing commands!------\r\n");
							ret=run_cmd_d201(&CurMsg);
							if(ret!=0)
							{
								xil_printf("------commands executing failed!------ ret=%d\r\n",ret);
								cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x10);
								break;
							}
							cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x11);
							xil_printf("------commands executing complete!------\r\n");
						break;

						case 0x2:
//							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
//							xil_printf("------Start executing commands!------\r\n");
							ret=run_cmd_d202(&CurMsg);
							if(ret!=0)
							{
								xil_printf("------commands executing failed!------ ret=%d\r\n",ret);
//								cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x10);
								break;
							}
//							cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x11);
//							xil_printf("------commands executing complete!------\r\n");
						break;

						case 0x3:
							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
							xil_printf("------Start executing commands!------\r\n");
							ret=run_cmd_d203(&CurMsg);
							if(ret!=0)
							{
								xil_printf("------commands executing failed!------ ret=%d\r\n",ret);
//								    cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x10);
								break;
							}
//							    cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x11);
							xil_printf("------commands executing complete!------\r\n");
						break;
						case 0x4:
							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
							xil_printf("------Start executing commands!------\r\n");
							ret=run_cmd_d204(&CurMsg);
							if(ret!=0)
							{
								xil_printf("------commands executing failed!------ ret=%d\r\n",ret);
//								cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x10);
								break;
							}
//							    cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x11);
							xil_printf("------commands executing complete!------\r\n");
						break;

						case 0x7:
							xil_printf("%s %d  CurMsg.HandType:0x%x CurMsg.HandId:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType,CurMsg.HandId);
							xil_printf("------Start executing commands!------\r\n");
							ret=run_cmd_d207(&CurMsg);
							if(ret!=0)
							{
								xil_printf("------commands executing failed!------ ret=%d\r\n",ret);
//								cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x10);
								break;
							}
//							    cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x11);
							xil_printf("------commands executing complete!------\r\n");
						break;

						default:
						break;
					}
				break;

				case 0XF2:
					xil_printf("%s %d  CurMsg.HandType:0x%x\r\n", __FUNCTION__, __LINE__,CurMsg.HandType);
					xil_printf("------Start executing commands!------\r\n");
					ret=run_cmd_f201(&CurMsg);
					if(ret!=0)
					{
						xil_printf("------commands executing failed!------ ret=%d\r\n",ret);
						cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x10);
						break;
					}
					cmd_reply_a203(CurMsg.PackNum,CurMsg.HandType,CurMsg.HandId,0x11);
					xil_printf("------commands executing complete!------\r\n");
				break;

				default:
				break;
		}//switch
		CLEAN_PARAM;
//		}
#endif
	}//while
#endif
#endif
	/* never reached */
	cleanup_platform();

	return 0;
}

int write_data(void)
{
		uint32_t ret=0,i=0,x=0,h=0;
		u16 unicode_u16=0;
		int k=0;
		uint32_t Status=0,bw=0;
		u32 file_cmd=0;
		uint8_t sts;
		uint32_t cmd_write_cnt=0,cmd_len=0;
//		uint32_t  len= 0x20000000;
		uint32_t  len= 0x0;
		uint32_t  buff = (void *)(0x80000000);
		uint64_t  slba=0x4000000;
		xil_printf("Waiting FPGA Vio Ctrl Read Write Start\r\n");
#if  1
		while (1)
		{
//			xil_printf("Start Write!\r\n");
			if (RxReceive(DestinationBuffer,&cmd_len) == XST_SUCCESS)
			{

				buff =DestinationBuffer[0];  // 保存写入数据的DDR地址
				len  =DestinationBuffer[1];  // 写入数据的长度
//				buff =0x80000000;  // 保存写入数据的DDR地址
//				len  =0x20000000;  // 写入数据的长度

				if(buff==0x3C3CBCBC)		// 3.19号改 by lyh
				{
					xil_printf("I/O Write Finish!\r\n");
					xil_printf("w_count = %u\r\n",cmd_write_cnt);
					for(i=0;i<NHC_NUM;i++)
					{
						while (nhc_queue_ept(i) == 0)
						{
							do {
								sts = nhc_cmd_sts(i);
							}while(sts == 0x01);
						}
					}
					break;
				}
				if(cmd_write_cnt==16)		// 3.19号改 by lyh
				{
					xil_printf("I/O Write Finish!\r\n");
					xil_printf("w_count = %u\r\n",cmd_write_cnt);
					for(i=0;i<NHC_NUM;i++)
					{
						while (nhc_queue_ept(i) == 0)
						{
							do {
								sts = nhc_cmd_sts(i);
							}while(sts == 0x01);
						}
					}
					break;
				}
#if 0
				for(int i=0;i<32;i++)
				{
					if (io_write2(NHC_NUM,0x1,buff, slba, 0x1000000, 0x0) != 0x02)
					{
						 xil_printf("I/O Write Failed!\r\n");
						 return 0;
					}
					buff += 0x1000000;
					slba += 0x1000000;
				}
#endif
				if (io_write2(NHC_NUM,0x1,buff, slba, len, 0x0) != 0x02)
				{
					 xil_printf("I/O Write Failed!\r\n");
					 return 0;
				}
//				buff += len;
				slba += len;
				cmd_write_cnt += 1;
//				xil_printf("buff:0x%lx \r\n",buff);
			}
			else
			{
				for(i=0;i<NHC_NUM;i++)
				{
					do {
							sts = nhc_cmd_sts(i);
						}while(sts == 0x01);
				}
			}

			if (buff <(0xDFFFFFFF-len))
			{
				buff +=len ;
			}
			else
			{
				buff = 0x80000000;
			}

			for(i=0;i<NHC_NUM;i++)
			{
				while (nhc_queue_ept(i) == 0)
				{
					do {
						sts = nhc_cmd_sts(i);
					}while(sts == 0x01);
				}
			}

		 }   // while
		 file_cap=slba;
		 return 0;
#endif
}

int read_data(void)
{
	 int i=0,Status,ret,h=0;
	 uint8_t sts;
	 int64_t size=0;
	 int br;
	 uint64_t  slba_r=0x4000000;
	 uint32_t  r_count=0,cmd_len=0;;
	 uint32_t  len;
	 uint32_t  buff_r=(void *)(0x80000000);
	 size=file_cap;
	 len= OFFSET_SIZE;
	 while(1)
	 {
		 	if(io_read3(NHC_NUM, 0x1, buff_r, slba_r, len, 0x0) != 0x02)
			{
				xil_printf("I/O Read Failed!\r\n");
				return 0;
			}
//		 	Xil_DCacheFlushRange((UINTPTR)buff_r, 0x20000000);
#if   1   // 回读存放的地址为:0x80000000~0x9FFFFFFF
			r_count++;
			slba_r+= OFFSET_SIZE;
			DestinationBuffer_1[0]=len;
			DestinationBuffer_1[1]=buff_r;
			xil_printf("buff_r:0x%lx \r\n",buff_r);
			XLLFIFO_SysInit();
//			XLLFIFO1_SysInit();
			ret = TxSend(DestinationBuffer_1,8);
			if (ret != XST_SUCCESS)
			{
				 xil_printf("TxSend Failed! ret=%d\r\n", ret);
				 return ret;
			}
			do
			{
				RxReceive(DestinationBuffer_1,&cmd_len);
			}while(!(0xaa55aa55 == DestinationBuffer_1[0]));

			if (buff_r <(0xDFFFFFFF-OFFSET_SIZE))
			{
				buff_r += OFFSET_SIZE;
			}
			else
			{
				buff_r = 0x80000000;
			}
#endif
			 for(i=0;i<NHC_NUM;i++)
			 {
					while (nhc_queue_ept(i) == 0)
					{
						do {
							sts = nhc_cmd_sts(i);
						}while(sts == 0x01);
					}
			 }
			 size-=len;
			 if((size<OFFSET_SIZE)&&(size>0))
			 {
				 len=size;
			 }
			 else if(size<=0)
			 {
					xil_printf("I/O Read or Write Test Finish!\r\n");
					xil_printf("r_count=%u\r\n",r_count);
					for(i=0;i<NHC_NUM;i++)
					{
						while (nhc_queue_ept(i) == 0)
						{
							do {
								sts = nhc_cmd_sts(i);
							}while(sts == 0x01);
						}
					}
					break;
			}
//			 if(r_count>=3200)
//			 {
//				 xil_printf("here\r\n");
//			 }

	 }  //while
	 return 0;
}

