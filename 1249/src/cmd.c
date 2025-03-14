#include "cmd.h"
#include "string.h"
#include "xil_cache.h"
#include "cmd.h"
#include "string.h"
#include "fat/ff.h"		/* Declarations of FatFs API */
#include "xllfifo_drv.h"
#include "nhc_amba.h"
#include "mem_test.h"
#include "wchar.h"    // 2023.9.20
#include "FIFO.h"
#include "simple_dma.h"
#include "xuartLite.h"
#include "xllfifo.h"

extern XLlFifo Fifo0;
#define STM32_UART      1
#define MERGE2(a, b) a ## b
#define CVTBL(tbl, cp) MERGE2(tbl, cp)
#define FF_CODE_PAGE	936

typedef unsigned int	UINT;	/* int must be 16-bit or 32-bit */
typedef unsigned char	BYTE;	/* char must be 8-bit */
typedef uint16_t		WORD;	/* 16-bit unsigned integer */
typedef uint32_t		DWORD;	/* 32-bit unsigned integer */
typedef uint64_t		QWORD;	/* 64-bit unsigned integer */
typedef WORD			WCHAR;	/* UTF-16 character type */


StructMsg			CurMsg;
StructMsgQuery		MsgQuery;
int Packet_body_length=0;
int CheckCode=0;
extern uint8_t   cancel;
extern FIL file;
extern FIL wfile;
extern FIL rfile;
extern uint8_t flag_tcp;
extern uint8_t flag_1x;
extern XUartLite UartLite;
extern uint64_t REMAIN_SPACE;
extern QWORD  LOSS;
extern QWORD  TOTAL_CAP;
uint32_t write_len=0;

DIR dir;
int result_a201=0x0;
int result_a204=0x0;
int result_a205=0x0;
int result_b201=0x0;
int result_d201=0x0;
int result_d205=0x0;
int result_d20A=0x0;
int result_f201=0x0;
int Read_Packet_Size=0x10000000;
//int Read_Packet_Size1=0x100000;
int Read_Packet_Size1=0x10000000;
//uint8_t HasCreat=0;
uint8_t Stop_read=0;
uint8_t Stop_write=0;
//extern struct message_struct fifodata;
//extern int a;
/**********************FIFO有关参数设置*************************/
u32 DestinationBuffer[MAX_DATA_BUFFER_SIZE * WORD_SIZE];
u32 DestinationBuffer_1[MAX_DATA_BUFFER_SIZE * WORD_SIZE];
/************************************************************/
//int16_t TcpProcessTransmission(uint8_t *pData, uint16_t Len);
//char buff[4]={0xAA,0x55,0xAA,0x55};
void MsgQueryInit(void)
{
	memset( &MsgQuery, 0, sizeof(MsgQuery) );
	MsgQuery.Start = 0;
	MsgQuery.End = 0;
	CurMsg.HandType = 0x00;
	CurMsg.HandId = 0x00;
}

void GetMessage(StructMsg *pMsg)
{
	u32 i;
	StructMsg	*p;

	if((MsgQuery.Start>=MSG_QUERY)||(MsgQuery.End>=MSG_QUERY)) {
		pMsg->HandType = MSG_WARNING;
		pMsg->HandId = WARNING_MSG_OVERFLOW;
		//xil_printf( "GetMessage High OverFlow\r\n" );
	}
	else
	{
		if(MsgQuery.Start!=MsgQuery.End)
		{
			p = &(MsgQuery.MsgQuery[MsgQuery.End]);
			if(++MsgQuery.End >= MSG_QUERY)
				MsgQuery.End = 0;
		}
		else
		{
			pMsg->HandType = MSG_NULL;
			return;
		}
		pMsg->HandType  = p->HandType;
		pMsg->HandId = p->HandId;
		pMsg->DataLen = p->DataLen;
		pMsg->PackNum= p->PackNum;
		//xil_printf("%s %d   p->HandType:%u  p->HandId:%u  pMsg->DataLen:%u\n", __FUNCTION__, __LINE__,p->HandType,p->HandId,p->DataLen);
		for( i=0; i<pMsg->DataLen; i++ )
			pMsg->MsgData[i]  = p->MsgData[i];
	}
}

void SendMessage(StructMsg *pMsg)
{
		u32 i;
		StructMsg	*p;
//		xil_printf("%s %d   p->HandType:0x%x  p->HandId:0x%x  pMsg->DataLen:%u\r\n", __FUNCTION__, __LINE__,pMsg->HandType,pMsg->HandId,pMsg->DataLen);
		if((MsgQuery.Start>=MSG_QUERY)||(MsgQuery.End>=MSG_QUERY))
			return;
			if((MsgQuery.Start==(MsgQuery.End-1))||
				((MsgQuery.End==0)&&(MsgQuery.Start==(MSG_QUERY-1))))
			{
				p = &(MsgQuery.MsgQuery[MsgQuery.End]);
				pMsg->HandType = MSG_WARNING;
				pMsg->HandId = WARNING_MSG_OVERFLOW;
				//xil_printf( "SendMessage High OverFlow\r\n" );
			}
			else
			{
				p = &(MsgQuery.MsgQuery[MsgQuery.Start]);  // 消息入队
				if(++MsgQuery.Start >= MSG_QUERY)
					MsgQuery.Start = 0;
			}

			p->HandType  = pMsg->HandType;
			p->HandId = pMsg->HandId;
			p->DataLen = pMsg->DataLen;
			p->PackNum= pMsg->PackNum;
			for( i=0; i< pMsg->DataLen; i++ )
			{
				p->MsgData[i]  = pMsg->MsgData[i];
			}
//			xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
}

int cmd_parse(void)
{
		StructMsg TMsg;
		int i=0;
		uint32_t a;
//		Xil_DCacheInvalidateRange((UINTPTR)pbuff, CMD_PACK_LEN);
		Xil_DCacheInvalidateRange((UINTPTR)CmdRxBufferPtr, CMD_PACK_LEN);
//		Xil_DCacheFlushRange((UINTPTR)CmdRxBufferPtr, MAX_PKT_LEN);
		int rev=CW32(CmdRxBufferPtr[i+0],CmdRxBufferPtr[i+1],CmdRxBufferPtr[i+2],CmdRxBufferPtr[i+3]);
		if(0x55555555 != CW32(CmdRxBufferPtr[i+0],CmdRxBufferPtr[i+1],CmdRxBufferPtr[i+2],CmdRxBufferPtr[i+3]))
			return -1;
		i+=4;
		if(SRC_ID != CW32(CmdRxBufferPtr[i+0],CmdRxBufferPtr[i+1],CmdRxBufferPtr[i+2],CmdRxBufferPtr[i+3]))
			return -1;
		i+=4;
		if(DEST_ID != CW32(CmdRxBufferPtr[i+0],CmdRxBufferPtr[i+1],CmdRxBufferPtr[i+2],CmdRxBufferPtr[i+3]))
			return -1;
		i+=4;
		TMsg.HandType = CW32(CmdRxBufferPtr[i+0],CmdRxBufferPtr[i+1],CmdRxBufferPtr[i+2],CmdRxBufferPtr[i+3]);
		i+=4;
		TMsg.HandId = CW32(CmdRxBufferPtr[i+0],CmdRxBufferPtr[i+1],CmdRxBufferPtr[i+2],CmdRxBufferPtr[i+3]);
		i+=4;
		TMsg.PackNum = CW32(CmdRxBufferPtr[i+0],CmdRxBufferPtr[i+1],CmdRxBufferPtr[i+2],CmdRxBufferPtr[i+3]);
//		i+=8;	//1.11改
		i+=4;

		while(1)
		{
			if(CW32(CmdRxBufferPtr[i+0],CmdRxBufferPtr[i+1],CmdRxBufferPtr[i+2],CmdRxBufferPtr[i+3])==0xAAAAAAAA)  break;
			i++;
			if(i>0x200000) return 0;  //超时退出
			Packet_body_length++;
		}
		Packet_body_length-=4;
		TMsg.DataLen=Packet_body_length;
		Packet_body_length=0;
		i-=4;
		CheckCode=CW32(CmdRxBufferPtr[i+0],CmdRxBufferPtr[i+1],CmdRxBufferPtr[i+2],CmdRxBufferPtr[i+3]);
		cmd_type_id_parse(&TMsg);
}

void cmd_type_id_parse(StructMsg *pMsg)
{
		int i=0;
		StructMsg TMsg={0};
		TMsg.HandType=pMsg->HandType;
		TMsg.HandId=pMsg->HandId;
		TMsg.PackNum=pMsg->PackNum;
		TMsg.DataLen=pMsg->DataLen;
//		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		switch(pMsg->HandType)
		{
				case 0xa2:
					switch(pMsg->HandId)
					{
						case 0x1:
//							xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
//								xil_printf("%x",TMsg.MsgData[i]);
							}
						break;
						case 0x2:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
						break;
						case 0x4:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
						break;
						case 0x5:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
						break;
//						case 0x8:
//							for(i=0; i < TMsg.DataLen; i++)
//							{
//								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
//							}
//						break;
						default:
							return 0;
						break;
					}
				break;
				case 0xb2:
					switch(pMsg->HandId)
				    {
					    case 0x1:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
						break;

					    case 0x2:
					    	for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
						break;
					    default:
							return 0;
						break;
					}
				break;

				case 0xd2:
					switch(pMsg->HandId)
					{
						case 0x1:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
						break;
						case 0x2:
//							Xil_L1DCacheFlush();
//							TMsg.DataLen=2060;
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}

//						    data_flag=1;
//							for(i=0; i <1050636; i++)
//							{
//								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
//							}
						break;
						case 0x3:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
						break;
						case 0x4:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
						break;
						case 0x5:
//							for(i=0; i < TMsg.DataLen; i++)
//							{
//								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
//							}
							Stop_write=1;
						break;
						case 0x6:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
							Stop_read=1;
						break;
						case 0x7:
							for(i=0; i < TMsg.DataLen; i++)
							{
								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
							}
						break;
//						case 0x8:
//							for(i=0; i < TMsg.DataLen; i++)
//							{
//								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
//							}
//						break;
//						case 0x9:
//							for(i=0; i < TMsg.DataLen; i++)
//							{
//								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
//							}
//						break;
//						case 0xA:
//							for(i=0; i < TMsg.DataLen; i++)
//							{
//								TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
//							}
//							Stop_read=1;
//						break;
						default:
							return 0;
						break;
					}
				break;

				case 0xf2:
					for(i=0; i < TMsg.DataLen; i++)
					{
						TMsg.MsgData[i] = CmdRxBufferPtr[i+24];
					}
				break;
				default:
					return 0;
				break;
		}
		SendMessage(&TMsg);
}

void ConvertReverse(u16 *str)   // 数组里类似0x3000的数,转换成0x0030
{
		int x=0;
		u8 STR=0;
		u8 STR1=0;
		for(x=0;;x++)
		{
			STR=str[x]>>8;
			STR1=str[x];
			if((STR<=0x7E)&&(STR1==0))
			{
				//按字节倒置
				str[x]>>=8;
			}
			if(str[x+1]=='\0')  break;
		}
}

void convert(u16 *str1,u8 *str2)	//add by lyh on 1.22  单字节字符的ASCLL码后补零凑成16位,16位GB2312保持不变
{
		int x=0,h=0;
		for (x = 0; x < 256; x++)
		{
				u8 name=str2[x];
				if(name<=0x7E)
				{
					str1[h]=name;
					str1[h]<<=8;
				}
				else
				{
					str1[h]=name;
					str1[h]<<=8;
					name=str2[++x];
					str1[h]|=name;
				}
				h++;
				if(str2[x+1]=='\0')  break;
		}
}

//递归删除文件夹
FRESULT delete_dir (BYTE* path)
{
    FRESULT res;
    DIR dir;
    UINT i;
    static FILINFO fno;
    BYTE FilePath[100]={0};
    int flag=0;
    res = f_opendir(&dir, path);                       /* Open the directory */
    if (res == FR_OK)
    {
        for (;;)
        {
            res = f_readdir(&dir, &fno);                   /* Read a directory item */
            if (res != FR_OK || fno.fname[0] == 0) /* Break on error or end of dir */
			{
                if(res==FR_OK && flag==0) 			// empty directory
                {
                	flag=1;
                	res = f_unlink (path); 			// delete the empty directory
					if (res != FR_OK) {
						xil_printf("Delete directory  Failed! ret=%d\r\n", res);
						return -1;
					}
					printf("succeed to delete directory: %s ! \r\n",path);
                }
            	break;
			}
            if (fno.fattrib & AM_DIR)       /* It is a directory */
			{
					i = strlen(path);
					sprintf(&path[i], "/%s", fno.fname);
					res = delete_dir(path);                    /* Enter the directory */
					if (res != FR_OK) break;
						path[i] = 0;
            }
            else 							/* It is a file. */
            {
//					printf("%s/%s \r\n", path, fno.fname);
					sprintf(FilePath,"%s/%s",path, fno.fname);
					if(f_unlink(FilePath) == FR_OK)
					{
						printf("succeed to delete file: %s ! \r\n",FilePath);
					}
            }
        }
        f_closedir(&dir);
    }
    else
    {
    	xil_printf("Failed to open! res=%d\r\n", res);
    }
    return res;
}

int run_cmd_a201(StructMsg *pMsg)
{
	int ret=0,i=0,x=0,temp=0,h=0;
	u16 unicode_u16=0;
	u32 file_cmd=0;
	WCHAR cmd_str_1[1024]={0},cmd_str_2[1024]={0};
	BYTE  cmd_str_11[100]={0},cmd_str_21[100]={0};
	FRESULT fr1;
	FILINFO fno1;
	uint32_t  databuf = (void *)(0xA0000000);
	uint32_t upload_time=1,lastpack_Size=0,wlen=0,COUNT=0;
//	xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
	file_cmd = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
	i=i+4;
	temp=i;   // 9.7 LYH
	xil_printf("%s %d cmd:0x%x\r\n", __FUNCTION__, __LINE__,file_cmd);
	for (x = 0; x < 1024; x++)
	{
			unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
			cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
			if(cmd_str_1[x]<=0x7E)
			{
				cmd_str_11[h++]=cmd_str_1[x];
			}
			else
			{
				cmd_str_11[h++]=(cmd_str_1[x]>>8);
				cmd_str_11[h++]=cmd_str_1[x];
			}
			if ((cmd_str_1[x] == '\0'))  break;
	}
	xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_11);
	switch(file_cmd)
	{
		case NEW_FILE:       //新建文件
			ret = f_open (&file, cmd_str_11, FA_CREATE_ALWAYS | FA_WRITE);
//			ret = f_open (&file, "abc", FA_CREATE_ALWAYS | FA_WRITE);
			if (ret != FR_OK) {
				xil_printf("f_open Failed! ret=%d\r\n", ret);
//			    cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x10);  // lyh 2023.8.15
//				cmd_reply_a203(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x10);
				return -1;
			}
			xil_printf("NEW_FILE Command Success! file name:%s\r\n",cmd_str_11);
			ret=f_close(&file);
			if (ret != FR_OK) {
				xil_printf("close file Failed! ret=%d\r\n", ret);
				return -1;
			}
		break;

/***************************************************************************************/
/***************************************************************************************/
		case NEW_FOLDER:    //新建文件夹
			ret =f_mkdir(cmd_str_11);
//			ret =f_mkdir("bcd");
			if (ret != FR_OK) {
				xil_printf("f_mkdir  Failed! ret=%d\r", ret);
				if(ret==8) xil_printf("Failed reason:FR_EXIST\n");
//				cmd_reply_a203(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x10);
				return -1;
			}
			xil_printf("NEW_FOLDER Command Success! folder name:%s\r\n",cmd_str_11);
		break;


/***************************************************************************************/
/***************************************************************************************/
		case DEL_FILE:       //删除文件时，除了判断文件是否存在外，还需判断文件是否是只读属性
			fr1 = f_stat(cmd_str_11, &fno1);    //lyh 9.4改
			switch(fr1)
			{
				case FR_OK:
					 if(fno1.fattrib & AM_RDO){
						 xil_printf("FR_DENIED, the file's attribute is read-only\r\n");
						 return -1;
				}
				ret = f_unlink (cmd_str_11); // LYH 2023.9.4
				if (ret != FR_OK) {
					xil_printf("Delete file  Failed! ret=%d\r\n", ret);
					return -1;
				}
				xil_printf("DEL_FILE Command Success! file name:%s\r\n",cmd_str_11);
				break;

				case FR_NO_FILE:
					xil_printf("%s is not exist.\r\n", cmd_str_11);// LYH 2023.9.4
					return -1;
				break;
				default:
					xil_printf("An error occured. (%d)\r\n", fr1);
				break;
			}
		break;


/***************************************************************************************/
/***************************************************************************************/
		case DEL_FOLDER:  // 删除文件夹
			ret = delete_dir(cmd_str_11);//  Writed on 2023.8.15 by LYH
			if (ret != FR_OK) {
				xil_printf("delete directory Failed! ret=%d\r\n", ret);
				return -1;
			}

			xil_printf("DEL_FOLDER Command Success! folder name:%s\r\n",cmd_str_11);
		break;
/***************************************************************************************/
/***************************************************************************************/
		case RENAME_FILE:    //重命名文件
			i=temp+2048;
			h=0;
			for (x = 0; x < 1024; x++)
			{
				unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
				cmd_str_2[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
				if(cmd_str_2[x]<=0x7E)
				{
					cmd_str_21[h++]=cmd_str_2[x];
				}
				else
				{
					cmd_str_21[h++]=(cmd_str_2[x]>>8);
					cmd_str_21[h++]=cmd_str_2[x];
				}
				if (cmd_str_2[x] == '\0') break;
			}
			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_21);

			ret = f_rename (cmd_str_11, cmd_str_21);
			if (ret != FR_OK) {
				xil_printf("rename file Failed! ret=%d\r\n", ret);
				return -1;
			}
			xil_printf("Success to rename file:%s to file:%s! \r\n",cmd_str_11,cmd_str_21);
		break;
/***************************************************************************************/
/***************************************************************************************/

		case RENAME_FOLDER:   //重命名文件夹
			i=temp+2048;
			h=0;
			for (x = 0; x < 1024; x++)
			{
				unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
				cmd_str_2[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
				if(cmd_str_2[x]<=0x7E)
				{
					cmd_str_21[h++]=cmd_str_2[x];
				}
				else
				{
					cmd_str_21[h++]=(cmd_str_2[x]>>8);
					cmd_str_21[h++]=cmd_str_2[x];
				}
				if (cmd_str_2[x] == '\0') break;
			}
			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_21);
			ret = f_rename (cmd_str_11, cmd_str_21);
			if (ret != FR_OK) {
				xil_printf("rename directory Failed! ret=%d\r\n", ret);
				return -1;
			}
			xil_printf("Success to copy directory:%s to directory:%s ! \r\n",cmd_str_11,cmd_str_21);
		break;

/***************************************************************************************/
/***************************************************************************************/

		case MOVE_FOLDER:   	//移动文件夹
			i=temp+2048;
			h=0;
			for (x = 0; x < 1024; x++)
			{
				unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
				cmd_str_2[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
				if(cmd_str_2[x]<=0x7E)
				{
					cmd_str_21[h++]=cmd_str_2[x];
				}
				else
				{
					cmd_str_21[h++]=(cmd_str_2[x]>>8);
					cmd_str_21[h++]=cmd_str_2[x];
				}
				if (cmd_str_2[x] == '\0') break;
			}

			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_21);
			ret = f_rename(cmd_str_11, cmd_str_21);
			if (ret != FR_OK) {
				xil_printf("move directory Failed! ret=%d\r\n", ret);
				return -1;
			}
			xil_printf("Success to move directory:%s to directory:%s ! \r\n",cmd_str_11,cmd_str_21);
		break;
/***************************************************************************************/
/***************************************************************************************/

		case MOVE_FILE:      	//移动文件
			i=temp+2048;
			h=0;
			for (x = 0; x < 1024; x++)
			{
				unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
				cmd_str_2[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
				if(cmd_str_2[x]<=0x7E)
				{
					cmd_str_21[h++]=cmd_str_2[x];
				}
				else
				{
					cmd_str_21[h++]=(cmd_str_2[x]>>8);
					cmd_str_21[h++]=cmd_str_2[x];
				}
				if (cmd_str_2[x] == '\0') break;
			}

			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_21);
			ret = f_rename(cmd_str_11, cmd_str_21);
			if (ret != FR_OK) {
				xil_printf("move file  Failed! ret=%d\r\n", ret);
				return -1;
			}
			xil_printf("Success to move file:%s to file:%s ! \r\n",cmd_str_11, cmd_str_21);
		break;
/***************************************************************************************/
/***************************************************************************************/

		case OPEN_FILE:    //打开文件
			ret = f_open (&file, cmd_str_11, FA_OPEN_EXISTING| FA_WRITE|FA_READ);
			if (ret != FR_OK) {
				xil_printf("f_open  Failed! ret=%d\r\n", ret);
				return -1;
			}
			xil_printf("OPEN_FILE Command Success! file name:%s\r\n",cmd_str_11);
		break;
/***************************************************************************************/
/***************************************************************************************/

		case CLOSE_FILE:    //关闭文件
			ret = f_close(&file);
			if (ret != FR_OK) {
				xil_printf("f_close  Failed! ret=%d\r\n", ret);
				return -1;
			}
		break;
/***************************************************************************************/
/***************************************************************************************/

		case COPY_FILE:   //拷贝文件 最后的参数是0表示不覆盖存在的文件，1表示覆盖存在的文件
			i=temp+2048;
			h=0;
			for (x = 0; x < 1024; x++)
			{
				  unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
				  cmd_str_2[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
				  if(cmd_str_2[x]<=0x7E)
				  {
						cmd_str_21[h++]=cmd_str_2[x];
				  }
				  else
				  {
						cmd_str_21[h++]=(cmd_str_2[x]>>8);
						cmd_str_21[h++]=cmd_str_2[x];
				  }
				  if (cmd_str_2[x] == '\0') break;
			}

			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_21);
			ret =my_fcopy (cmd_str_11,cmd_str_21,0);
			if (ret != FR_OK) {
				xil_printf("Copy File Failed! ret=%d\r\n", ret);
				return -1;
			}
		break;
/***************************************************************************************/
/***************************************************************************************/

		case COPY_FOLDER:  //拷贝文件夹 最后的参数是0表示不覆盖存在的文件，1表示覆盖存在的文件
			i=temp+2048;
			h=0;
			temp=strlen(cmd_str_11);
			for (x = 0; x < 1024; x++)
			{
				  unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
				  cmd_str_2[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
				  if(cmd_str_2[x]<=0x7E)
				  {
						cmd_str_21[h++]=cmd_str_2[x];
				  }
				  else
				  {
						cmd_str_21[h++]=(cmd_str_2[x]>>8);
						cmd_str_21[h++]=cmd_str_2[x];
				  }
				  if (cmd_str_2[x] == '\0') break;
			}
			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_21);
			ret = my_dcopy (cmd_str_11,cmd_str_21,0);
			if (ret != FR_OK) {
				xil_printf("Copy Directory Failed! ret=%d\r\n", ret);
				return -1;
			}
		break;
/***************************************************************************************/
/***************************************************************************************/

		case GET_DIR:   // 返回目录中的文件和子目录列表
			ret = cmd_reply_a208(cmd_str_11);
//			ret = cmd_reply_a208("0:");
			if (ret != FR_OK)
			{
				xil_printf("Returns Directory List Failed! ret=%d\r\n", ret);
				return -1;
			}
			cmd_reply_a203(0,0xA2,0x1,0x11);
//			cmd_reply_a204(0,0xA2,0x1,0x11);
			usleep(100000);

			wlen=write_len;
			if(write_len>MAX_LEN)   //0x3FFE
			{
				upload_time = (write_len/MAX_LEN)+1;
				wlen = MAX_LEN;
				lastpack_Size = write_len % MAX_LEN;
				if((write_len%MAX_LEN)==0)
				{
					upload_time = write_len/MAX_LEN;
					wlen = MAX_LEN;
					lastpack_Size = MAX_LEN;
				}
			}

			while(1)
			{
				if(flag_1x==1)
				{
					AxiDma.TxBdRing.HasDRE=1;
					ret = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR *)(databuf),
							wlen, XAXIDMA_DMA_TO_DEVICE);

					if (ret != XST_SUCCESS)
					{
						return XST_FAILURE;
					}
				}
				else if(flag_tcp==1)
				{
					while(FALSE!=XAxiDma_Busy(&AxiDma1, XAXIDMA_DMA_TO_DEVICE));//12.16 add by lyh
					AxiDma1.TxBdRing.HasDRE=1;
					ret = XAxiDma_SimpleTransfer(&AxiDma1,(UINTPTR *)(databuf),
							wlen, XAXIDMA_DMA_TO_DEVICE);

					if (ret != XST_SUCCESS)
					{
						xil_printf("%s %d ret:%d\r\n", __FUNCTION__, __LINE__,ret);
						return XST_FAILURE;
					}
//					xil_printf("ret=%d\r\n", ret);
				}
				COUNT++;
				databuf+=wlen;
				if(COUNT==upload_time-1)
				{
					wlen=lastpack_Size;
				}
				if(COUNT==upload_time)  break;
				//usleep(100);
				usleep(50000);				
				//usleep(10000);
			}
			xil_printf("%s %d  write_len=%d\r\n", __FUNCTION__, __LINE__,write_len);
			write_len=0;
			Reply_REMAIN_SPACE();
		break;

		default:
			break;
	}
	return 0;
}

int run_cmd_a202(StructMsg *pMsg)
{
        //解析要问讯哪种（包括A2-01、A2-04、A2-05、A2-08、D2-01、D2-01）
	    int ret=0,i=0;
        u32 ack_HandType,ack_HandID,ack_PackNum;
        ack_HandType=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
        i+=4;
        ack_HandID=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
        i+=4;
        ack_PackNum=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);

        switch(ack_HandType)
        {
           case 0xA2:
                 switch(ack_HandID)
                {
                     case  0x01:
                    	 cmd_reply_a203(ack_PackNum,ack_HandType,ack_HandID,result_a201);
//                    	 cmd_reply_a203_to_a201(ack_PackNum, ack_HandType, ack_HandID, result_a201);
                    	 xil_printf("ack_HandType:%x ack_HandID:%d result:0x%x \r\n", ack_HandType, ack_HandID,result_a201);
                     break;

                     case  0x04:
                    	 cmd_reply_a203(ack_PackNum,ack_HandType,ack_HandID,result_a204);
//                    	 cmd_reply_a203_to_a204(ack_PackNum, ack_HandType, ack_HandID, result_a204);
                    	 xil_printf("ack_HandType:%x ack_HandID:%d result:0x%x \r\n", ack_HandType, ack_HandID,result_a204);
                     break;

                     case  0x05:
                    	 cmd_reply_a203(ack_PackNum,ack_HandType,ack_HandID,result_a205);
//                       cmd_reply_a203_to_a205(ack_PackNum, ack_HandType, ack_HandID, result_a205);
                      	 xil_printf("ack_HandType:%x ack_HandID:%d result:0x%x \r\n", ack_HandType, ack_HandID,result_a205);
                     break;

//                     case  0x08:
//                    	 cmd_reply_a203_to_a208(ack_PackNum, ack_HandType, ack_HandID, result_a208);
//                    	 xil_printf("ack_HandType:%x ack_HandID:%d result:0x%x \r\n", ack_HandType, ack_HandID,result_a208);
//                     break;

                     default:
                         return 0;
                     break;
                }
           break;
           case 0xD2:
        	   switch(ack_HandID)
			   {
				   case  0x01:
//					    cmd_reply_a203_to_d201(ack_PackNum, ack_HandType, ack_HandID, result_d201);
					    cmd_reply_a203(ack_PackNum, ack_HandType, ack_HandID, result_d201);
					    xil_printf("ack_HandType:%x ack_HandID:%d result:0x%x \r\n", ack_HandType, ack_HandID,result_d201);

				   break;
				   default:
						return 0;
				   break;
			  }
           break;
           case 0xB2:
        	   switch(ack_HandID)
			  {
				   case  0x01:
//					    cmd_reply_a203_to_b201(ack_PackNum, ack_HandType, ack_HandID, result_b201);
					    cmd_reply_a203(ack_PackNum, ack_HandType, ack_HandID, result_b201);
					    xil_printf("ack_HandType:%x ack_HandID:%d result:0x%x \r\n", ack_HandType, ack_HandID,result_b201);
				   break;
				   default:
						return 0;
				   break;
			  }
           break;

           default:
        	   return 0;
           break;
        }
      return 0;
}

int cmd_reply_a203(u32 packnum, u32 type, u32 id, u32 result)
{
		int Status;
		StructA203Ack ReplyStructA203Ack;
//		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		ReplyStructA203Ack.Head = 0x55555555;
		ReplyStructA203Ack.SrcId = SRC_ID;
		ReplyStructA203Ack.DestId = DEST_ID;
#if   1
		ReplyStructA203Ack.HandType = 0xA2;
		ReplyStructA203Ack.HandId = 0x3;
		ReplyStructA203Ack.PackNum = 0;
#endif
		switch(type)
		{
			case  0xA2:
				switch(id)
				{
					case  0x01:
						if(result_a201==result)
						{
							ReplyStructA203Ack.AckResult = result_a201;
						}
						else
						{
							result_a201=result;
							ReplyStructA203Ack.AckResult = result_a201;
						}
					break;

					case  0x04:
						if(result_a204==result)
						{
							ReplyStructA203Ack.AckResult = result_a204;
						}
						else
						{
							result_a204=result;
							ReplyStructA203Ack.AckResult = result_a204;
						}
					break;

					case  0x05:
						if(result_a205==result)
						{
							ReplyStructA203Ack.AckResult = result_a205;
						}
						else
						{
							result_a205=result;
							ReplyStructA203Ack.AckResult = result_a205;
						}
					break;
					default:
					break;
				}
			break;

			case  0xB2:
				if(result_b201==result)
				{
					ReplyStructA203Ack.AckResult = result_b201;
				}
				else
				{
					result_b201=result;
					ReplyStructA203Ack.AckResult = result_b201;
				}
			break;

			case  0xD2:
				switch(id)
				{
					case  0x01:
						if(result_d201==result)
						{
							ReplyStructA203Ack.AckResult = result_d201;
						}
						else
						{
							result_d201=result;
							ReplyStructA203Ack.AckResult = result_d201;
						}
					break;

					case  0x05:
						if(result_d205==result)
						{
							ReplyStructA203Ack.AckResult = result_d205;
						}
						else
						{
							result_d205=result;
							ReplyStructA203Ack.AckResult = result_d205;
						}
					break;

					case  0x0A:
						if(result_d20A==result)
						{
							ReplyStructA203Ack.AckResult = result_d20A;
						}
						else
						{
							result_d20A=result;
							ReplyStructA203Ack.AckResult = result_d20A;
						}
					break;

					default:
					 break;
				}
			break;

			case  0xF2:
				if(result_f201==result)
				{
					ReplyStructA203Ack.AckResult = result_f201;
				}
				else
				{
					result_f201=result;
					ReplyStructA203Ack.AckResult = result_f201;
				}
			break;

			default:
			break;
		}
		ReplyStructA203Ack.AckPackNum = packnum;
		ReplyStructA203Ack.AckHandType = type;
		ReplyStructA203Ack.AckHandId = id;

		for(int i=0;i<4;i++)
		{
			ReplyStructA203Ack.backups[i]=0x0;
		}

		ReplyStructA203Ack.CheckCode = ReplyStructA203Ack.AckPackNum + \
				ReplyStructA203Ack.AckHandType +ReplyStructA203Ack.AckHandId + \
				ReplyStructA203Ack.AckResult;
		ReplyStructA203Ack.Tail = 0xAAAAAAAA;

#if 1    // 改变字节序
		ReplyStructA203Ack.HandType = SW32(0xA2);
		ReplyStructA203Ack.HandId =  SW32(0x03);
		ReplyStructA203Ack.PackNum = SW32(0x0);   //need change
		ReplyStructA203Ack.AckPackNum = SW32(ReplyStructA203Ack.AckPackNum);
		ReplyStructA203Ack.AckHandType = SW32(ReplyStructA203Ack.AckHandType);
		ReplyStructA203Ack.AckHandId = SW32(ReplyStructA203Ack.AckHandId);
		ReplyStructA203Ack.CheckCode = SW32(ReplyStructA203Ack.CheckCode);
		ReplyStructA203Ack.AckResult = SW32(ReplyStructA203Ack.AckResult);
#endif
		xil_printf("%s %d  sizeof(StructA203Ack)=%d\r\n", __FUNCTION__, __LINE__,sizeof(StructA203Ack));

		if(flag_1x==1)
		{
			AxiDma.TxBdRing.HasDRE=1;
			Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR)&ReplyStructA203Ack,
					sizeof(StructA203Ack), XAXIDMA_DMA_TO_DEVICE);

			if (Status != XST_SUCCESS)
			{
				return XST_FAILURE;
			}
//			flag_1x=0;
		}
		else if(flag_tcp==1)
		{
			AxiDma1.TxBdRing.HasDRE=1;
			Status = XAxiDma_SimpleTransfer(&AxiDma1,(UINTPTR)&ReplyStructA203Ack,
					sizeof(StructA203Ack), XAXIDMA_DMA_TO_DEVICE);

			if (Status != XST_SUCCESS)
			{
				return XST_FAILURE;
			}
//			flag_tcp=0;
		}
		xil_printf("%s %d result:0x%x\r\n", __FUNCTION__, __LINE__,result);

		return 0;
}

int cmd_reply_a204(u32 packnum, u32 type, u32 id, u32 result)
{
		int Status;
		StructA203Ack ReplyStructA203Ack;
//		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		ReplyStructA203Ack.Head = 0x55555555;
		ReplyStructA203Ack.SrcId = SRC_ID;
		ReplyStructA203Ack.DestId = DEST_ID;
#if   1
		ReplyStructA203Ack.HandType = 0xA2;
		ReplyStructA203Ack.HandId = 0x4;
		ReplyStructA203Ack.PackNum = 0;
#endif
		switch(type)
		{
			case  0xA2:
				switch(id)
				{
					case  0x01:
						if(result_a201==result)
						{
							ReplyStructA203Ack.AckResult = result_a201;
						}
						else
						{
							result_a201=result;
							ReplyStructA203Ack.AckResult = result_a201;
						}
					break;

					case  0x04:
						if(result_a204==result)
						{
							ReplyStructA203Ack.AckResult = result_a204;
						}
						else
						{
							result_a204=result;
							ReplyStructA203Ack.AckResult = result_a204;
						}
					break;

					case  0x05:
						if(result_a205==result)
						{
							ReplyStructA203Ack.AckResult = result_a205;
						}
						else
						{
							result_a205=result;
							ReplyStructA203Ack.AckResult = result_a205;
						}
					break;
					default:
					break;
				}
			break;

			case  0xB2:
				if(result_b201==result)
				{
					ReplyStructA203Ack.AckResult = result_b201;
				}
				else
				{
					result_b201=result;
					ReplyStructA203Ack.AckResult = result_b201;
				}
			break;

			case  0xD2:
				switch(id)
				{
					case  0x01:
						if(result_d201==result)
						{
							ReplyStructA203Ack.AckResult = result_d201;
						}
						else
						{
							result_d201=result;
							ReplyStructA203Ack.AckResult = result_d201;
						}
					break;

					case  0x05:
						if(result_d205==result)
						{
							ReplyStructA203Ack.AckResult = result_d205;
						}
						else
						{
							result_d205=result;
							ReplyStructA203Ack.AckResult = result_d205;
						}
					break;

					case  0x0A:
						if(result_d20A==result)
						{
							ReplyStructA203Ack.AckResult = result_d20A;
						}
						else
						{
							result_d20A=result;
							ReplyStructA203Ack.AckResult = result_d20A;
						}
					break;

					default:
					 break;
				}
			break;

			case  0xF2:
				if(result_f201==result)
				{
					ReplyStructA203Ack.AckResult = result_f201;
				}
				else
				{
					result_f201=result;
					ReplyStructA203Ack.AckResult = result_f201;
				}
			break;

			default:
			break;
		}
		ReplyStructA203Ack.AckPackNum = packnum;
		ReplyStructA203Ack.AckHandType = type;
		ReplyStructA203Ack.AckHandId = id;

		for(int i=0;i<4;i++)
		{
			ReplyStructA203Ack.backups[i]=0x0;
		}

		ReplyStructA203Ack.CheckCode = ReplyStructA203Ack.AckPackNum + \
				ReplyStructA203Ack.AckHandType +ReplyStructA203Ack.AckHandId + \
				ReplyStructA203Ack.AckResult;
		ReplyStructA203Ack.Tail = 0xAAAAAAAA;

#if 1    // 改变字节序
		ReplyStructA203Ack.HandType = SW32(0xA2);
		ReplyStructA203Ack.HandId =  SW32(0x03);
		ReplyStructA203Ack.PackNum = SW32(0x0);   //need change
		ReplyStructA203Ack.AckPackNum = SW32(ReplyStructA203Ack.AckPackNum);
		ReplyStructA203Ack.AckHandType = SW32(ReplyStructA203Ack.AckHandType);
		ReplyStructA203Ack.AckHandId = SW32(ReplyStructA203Ack.AckHandId);
		ReplyStructA203Ack.CheckCode = SW32(ReplyStructA203Ack.CheckCode);
		ReplyStructA203Ack.AckResult = SW32(ReplyStructA203Ack.AckResult);
#endif
		xil_printf("%s %d  sizeof(StructA203Ack)=%d\r\n", __FUNCTION__, __LINE__,sizeof(StructA203Ack));

		if(flag_1x==1)
		{
			AxiDma.TxBdRing.HasDRE=1;
			Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR)&ReplyStructA203Ack,
					sizeof(StructA203Ack), XAXIDMA_DMA_TO_DEVICE);

			if (Status != XST_SUCCESS)
			{
				return XST_FAILURE;
			}
//			flag_1x=0;
		}
		else if(flag_tcp==1)
		{
			AxiDma1.TxBdRing.HasDRE=1;
			Status = XAxiDma_SimpleTransfer(&AxiDma1,(UINTPTR)&ReplyStructA203Ack,
					sizeof(StructA203Ack), XAXIDMA_DMA_TO_DEVICE);

			if (Status != XST_SUCCESS)
			{
				return XST_FAILURE;
			}
//			flag_tcp=0;
		}
		xil_printf("%s %d result:0x%x\r\n", __FUNCTION__, __LINE__,result);

		return 0;
}

int cmd_reply_AcqusionRet(u32 ID,u32 result,u32 mode)
{
		int Status;
		StructAcqusionRet ReplyRetStruct;
//		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		ReplyRetStruct.Head = 0x55555555;
		ReplyRetStruct.SrcId = SRC_ID;
		ReplyRetStruct.DestId = DEST_ID;
		ReplyRetStruct.HandType = SW32(0xC2);
		ReplyRetStruct.HandId = SW32(ID);
		ReplyRetStruct.PackNum = 0;
		ReplyRetStruct.AckStateRet = SW32(result);
		ReplyRetStruct.mode = SW32(mode);
		for(int i=0;i<6;i++)
		{
			ReplyRetStruct.backups[i] = 0x0;
		}
		ReplyRetStruct.CheckCode = 0x0;
		ReplyRetStruct.Tail = 0xAAAAAAAA;

		if(flag_1x==1)
		{
			AxiDma.TxBdRing.HasDRE=1;
			Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR)&ReplyRetStruct,
					sizeof(StructAcqusionRet), XAXIDMA_DMA_TO_DEVICE);

			if (Status != XST_SUCCESS)
			{
				return XST_FAILURE;
			}
//			flag_1x=0;
		}
		else if(flag_tcp==1)
		{
			AxiDma1.TxBdRing.HasDRE=1;
			Status = XAxiDma_SimpleTransfer(&AxiDma1,(UINTPTR)&ReplyRetStruct,
					sizeof(StructAcqusionRet), XAXIDMA_DMA_TO_DEVICE);

			if (Status != XST_SUCCESS)
			{
				return XST_FAILURE;
			}
//			flag_tcp=0;
		}
		return XST_SUCCESS;
}
//int run_cmd_a204(StructMsg *pMsg)   //增加了协议内容
//{
//	    int ret=0,i=0,x=0,temp=0,h=0;
//		u16 unicode_u16=0;
//		int flag=0;
//		u32 file_cmd=0,file_cmd2=0;
//		WCHAR cmd_str_1[1024]={0},cmd_str_2[512]={0};
//		BYTE cmd_str_11[100]={0},cmd_str_21[100]={0};
//
//		WCHAR cmd_str_3[1024]={0},cmd_str_4[512]={0};
//		BYTE cmd_str_31[100]={0},cmd_str_41[100]={0};
//		int x0=0,x1=0,x2=0,x3=0,x4=0,x5=0,x6=0,x7=0;
//		DIR dir;
//		FIL file;
//		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
//		//属性修改使能指令file_cmd
//		file_cmd = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
//		i=i+4;
//		temp=i;
//		xil_printf("%s %d  file_cmd:0x%x\r\n", __FUNCTION__, __LINE__,file_cmd);
//
//		// 解析操作指令
//		x0=((file_cmd>>0)&0x1==1)?1:0;   //x0=1修改文件属性,x0=0不修改文件属性
//		x1=((file_cmd>>4)&0x1==1)?1:0;   //x1=1修改文件读写控制,x1=0不修改文件读写控制
//		x2=((file_cmd>>8)&0x1==1)?1:0;   //x2=1修改文件显示控制,x2=0不修改文件显示控制
//		x3=((file_cmd>>12)&0x1==1)?1:0;  //x3=1对文件进行操作,x3=0不对文件进行操作
//
//		x4=((file_cmd>>16)&0x1==1)?1:0;  //x4=1修改文件夹属性,x4=0不修改文件夹属性
//		x5=((file_cmd>>20)&0x1==1)?1:0;  //x5=1修改文件夹读写控制,x5=0不修改文件夹读写控制
//		x6=((file_cmd>>24)&0x1==1)?1:0;  //x6=1修改文件夹显示控制,x6=0不修改文件夹显示控制
//		x7=((file_cmd>>28)&0x1==1)?1:0;  //x7=1对文件夹进行操作,x7=0不对文件夹进行操作
//
///////////////////////////////////////////////////////////////////////////
//		if(x3==1)
//		{
//			for (x = 0; x < 1024; x++)
//			{
//				unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
//				cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
//				if(cmd_str_1[x]<=0x7E)
//				{
//					cmd_str_11[h++]=cmd_str_1[x];
//				}
//				else
//				{
//					cmd_str_11[h++]=(cmd_str_1[x]>>8);
//					cmd_str_11[h++]=cmd_str_1[x];
//				}
//				if (cmd_str_1[x] == '\0')	break;
//			}
//			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_11);
//			// *****************   判断是否更改文件属性        *******************//
//			if(x0==1)
//			{
//				flag=1;
////				i=temp+strlen(cmd_char_str1)*2+2;
////				i=temp+(strlen(cmd_str_11)+1)*2;	//1.15 改 by lyh
//				i=temp+2048;
//				temp=i;
//				h=0;
//				for (x = 0; x < 512; x++)
//				{
//				   unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
//				   cmd_str_2[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
//				   if(cmd_str_2[x]<=0x7E)
//			       {
//						cmd_str_21[h++]=cmd_str_2[x];
//				   }
//				   else
//				   {
//						cmd_str_21[h++]=(cmd_str_2[x]>>8);
//						cmd_str_21[h++]=cmd_str_2[x];
//				   }
//				   if (cmd_str_2[x] == '\0')	break;
//				}
//
//				xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_21); // 新文件名称
//
//				ret=f_rename(cmd_str_11,cmd_str_21);
//				if (ret != FR_OK) {
//					xil_printf("Rename File Failed! ret=%d\r\n", ret);
//					return -1;
//				}
//			}
//			// *****************                   *******************//
//			if(flag==0)
////				i=temp+(strlen(cmd_str_11)+1)*2;
//				i=temp+2048+1024;
//			else if(flag==1)
////				i=temp+(strlen(cmd_str_21)+1)*2;
//				i=temp+1024;
//			flag=0;
//			file_cmd2 = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]); //文件新读写控制值
//			if(x1==1)
//			{
//				if(file_cmd2==1)
//				{
//						if(x0==1)    //  用新文件名
//						{
//							ret=f_chmod(cmd_str_21,AM_RDO,AM_RDO );        //  文件设成只读
//							if (ret != FR_OK) {
//								xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//								return -1;
//							}
//						}
//						else         //  用旧文件名
//						{
//							ret=f_chmod(cmd_str_11,AM_RDO,AM_RDO );         //  文件设成只读
//							if (ret != FR_OK) {
//								xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//								return -1;
//							}
//						}
//				}
//				else if(file_cmd2==0)
//				{
//						if(x0==1)    //  用新文件名
//						{
//							ret=f_chmod(cmd_str_21,0, AM_RDO); 		 //  文件取消只读
//							if (ret != FR_OK) {
//								xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//								return -1;
//							}
//						}
//						else         //  用旧文件名
//						{
//							ret=f_chmod(cmd_str_11,0,AM_RDO);         //  文件取消只读
//							if (ret != FR_OK) {
//								xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//								return -1;
//							}
//						}
//				}
//			}
//			i+=4;
//			file_cmd2 = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);//文件新显示控制值
//			if(x2==1)
//			{
//				if(file_cmd2==1)
//				{
//						if(x0==1)    //  用新文件名
//						{
//							ret=f_chmod(cmd_str_21,AM_HID,AM_HID );
//							if (ret != FR_OK) {
//								xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//								return -1;
//							}
//						}
//						else         //  用旧文件名
//						{
//							ret=f_chmod(cmd_str_11,AM_HID,AM_HID);         //  文件设成隐藏
//							if (ret != FR_OK) {
//								xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//								return -1;
//							}
//						}
//				}
//				else if(file_cmd2==0)
//				{
//						if(x0==1)    //  用新文件名
//						{
//							ret=f_chmod(cmd_str_21,0,AM_HID);
//							if (ret != FR_OK) {
//								xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//								return -1;
//							}
//						}
//						else         //  用旧文件名
//						{
//							ret=f_chmod(cmd_str_11,0,AM_HID);         //  文件取消隐藏
//							if (ret != FR_OK) {
//								xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//								return -1;
//							}
//						}
//				}
//			}
//		i+=4;
//		temp=i;
//		}//if(x3==1)
//
//        /*********************************************/
//		if(x7==1)
//		{
//			if(x3==0)  i=temp+3080;			// 2048+1024+4+4
//			h=0;
//			for (x = 0; x < 1024; x++)
//			{
//				unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
//				cmd_str_3[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
//				if(cmd_str_3[x]<=0x7E)
//			    {
//					cmd_str_31[h++]=cmd_str_3[x];
//			    }
//			    else
//			    {
//					cmd_str_31[h++]=(cmd_str_3[x]>>8);
//					cmd_str_31[h++]=cmd_str_3[x];
//			    }
//				if ((cmd_str_3[x] == '\0')||(cmd_str_3[x] == ' '))
//				{
//					if(cmd_str_3[x] == ' ')
//					{
//						cmd_str_31[x]='\0';
//					}
//					break;
//				}
//			}
//			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_31);//  原文件夹名称
//
//			// *****************   判断是否更改文件夹属性        *******************//
//			if(x4==1)
//			{
//				flag=1;
//				i=temp+(strlen(cmd_str_31)+1)*2;
//				temp=i;
//				for (x = 0; x < 512; x++)
//				{
//					unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
//					cmd_str_4[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
//					if(cmd_str_4[x]<=0x7E)
//					{
//						cmd_str_41[h++]=cmd_str_4[x];
//					}
//					else
//					{
//						cmd_str_41[h++]=(cmd_str_4[x]>>8);
//						cmd_str_41[h++]=cmd_str_4[x];
//					}
//					if ((cmd_str_4[x] == '\0')||(cmd_str_4[x] == ' '))
//					{
//						if(cmd_str_4[x] == ' ')
//						{
//							cmd_str_41[x]='\0';
//						}
//						break;
//					}
//				}
//				xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_41); //  新文件夹名称
//				ret=f_rename(cmd_str_31,cmd_str_41);
//				if (ret != FR_OK) {
//					xil_printf("Rename File Failed! ret=%d\r\n", ret);
//					return -1;
//				}
//			}
//			// *****************                    *******************//
//			if(flag==0)
////				i=temp+strlen(cmd_str_31)*2;
//				i=temp+2048+1024;
//			else if(flag==1)
////				i=temp+strlen(cmd_str_41)*2;
//				i=temp+1024;
//			file_cmd2 = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);//文件夹新读写控制值
//			if(x5==1)
//			{
//				if(file_cmd2==1)
//				{
//						if(x4==1)           //  用新文件夹名
//						{
//							ret=f_chmod(cmd_str_41,AM_RDO,AM_RDO );
//							if (ret != FR_OK) {
//								  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//								  return -1;
//							}
//						}
//						else                //  用旧文件夹名
//						{
//							ret=f_chmod(cmd_str_31,AM_RDO,AM_RDO );	    //  文件夹设成只读
//							if (ret != FR_OK) {
//								  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//								  return -1;
//							}
//						}
//				}
//				else if(file_cmd2==0)
//				{
//						if(x4==1)           //  用新文件夹名
//						{
//							ret=f_chmod(cmd_str_41,0, AM_RDO);
//							if (ret != FR_OK) {
//								  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//								  return -1;
//							}
//						}
//						else                //  用旧文件夹名
//						{
//							ret=f_chmod(cmd_str_31,0, AM_RDO);	    //  文件夹取消只读
//							if (ret != FR_OK) {
//								  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//								  return -1;
//							}
//						}
//				}
//			}
//			i+=4;
//			file_cmd2 = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);//文件夹新显示控制值
//			if(x6==1)
//			{
//				if(file_cmd2==1)
//				{
//							if(x4==1)           //  用新文件夹名
//							{
//								  ret=f_chmod(cmd_str_41,AM_HID,AM_HID );
//								  if (ret != FR_OK) {
//									  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//									  return -1;
//								  }
//							}
//							else                //  用旧文件夹名
//							{
//								  ret=f_chmod(cmd_str_31,AM_HID,AM_HID );     //  文件夹设成隐藏
//								  if (ret != FR_OK) {
//									  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//									  return -1;
//								  }
//							}
//				}
//				else if(file_cmd2==0)
//				{
//							if(x4==1)           //  用新文件夹名
//							{
//								  ret=f_chmod(cmd_str_41,0, AM_HID);
//								  if (ret != FR_OK) {
//									  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//									  return -1;
//								  }
//							}
//							else                //  用旧文件夹名
//							{
//								  ret=f_chmod(cmd_str_31,0, AM_HID);     //  文件夹取消隐藏
//								  if (ret != FR_OK) {
//									  xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
//									  return -1;
//								  }
//							}
//				}
//
//			}
//		}//if(x7==1)
//		return 0;
////		cmd_reply_a203(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
////        cmd_reply_a203_to_a204(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x11);
//}

int run_cmd_a204(StructMsg *pMsg)   //增加了协议内容
{
	    int ret=0,i=0,x=0,temp=0,h=0;
		u16 unicode_u16=0;
		int flag=0;
		u32 file_cmd=0,file_cmd2=0;
		WCHAR cmd_str_1[1024]={0};
		BYTE cmd_str_11[100]={0};

		WCHAR cmd_str_3[1024]={0};
		BYTE cmd_str_31[100]={0};
		int x0=0,x1=0,x2=0,x3=0,x4=0,x5=0,x6=0,x7=0;
		DIR dir;
		FIL file;
		xil_printf("%s %d\r\n", __FUNCTION__, __LINE__);
		//属性修改使能指令file_cmd
		file_cmd = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
		i=i+4;
		temp=i;
		xil_printf("%s %d  file_cmd:0x%x\r\n", __FUNCTION__, __LINE__,file_cmd);

		// 解析操作指令
		x0=((file_cmd>>0)&0x1==1)?1:0;   //x0=1修改文件属性,x0=0不修改文件属性
		x1=((file_cmd>>4)&0x1==1)?1:0;   //x1=1修改文件读写控制,x1=0不修改文件读写控制
		x2=((file_cmd>>8)&0x1==1)?1:0;   //x2=1修改文件显示控制,x2=0不修改文件显示控制
		// x3=((file_cmd>>12)&0x1==1)?1:0;  //x3=1对文件进行操作,x3=0不对文件进行操作

		x4=((file_cmd>>16)&0x1==1)?1:0;  //x4=1修改文件夹属性,x4=0不修改文件夹属性
		x5=((file_cmd>>20)&0x1==1)?1:0;  //x5=1修改文件夹读写控制,x5=0不修改文件夹读写控制
		x6=((file_cmd>>24)&0x1==1)?1:0;  //x6=1修改文件夹显示控制,x6=0不修改文件夹显示控制
		// x7=((file_cmd>>28)&0x1==1)?1:0;  //x7=1对文件夹进行操作,x7=0不对文件夹进行操作

/////////////////////////////////////////////////////////////////////////
		if(x0==1)
		{
			for (x = 0; x < 1024; x++)
			{
				unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
				cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
				if(cmd_str_1[x]<=0x7E)
				{
					cmd_str_11[h++]=cmd_str_1[x];
				}
				else
				{
					cmd_str_11[h++]=(cmd_str_1[x]>>8);
					cmd_str_11[h++]=cmd_str_1[x];
				}
				if (cmd_str_1[x] == '\0')	break;
			}
			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_11);
			i=temp+2048;


			if(x1==1)  // *****************     判断是否更改文件读写属性        *******************//
			{
				file_cmd2 = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]); //文件新读写控制值
				if (file_cmd2==0x1)
				{
					ret=f_chmod(cmd_str_11,AM_RDO,AM_RDO );         //  文件设成只读
					if (ret != FR_OK) {
						xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
						return -1;
					}
				}
				else if(file_cmd2==0x0)
				{
					ret=f_chmod(cmd_str_11,0, AM_RDO); 		 		//  文件取消只读
					if (ret != FR_OK) {
						xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
						return -1;
					}
				}
			}
            i=i+4;
			if(x2==1) // *****************     判断是否更改文件显示属性        *******************//
			{
				file_cmd2 = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]); //文件新显示控制值
				if (file_cmd2==0x1)
				{
					ret=f_chmod(cmd_str_11,AM_HID,AM_HID);         //  文件设成隐藏
                    if (ret != FR_OK) {
                        xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
                        return -1;
                    }
				}
                else if(file_cmd2==0x0)
                {
                    ret=f_chmod(cmd_str_11,0,AM_HID);           //  文件取消隐藏
                    if (ret != FR_OK) {
                        xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
                        return -1;
                    }

                }
			}
        }


        /*********************************************/
		if(x4==1)
		{
			if(x0==0)  i=temp+2056;			// 2048+4+4
			temp=i;
			h=0;
			for (x = 0; x < 1024; x++)
			{
				unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
				cmd_str_3[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
				if(cmd_str_3[x]<=0x7E)
			    {
					cmd_str_31[h++]=cmd_str_3[x];
			    }
			    else
			    {
					cmd_str_31[h++]=(cmd_str_3[x]>>8);
					cmd_str_31[h++]=cmd_str_3[x];
			    }
				if ((cmd_str_3[x] == '\0')||(cmd_str_3[x] == ' '))
				{
					if(cmd_str_3[x] == ' ')
					{
						cmd_str_31[x]='\0';
					}
					break;
				}
			}
			xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_31);//  文件夹名称
			i=temp+2048;
			if(x5==1)
			{
				file_cmd2 = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);//文件夹新读写控制值
				if(file_cmd2==0x1)
				{
					ret=f_chmod(cmd_str_31,AM_RDO,AM_RDO );	    //  文件夹设成只读
					if (ret != FR_OK)
					{
						xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
						return -1;
					}
				}
				else if(file_cmd2==0x0)
				{
					ret=f_chmod(cmd_str_31,0, AM_RDO);	    //  文件夹取消只读
					if (ret != FR_OK)
					{
						xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
						return -1;
					}
				}
			}

			i+=4;
			if(x6==1) // *****************     判断是否更改文件夹显示属性        *******************//
			{
				file_cmd2 = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);//文件夹新显示控制值
				if(file_cmd2==0x1)
				{
					ret=f_chmod(cmd_str_31,AM_HID,AM_HID );     //  文件夹设成隐藏
					if (ret != FR_OK) {
						xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
						return -1;
					}
				}
				else if(file_cmd2==0x0)
				{
					ret=f_chmod(cmd_str_31,0, AM_HID);     //  文件夹取消隐藏
					if (ret != FR_OK) {
						xil_printf("Change Attribute Failed! ret=%d\r\n", ret);
						return -1;
					}
				}
			}
		}
		return 0;
}

//文件及文件夹属性获取   主控组件->存储组件
int run_cmd_a205(StructMsg *pMsg)
{
		 int file_cmd=0,ret=0,i=0,x = 0,temp=0,h=0;
		 u16 unicode_u16=0;
		 int res;
		 WCHAR cmd_str_1[1024]={0},cmd_str_2[1024]={0};
		 BYTE cmd_str_11[100]={0},cmd_str_21[100]={0};
//		 DIR dir;
//		 FIL file;
		 file_cmd = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
		 i=i+4;
		 temp=i;
		 switch(file_cmd)
		 {
			case FILE_ATTRIBUTE:
				for (x = 0; x < 1024; x++)
				{
					 unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
					 cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
					 if(cmd_str_1[x]<=0x7E)
					 {
						 cmd_str_11[h++]=cmd_str_1[x];
					 }
					 else
					 {
						 cmd_str_11[h++]=(cmd_str_1[x]>>8);
						 cmd_str_11[h++]=cmd_str_1[x];
					 }
					 if (cmd_str_1[x] == '\0') break;
				}
				xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_11);
				cmd_reply_a203(0,0xA2,0x5,0x11);
				res=cmd_reply_a206(pMsg, cmd_str_11);
//				res=cmd_reply_a206(pMsg, "abc");
				if(res!=0)
				{
					xil_printf("Failed to get file attributes! res=%d\r\n",res);
//					cmd_reply_a203(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x10);
					return -1;
				}
				break;

			case FOLDER_ATTRIBUTE:
				i=temp+2048;
				for (x = 0; x < 1024; x++)
				{
					unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
					cmd_str_2[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
					if(cmd_str_2[x]<=0x7E)
				    {
					    cmd_str_21[h++]=cmd_str_2[x];
				    }
				    else
				    {
					    cmd_str_21[h++]=(cmd_str_2[x]>>8);
					    cmd_str_21[h++]=cmd_str_2[x];
				    }
					if (cmd_str_2[x] == '\0') break;
				}
				xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_21);
				cmd_reply_a203(0,0xA2,0x5,0x11);
				res=cmd_reply_a207(pMsg, cmd_str_21);
//				res=cmd_reply_a207(pMsg, "bcd");
				if(res!=0)
				{
					xil_printf("Failed to get folder properties! res=%d\r\n",res);
//					cmd_reply_a203(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x10);
					return -1;
				}
				break;
			default:       // 操作失败通过a203进行回复
				xil_printf("command failed!\r\n");
//				cmd_reply_a203_to_a205(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x10);
//				cmd_reply_a203(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x10);
				return -1;
				break;
		 }
//		 cmd_reply_a203(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x11);
    return 0;
}

// 文件属性获取应答  存储组件—>主控组件
int cmd_reply_a206(StructMsg *pMsg, const BYTE* path)
{
	   int Status;
	   FILINFO fno;
	   FRESULT fr;
	   u8 CreateTime1[48]={0};
	   u8 CreateTime2[48]={0};
	   u8 ChangeTime1[48]={0};
	   u8 ChangeTime2[48]={0};
	   u8 AccessTime1[48]={0};
	   u8 AccessTime2[48]={0};
	   StructA206Ack ReplyStructA206Ack={0};
	   fr = f_stat(path, &fno);
	   switch(fr)
	   {
	   	   case FR_OK:
		   break;
		   case FR_NO_FILE:
				  xil_printf("\"%s\" is not exist.\r\n", path);
				  return -1;
		   break;
           default:
				  xil_printf("An error occured. (%d)\r\n", fr);
				  return -1;
	    }
		ReplyStructA206Ack.Head = 0x55555555;
		ReplyStructA206Ack.SrcId = SRC_ID;
		ReplyStructA206Ack.DestId = DEST_ID;
#if  0
		ReplyStructA206Ack.HandType = 0xA2;
		ReplyStructA206Ack.HandId = 0x6;
		ReplyStructA206Ack.PackNum=0x0;
#endif

//		sprintf(ReplyStructA206Ack.Name,"%s",(char *)fno.fname);
		strcpy(ReplyStructA206Ack.Name,(char *)fno.fname);  //noted by lyh on the 1.22
//		convert(ReplyStructA206Ack.Name,fno.fname);				  //add   by lyh on the 1.22
		Convert_GB2312_to_UTF16LE(ReplyStructA206Ack.Name);       //add   by lyh on the 2.02
		Reverse_u16(ReplyStructA206Ack.Name);

		get_path_dname(path,ReplyStructA206Ack.Dir);
		Convert_GB2312_to_UTF16LE(ReplyStructA206Ack.Dir);        //add   by lyh on the 2.02
		Reverse_u16(ReplyStructA206Ack.Dir);

		ReplyStructA206Ack.Size = fno.fsize;
#if  1   //改变字节顺序
		ReplyStructA206Ack.HandType = SW32(0xA2);
		ReplyStructA206Ack.HandId = SW32(0x6);
		ReplyStructA206Ack.PackNum=SW32(0x0);
//		ReplyStructA206Ack.Size = SW64(ReplyStructA206Ack.Size);
		ReplyStructA206Ack.Size=Reverse_u64(ReplyStructA206Ack.Size);

#endif
//		sprintf(ReplyStructA206Ack.CreateTime2,"%u年.%02u月.%02u日,%02u时.%02u分.%02u秒",
//				(fno.fdate >> 9) + 1980, fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
//				fno.ftime >> 5 & 63,fno.ftime*2);  //创建时间   // 11.21 tested by lyh: have some problems
//
//		sprintf(ReplyStructA206Ack.ChangeTime2,"%u年.%02u月.%02u日,%02u时.%02u分.%02u秒",
//				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
//				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems
//
//		sprintf(ReplyStructA206Ack.AccessTime2,"%u年.%02u月.%02u日,%02u时.%02u分.%02u秒",
//				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
//				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems

		sprintf(CreateTime1,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980, fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //创建时间
		convert(ReplyStructA206Ack.CreateTime1,CreateTime1);

		sprintf(CreateTime2,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980, fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //创建时间
		convert(ReplyStructA206Ack.CreateTime2,CreateTime2);

		sprintf(ChangeTime1,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems
		convert(ReplyStructA206Ack.ChangeTime1,ChangeTime1);

		sprintf(ChangeTime2,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems
		convert(ReplyStructA206Ack.ChangeTime2,ChangeTime2);

		sprintf(AccessTime1,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems
		convert(ReplyStructA206Ack.AccessTime1,AccessTime1);

		sprintf(AccessTime2,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems
		convert(ReplyStructA206Ack.AccessTime2,AccessTime2);

		if((fno.fattrib & AM_RDO) == 0)
		{
				ReplyStructA206Ack.RWCtrl = 0x0;
		}
		else
		{
				ReplyStructA206Ack.RWCtrl = 0x1;
		}

		if((fno.fattrib & AM_HID) == 0)
		{
				ReplyStructA206Ack.DisplayCtrl = 0x0;
		}
		else
		{
				ReplyStructA206Ack.DisplayCtrl = 0x1;
		}
		/****** CRC *****/
	//	ReplyStructA206Ack.CheckCode = ReplyStructA206Ack.AckPackNum + \
	//			ReplyStructA206Ack.AckHandType +ReplyStructA206Ack.AckHandId + \
	//			ReplyStructA206Ack.AckResult;
		ReplyStructA206Ack.Tail = 0xAAAAAAAA;
		// 改变字节序
#if 1
		ReplyStructA206Ack.RWCtrl=SW32(ReplyStructA206Ack.RWCtrl);
		ReplyStructA206Ack.DisplayCtrl=SW32(ReplyStructA206Ack.DisplayCtrl);
#endif

		if(flag_1x==1)
		{
			AxiDma.TxBdRing.HasDRE=1;
			Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR)&ReplyStructA206Ack,
					sizeof(StructA206Ack), XAXIDMA_DMA_TO_DEVICE);

			if (Status != XST_SUCCESS)
			{
				xil_printf("SimpleTransfer failed!\r\n");
				return XST_FAILURE;
			}
			flag_1x=0;
		}
		else if(flag_tcp==1)
		{
			AxiDma1.TxBdRing.HasDRE=1;
			Status = XAxiDma_SimpleTransfer(&AxiDma1,(UINTPTR)&ReplyStructA206Ack,
					sizeof(StructA206Ack), XAXIDMA_DMA_TO_DEVICE);

			if (Status != XST_SUCCESS)
			{
				xil_printf("SimpleTransfer failed!\r\n");
				return XST_FAILURE;
			}
			flag_tcp=0;
		}
		xil_printf("%s %d  sizeof(StructA206Ack)=%d\r\n", __FUNCTION__, __LINE__,sizeof(StructA206Ack));
		return 0;
}

// 文件夹属性获取应答  存储组件——>主控组件
int cmd_reply_a207(StructMsg *pMsg, const BYTE* path)
{
		int Status;
		FRESULT fr;
		FILINFO fno;
		int temp1, temp2;
		int nfile=0,ndir=0;
		u8 CreateTime1[48]={0};
		u8 CreateTime2[48]={0};
		u8 ChangeTime1[48]={0};
		u8 ChangeTime2[48]={0};
		u8 AccessTime1[48]={0};
		u8 AccessTime2[48]={0};
		StructA207Ack ReplyStructA207Ack={0};
		fr = f_stat(path, &fno);
		Num_of_Dir_and_File (path,&nfile,&ndir,0);
		switch (fr)
		{
			case FR_OK:
				break;

			case FR_NO_PATH:
				xil_printf("\"%s\" is not exist.\r\n", path);
				return -1;

			default:
				xil_printf("An error occured. (%d)\r\n", fr);
				return -1;
		}
		ReplyStructA207Ack.Head = 0x55555555;
		ReplyStructA207Ack.SrcId = SRC_ID;
		ReplyStructA207Ack.DestId = DEST_ID;
#if  0
		ReplyStructA207Ack.HandType = 0xA2;
		ReplyStructA207Ack.HandId = 0x7;     // lyh 203.8.11改
#endif
#if  1   //改变字节顺序
		ReplyStructA207Ack.HandType = SW32(0xA2);
		ReplyStructA207Ack.HandId = SW32(0x7);
		ReplyStructA207Ack.PackNum=SW32(0x0);
#endif

		strcpy((char *)ReplyStructA207Ack.Name, (char *)fno.fname);
		get_path_dname(path,ReplyStructA207Ack.Dir);
//		ReplyStructA207Ack.Size =fno.fsize;
		get_Dir_size(ReplyStructA207Ack.Name,&ReplyStructA207Ack.Size);

//		convert(ReplyStructA207Ack.Name,fno.fname);     //add   by lyh on the 1.22
//		reverse_u16(ReplyStructA207Ack.Name,a);
//		ConvertReverse(ReplyStructA207Ack.Name);
		Convert_GB2312_to_UTF16LE(ReplyStructA207Ack.Name);
		Reverse_u16(ReplyStructA207Ack.Name);
		Convert_GB2312_to_UTF16LE(ReplyStructA207Ack.Dir);
		Reverse_u16(ReplyStructA207Ack.Dir);
		ReplyStructA207Ack.SubFolderNum =ndir;
		ReplyStructA207Ack.SubFileNum =nfile;
#if 1
//		ReplyStructA207Ack.Size=SW64(ReplyStructA207Ack.Size);
		ReplyStructA207Ack.Size=Reverse_u64(ReplyStructA207Ack.Size);
		ReplyStructA207Ack.SubFolderNum =SW32(ReplyStructA207Ack.SubFolderNum);
		ReplyStructA207Ack.SubFileNum=SW32(ReplyStructA207Ack.SubFileNum);
#endif
//		sprintf(ReplyStructA207Ack.CreateTime2,"%u年.%02u月.%02u日,%02u时.%02u分.%02u秒",
//				(fno.fdate >> 9) + 1980, fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
//				fno.ftime >> 5 & 63,fno.ftime*2);  //创建时间     //
		sprintf(CreateTime1,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980, fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //创建时间     //
		convert(ReplyStructA207Ack.CreateTime1,CreateTime1);
//		ConvertReverse(ReplyStructA207Ack.CreateTime1);

		sprintf(CreateTime2,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980, fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //创建时间     //
		convert(ReplyStructA207Ack.CreateTime2,CreateTime2);

		sprintf(ChangeTime1,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems
		convert(ReplyStructA207Ack.ChangeTime1,ChangeTime1);

		sprintf(ChangeTime2,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems
		convert(ReplyStructA207Ack.ChangeTime2,ChangeTime2);

		sprintf(AccessTime1,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems
		convert(ReplyStructA207Ack.AccessTime1,AccessTime1);

		sprintf(AccessTime2,"%u.%02u.%02u,%02u.%02u.%02u",
				(fno.fdate >> 9) + 1980,fno.fdate >> 5 & 15, fno.fdate & 31,fno.ftime >> 11,
				fno.ftime >> 5 & 63,fno.ftime*2);  //修改时间     // 11.21 tested by lyh: have some problems
		convert(ReplyStructA207Ack.AccessTime2,AccessTime2);

		if ((fno.fattrib & AM_RDO) == 0)
		{
			ReplyStructA207Ack.RWCtrl = 0x0;
		}
		else
		{
			ReplyStructA207Ack.RWCtrl = 0x1;
		}

		if ((fno.fattrib & AM_HID) == 0)
		{
			ReplyStructA207Ack.DisplayCtrl = 0x0;
		}
		else
		{
			ReplyStructA207Ack.DisplayCtrl = 0x1;
		}
		/****** CRC *****/
	//	ReplyStructA207Ack.CheckCode = ReplyStructA206Ack.AckPackNum + \
	//			ReplyStructA206Ack.AckHandType +ReplyStructA206Ack.AckHandId + \
	//			ReplyStructA206Ack.AckResult;
		ReplyStructA207Ack.Tail = 0xAAAAAAAA;
#if    1
		ReplyStructA207Ack.RWCtrl=SW32(ReplyStructA207Ack.RWCtrl);
		ReplyStructA207Ack.DisplayCtrl=SW32(ReplyStructA207Ack.DisplayCtrl);
#endif
		if(flag_1x==1)
		{
			AxiDma.TxBdRing.HasDRE=1;
			Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR)&ReplyStructA207Ack,
					sizeof(StructA207Ack), XAXIDMA_DMA_TO_DEVICE);

			if (Status != XST_SUCCESS)
			{
				return XST_FAILURE;
			}
			flag_1x=0;
		}
		else if(flag_tcp==1)
		{
			AxiDma1.TxBdRing.HasDRE=1;
			Status = XAxiDma_SimpleTransfer(&AxiDma1,(UINTPTR)&ReplyStructA207Ack,
					sizeof(StructA207Ack), XAXIDMA_DMA_TO_DEVICE);

			if (Status != XST_SUCCESS)
			{
				return XST_FAILURE;
			}
			flag_tcp=0;
		}
		xil_printf("%s %d  sizeof(StructA207Ack)=%d\r\n", __FUNCTION__, __LINE__,sizeof(StructA207Ack));
		return 0;
}

void Reverse_u16(u16 *name)
{
	u16 temp=0x0;
	int i=0;
	while(name[i]!=0)
	{
		temp=name[i];
		name[i]=name[i+1];
		name[i+1]=temp;
		i+=2;
	}
}

u64 Reverse_u64(u64 element)  // 8字节数据，高四字节字节调换，低四字节字节调换
{
	u32 temp_H=0x0,temp_L=0x0;
	u64 data=0;
	temp_H=((element&0xffffffff00000000)>>32);
	temp_L=(element&0xffffffff);
	temp_H=SW32(temp_H);
	temp_L=SW32(temp_L);
	data=(((u64)(temp_H))<<32)|((u64)(temp_L));
	return data;
}

void Convert_GB2312_to_UTF16LE(u16 *name)
{
		u8 dat[1024]={0};
		u8 flag=0;
		int k=0;
		int h=0;
		u16 data=0;
		while(1)
		{
			dat[k]=name[k/2]&0xff;
			dat[k+1]=((name[k/2]&0xff00)>>8);
			if( dat[k]==0 && dat[k+1]==0 )   break;
			k+=2;
		}
		k=0;
		while(1)
		{
			if(dat[k]>=0x80 && dat[k+1]>=0x80)
			{
				name[h++]=CW16(dat[k],dat[k+1]);
			}
			else if(dat[k] < 0x80 && dat[k + 1] < 0x80)
			{
				name[h++]=(u16)(dat[k]<<8);
				name[h++]=(u16)(dat[k+1]<<8);
			}
			else
			{
				name[h++]=(u16)(dat[k] << 8);
				flag = 1;
			}
			if(dat[k]==0 && dat[k+1]==0)   break;
			if (flag == 1)
			{
				k+=1;
				flag = 0;
			}
			else
			{
				k+=2;
			}
		}
		k=0;
		h=0;
		ConvertReverse(name);
		while(1)
		{
			data=name[k];
			name[k]=ff_oem2uni(data,FF_CODE_PAGE);
			if(name[k]==0)   break;
			name[k]=SW16(name[k]);
			k++;
		}
		k=0;
		h=0;
}


//初始化链表
static LinkedList InitList(void)
{
//	 LinkedList List= (Node * ) malloc (sizeof(Node));          // 分配一个头结点
//	 LinkedList List= (Node * ) wjq_malloc_t (sizeof(Node));    // 分配一个头结点
	 LinkedList List= (Node * ) wjq_malloc_m (sizeof(Node));    // 分配一个头结点
	 if (List == NULL)
	 {											  			    // 内存不足分配失败
		 return -1;
	 }
	 List->next  = NULL;

	 return List;
}

// 返回目录中的文件和子目录列表 	存储组件->主控组件
int cmd_reply_a208(BYTE* path)
{
		int Status;
		uint32_t TotalFileNum=0,TotaldirNum=0;
		LinkedList LinkList=NULL;     //12.21写
		uint32_t sum=0;
		LinkList=InitList();
		if(LinkList==NULL)
		{
			xil_printf("list allocated failed！\r\n");
			return -1;
		}
		StructA208Ack ReplyStructA208Ack;
		ReplyStructA208Ack.Head=0x55555555;
		ReplyStructA208Ack.SrcId=SRC_ID;
		ReplyStructA208Ack.DestId=DEST_ID;
#if 0
		ReplyStructA208Ack.HandType=0xA2;
		ReplyStructA208Ack.HandId=0x08;
		ReplyStructA208Ack.PackNum=0;
		ReplyStructA208Ack.AckHandType=0xA2;
		ReplyStructA208Ack.AckHandId=0x01;
#endif

//		Status = Num_of_Dir_and_File(path,&TotalFileNum,&TotaldirNum,1);
		Status = Num_of_Dir_and_File(path,&TotalFileNum,&TotaldirNum,0);
		if (Status != FR_OK) {
			xil_printf("Count Failed! ret=%d\r\n",Status);
//			return -1;
		}
		ReplyStructA208Ack.FileNum=TotalFileNum; //文件总个数
		ReplyStructA208Ack.DirNum=TotaldirNum;   //文件夹总个数
		sum=TotalFileNum+TotaldirNum;

#if 1    // 改变字节序
		ReplyStructA208Ack.HandType=SW32(0xA2);
		ReplyStructA208Ack.HandId=SW32(0x08);
		ReplyStructA208Ack.PackNum=SW32(0);
		ReplyStructA208Ack.AckHandType=SW32(0xA2);
		ReplyStructA208Ack.AckHandId=SW32(0x01);
		ReplyStructA208Ack.FileNum=SW32(ReplyStructA208Ack.FileNum);
		ReplyStructA208Ack.DirNum=SW32(ReplyStructA208Ack.DirNum);
#endif

		ReplyStructA208Ack.CheckCode=0x0;
		ReplyStructA208Ack.Tail=0xAAAAAAAA;

		// 循环N个单个文件或文件夹目录信息
		// 1.19号之前的代码，客户不想展示每一层文件夹下的内容，只想显示一层，因此1.19号之后用另一套代码
		record_struct_of_Dir_and_File(path,LinkList);

//		ReplyStructA208Ack.message=(SingleFileOrDir  *)wjq_malloc_t(sizeof(SingleFileOrDir)*(sum+1));// 定义存储单个文件或文件夹目录信息结构体数组
		ReplyStructA208Ack.message=(SingleFileOrDir  *)wjq_malloc_m(sizeof(SingleFileOrDir)*(sum+1));// 定义存储单个文件或文件夹目录信息结构体数组
		LinkedList r=LinkList;
		for(int i=0;;i++)
        {
        	ReplyStructA208Ack.message[i]=r->data;
        	r=r->next;
        	if(r==NULL)
        		break;
        }

		for(int i=1;i<sum+1;i++)
		{
			Convert_GB2312_to_UTF16LE(ReplyStructA208Ack.message[i].name);
			Reverse_u16(ReplyStructA208Ack.message[i].name);
			ReplyStructA208Ack.message[i].type=Reverse_u64(ReplyStructA208Ack.message[i].type);
			ReplyStructA208Ack.message[i].size=Reverse_u64(ReplyStructA208Ack.message[i].size);
			if(ReplyStructA208Ack.message[i].type==0x1000000)    ReplyStructA208Ack.message[i].size=0x0;//24.9.28 add by lyh  文件夹的大小不能为0，否则上位机会崩
//			ReplyStructA208Ack.message[i].type=SW64(ReplyStructA208Ack.message[i].type);
//			ReplyStructA208Ack.message[i].size=SW64(ReplyStructA208Ack.message[i].size);
		}
#if  10
		memcpy((uint8_t *)(0xA0000000),&ReplyStructA208Ack,40);
		for(int i=1;i<sum+1;i++)
		{
			memcpy((uint8_t *)(0xA0000028+(i-1)*sizeof(SingleFileOrDir)),&ReplyStructA208Ack.message[i],sizeof(SingleFileOrDir));
		}

		memcpy((uint8_t *)(0xA0000028+sum*sizeof(SingleFileOrDir)),&ReplyStructA208Ack.CheckCode,4);
		memcpy((uint8_t *)(0xA0000028+sum*sizeof(SingleFileOrDir)+4),&ReplyStructA208Ack.Tail,4);
		write_len=48+sum*sizeof(SingleFileOrDir);

#endif

#if 0
		cmd_reply_a203(0,0xA2,0x1,0x11);
		usleep(100);

		AxiDma.TxBdRing.HasDRE=1;
		Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR) &ReplyStructA208Ack,
        					40, XAXIDMA_DMA_TO_DEVICE);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		AxiDma.TxBdRing.HasDRE=1;
		for(int i=1;i<sum+1;i++)
		{
				Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR) &ReplyStructA208Ack.message[i],
						sizeof(SingleFileOrDir), XAXIDMA_DMA_TO_DEVICE);
				if (Status != XST_SUCCESS) {
					return XST_FAILURE;
				}
				xil_printf("%s %d  sizeof(SingleFileOrDir)=%d\r\n", __FUNCTION__, __LINE__,sizeof(SingleFileOrDir));
		}
// 		发校验码和包尾
		AxiDma.TxBdRing.HasDRE=1;
		Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR) &ReplyStructA208Ack.CheckCode,
							4, XAXIDMA_DMA_TO_DEVICE);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		AxiDma.TxBdRing.HasDRE=1;
		Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR) &ReplyStructA208Ack.Tail,
							4, XAXIDMA_DMA_TO_DEVICE);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

#endif
		//销毁链表
		DestroyList(LinkList);
//		wjq_free_t(ReplyStructA208Ack.message); 	// 释放内存
		wjq_free_m(ReplyStructA208Ack.message); 	// 释放内存
		LinkList=NULL;
		return 0;
}

int run_cmd_b201(StructMsg *pMsg)
{
	int file_cmd=0,ret=0,i=0;
	BYTE Buff[4096];
	file_cmd = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
	switch(file_cmd)
	{
		case DISK_FORMAT:
				ret = f_mkfs(
						" ",	/* Logical drive number */
						0,			/* Format option  FM_EXFAT*/
						Buff,			/* Pointer to working buffer (null: use heap memory) */
						sizeof Buff			/* Size of working buffer [byte] */
						);
				if (ret != FR_OK)
				{
						xil_printf("f_mkfs  Failed! ret=%d\r\n", ret);
					//	cmd_reply_a203_to_b201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x10);
						return -1;
				}
//				cmd_reply_a203_to_b201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x11);
		break;

		case DISK_REMOUNT:
				ret = f_mount(&fs,"",1);
				if (ret != FR_OK)
				{
						xil_printf("f_remount  Failed! ret=%d\r\n", ret);
					//	cmd_reply_a203_to_b201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x10);
						return -1;
				}
//				cmd_reply_a203_to_b201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x11);
		break;

		case DISK_UNMOUNT:
				ret = f_mount(0,"",1);
				if (ret != FR_OK)
				{
						xil_printf("f_unmount  Failed! ret=%d\r\n", ret);
					//	cmd_reply_a203_to_b201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x10);
						return -1;
				}
//				cmd_reply_a203_to_b201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x11);
		break;

		default:
			break;
	}
//	cmd_reply_a203(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x11);
	return 0;
}

int run_cmd_b202(StructMsg *pMsg)
{
		int IPADDR=0,ret=0,i=0;
		u32 sendBuffer[3]={0};
		IPADDR = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
		switch(IPADDR)
		{
			case 1:
				sendBuffer[0]=0x00000000;
				sendBuffer[1]=0x55AA55AA;
				sendBuffer[2]=0x1;
			break;

			case 2:
				sendBuffer[0]=0x00000000;
				sendBuffer[1]=0x55AA55AA;
				sendBuffer[2]=0x2;
			break;

			case 3:
				sendBuffer[0]=0x00000000;
				sendBuffer[1]=0x55AA55AA;
				sendBuffer[2]=0x3;
			break;

			case 4:
				sendBuffer[0]=0x00000000;
				sendBuffer[1]=0x55AA55AA;
				sendBuffer[2]=0x4;
			break;

			default:
				break;
		}
		TxSend(sendBuffer,12);
		return 0;
}

int run_cmd_d201(StructMsg *pMsg)
{
		int file_cmd=0,ret=0,i=0,x=0,h=0;
		int temp=0;
		u16 unicode_u16=0;
		WCHAR cmd_str_1[1024]={0};
		BYTE cmd_str_11[100]={0};
		int32_t file_offset=0,file_seek=0;
		FIL file1;
		BYTE *cmd_char_str1;
		for (x = 0; x < 1024; x++)
		{
			unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
			cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
			if(cmd_str_1[x]<=0x7E)
			{
				cmd_str_11[h++]=cmd_str_1[x];
			}
			else
			{
				cmd_str_11[h++]=(cmd_str_1[x]>>8);
				cmd_str_11[h++]=cmd_str_1[x];
			}
			if (cmd_str_1[x] == '\0') break;
		}              // 文件路径
		xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_11);
//		i=temp+strlen(cmd_str_11)*2;
		i=temp+2048;   // 1.15 改 by lyh
		file_offset = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
		  // 偏移量
		i=i+4;
		file_seek = CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
		  // 起始位置
		ret =f_open(&file1, cmd_str_11,  FA_OPEN_EXISTING |FA_READ);
		if (ret != FR_OK)
		{
			xil_printf("f_open  Failed! ret=%d\r\n", ret);
			return -1;
		}         // 应该先打开文件
		switch(file_seek)
		{
			   case   SEEK_SET:
				   // offset
				   ret =f_lseek(&file1,file_offset);
				   if (ret != FR_OK)
				   {
					   xil_printf("f_lseek  Failed! ret=%d\r\n", ret);
					   return -1;
				   }
				   f_close(&file1);
//				   cmd_reply_a203_to_d201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x11);
			   break;

			   case   SEEK_CUR:
				   // offset
				   ret =f_lseek(&file1,f_tell(&file1)+file_offset);
				   if (ret != FR_OK)
				   {
					   xil_printf("f_lseek  Failed! ret=%d\r\n", ret);
					   return -1;
				   }
				   f_close(&file1);
//				   cmd_reply_a203_to_d201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x11);
			   break;

			   case  SEEK_END:
				   ret =f_lseek(&file1,f_size(&file1)+file_offset);
				   if (ret != FR_OK)
				   {
					   xil_printf("f_lseek  Failed! ret=%d\r\n", ret);
					   return -1;
				   }
				   f_close(&file1);
//				   cmd_reply_a203_to_d201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x11);
			   break;
			   default:
			   //cmd_reply_a203_to_d201(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x10);
			   break;
		}
//		 cmd_reply_a203(pMsg->PackNum,pMsg->HandType, pMsg->HandId, 0x11);
		 return 0;
}

/*****************包写入文件命令*******************/
int run_cmd_d202(StructMsg *pMsg)
{
		uint32_t ret=0,i=0,x=0,h=0,temp=0,bw=0;
		u16 unicode_u16=0;
		WCHAR cmd_str_1[1024]={0};
		BYTE cmd_str_11[100]={0};
		uint32_t  cmd_write_cnt=0,cmd_len=0;
		uint8_t   sts=0;
		uint32_t  len=0,Free_Clust=0;
		uint32_t  count=0;
		uint32_t  buff = (void *)(MEM_DDR4_BASE);
		for (x = 0; x < 1024; x++)
		{
			unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
			cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
			if(cmd_str_1[x]<=0x7E)
			{
				cmd_str_11[h++]=cmd_str_1[x];
			}
			else
			{
				cmd_str_11[h++]=(cmd_str_1[x]>>8);
				cmd_str_11[h++]=cmd_str_1[x];
			}
			if (cmd_str_1[x] == '\0')  break;
		}
		i=temp+2048;

#if     1	//覆盖写入
		ret = f_open(&wfile,cmd_str_11, FA_CREATE_ALWAYS | FA_WRITE |FA_READ);
		if (ret != FR_OK)
		{
			xil_printf("f_open Failed! ret=%d\r\n", ret);
			return ret;
		}
		xil_printf("Waiting FPGA Write Start! name:%s\r\n",cmd_str_11);
#endif

		while (1)
		{
			if (RxReceive(DestinationBuffer,&cmd_len) == XST_SUCCESS)
			{

				buff =DestinationBuffer[0];  // 保存写入数据的DDR地址
				len  =DestinationBuffer[1];  // 写入数据的长度

				if(buff==0x3C3CBCBC)		// 3C3CBCBC认为是结束标志
				{
					xil_printf("I/O Write Finish!\r\n");
					xil_printf("w_count = %u w_size = %lu\r\n",cmd_write_cnt,f_size(&wfile));
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
				if(len>=REMAIN_SPACE)
				{
					xil_printf("Have no space!\r\n");
//					return -1;
					break;
				}
				ret = f_write1(
					&wfile,			/* Open file to be written */
					buff,			/* Data to be written */
					len,			/* Number of bytes to write */
					&bw				/* Number of bytes written */
				);
				if (ret != FR_OK)
				{
					 xil_printf(" f_write Failed! %d\r\n",ret);
					 f_close(&wfile);
					 return ret;
				}
				cmd_write_cnt += 1;
				REMAIN_SPACE-=len;
//				xil_printf("buff:0x%lx  len:0x%lx\r\n",buff,len);
			}
			else
			{
				for(i=0;i<NHC_NUM;i++)
				{
					do {
							sts = nhc_cmd_sts(i);
						}while(sts == 0x01);
				}
//				count++;
//				if(count==0xe0000)
//				{
//					xil_printf("TimeOut done! %d\r\n",count);
//					return 0;
//				}

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
		 }
		 ret=f_close(&wfile);
		 if (ret != FR_OK)
		 {
			 xil_printf(" f_close Failed! %d\r\n",ret);
			 return ret;
		 }
		 return 0;
}

/*****************流式写入文件命令*******************/
int run_cmd_d203(StructMsg *pMsg)
{
		uint32_t ret=0,i=0,x=0,h=0;
		u16 unicode_u16=0;
		uint32_t Status=0,bw=0;
		u32 file_cmd=0;
		uint8_t sts;
		WCHAR cmd_str_1[1024]={0};
		BYTE cmd_str_11[100]={0};
		uint32_t cmd_write_cnt=0,cmd_len=0;
		uint32_t  len;
		uint32_t  buff = (void *)(MEM_DDR4_BASE);
//		uint8_t wbuff[4096]={0};
		for (x = 0; x < 1024; x++)
		{
			unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
			cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
			if(cmd_str_1[x]<=0x7E)
			{
				cmd_str_11[h++]=cmd_str_1[x];
			}
			else
			{
				cmd_str_11[h++]=(cmd_str_1[x]>>8);
				cmd_str_11[h++]=cmd_str_1[x];
			}
			if (cmd_str_1[x] == '\0') break;
		}
		xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_11);


#if   10	//覆盖写入
		// 获取并解析从DMA0传过来的文件路径
		ret = f_open(&wfile,cmd_str_11, FA_CREATE_ALWAYS | FA_WRITE |FA_READ);
//		ret = f_open(&wfile,"G", FA_CREATE_ALWAYS | FA_WRITE |FA_READ);
		if (ret != FR_OK)
		{
			xil_printf("f_open Failed! ret=%d\r\n", ret);
			ret = cmd_reply_AcqusionRet(0x01,0x10,0x0);//接收到开始采集指令，但是准备开始采集的状态有问题，导致无法接收采集数据，需要给上位机回一个失败的状态。
			if (ret != FR_OK)
			{
				xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", ret);
				return ret;
			}
			return ret;
		}

		xil_printf("Waiting FPGA Vio Ctrl Read Write Start\r\n");
		ret = cmd_reply_AcqusionRet(0x01,0x11,0x0);//接收到开始采集指令，且已经准备好开始采集。
		if (ret != FR_OK)
		{
			xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", ret);
			return ret;
		}
#endif

#if   0    //不覆盖写入
		// 获取并解析从DMA0传过来的文件路径
		ret = f_open(&wfile,"A", FA_OPEN_EXISTING| FA_WRITE |FA_READ);
//		ret = f_open(&wfile,cmd_str_11, FA_OPEN_EXISTING| FA_WRITE |FA_READ);
		if(ret ==FR_NO_FILE)
		{
//		    ret = f_open(&wfile,cmd_str_11, FA_CREATE_ALWAYS | FA_WRITE |FA_READ);
			ret = f_open(&wfile,"A", FA_CREATE_ALWAYS | FA_WRITE |FA_READ);
			if (ret != FR_OK)
			{
				xil_printf("f_open Failed! ret=%d\r\n", ret);
				//cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x10);  // lyh 2023.8.15
				return ret;
			}
			xil_printf(" Open ok!\r\n");
		}
		else if(ret ==FR_OK)
		{
			xil_printf(" Open ok!\r\n");
		}
		else
		{
			xil_printf("f_open Failed! ret=%d\r\n", ret);
			//cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x10);  // lyh 2023.8.15
			return ret;
		}
		xil_printf("Waiting FPGA Vio Ctrl Read Write Start\r\n");
		ret = f_lseek(&wfile, f_size(&wfile));
		if (ret != FR_OK)
		{
			xil_printf("f_lseek Failed! ret=%d\r\n", ret);
			//cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x10);  // lyh 2023.8.15
			return ret;
		}
#endif

		while (1)
		{
//			xil_printf("Start Write!\r\n");
			if (RxReceive(DestinationBuffer,&cmd_len) == XST_SUCCESS)
			{
				buff =DestinationBuffer[0];  // 保存写入数据的DDR地址
				len  =DestinationBuffer[1];  // 写入数据的长度
//				buff =0x80000000;  // 保存写入数据的DDR地址
//				len  =0x40000000;  // 写入数据的长度
				if(buff==0x3C3CBCBC)		// 3.19号改 by lyh
				{
					xil_printf("I/O Write Finish!\r\n");
//					xil_printf("w_count = %u\r\n",cmd_write_cnt);
					xil_printf("w_count = %u w_size = %lu\r\n",cmd_write_cnt,f_size(&wfile));
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
//				if(cmd_write_cnt==15)
//				{
//					xil_printf("Write Finish!\r\n");
////					xil_printf("w_count = %u\r\n",cmd_write_cnt);
//					xil_printf("w_count = %u w_size = %lu\r\n",cmd_write_cnt,f_size(&wfile));
//					for(i=0;i<NHC_NUM;i++)
//					{
//						while (nhc_queue_ept(i) == 0)
//						{
//							do {
//								sts = nhc_cmd_sts(i);
//							}while(sts == 0x01);
//						}
//					}
//					break;
//				}
//				FLAG=1;
				if(len>=REMAIN_SPACE)
				{
					xil_printf("Have no space! Stop to write!\r\n");
//					return -1;
					ret = cmd_reply_AcqusionRet(0x02,0x11,0x1);//异常退出，需要停止采集。
					if (ret != FR_OK)
					{
						xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", ret);
						return ret;
					}
					break;
				}
				ret = f_write1(
					&wfile,			/* Open file to be written */
					buff,			/* Data to be written */
					len,			/* Number of bytes to write */
					&bw				/* Number of bytes written */
				);
				if (ret != FR_OK)
				{
					 xil_printf(" f_write Failed! %d\r\n",ret);
					 f_close(&wfile);
					 usleep(100000);
					 Status = cmd_reply_AcqusionRet(0x02,0x11,0x1);//异常退出，需要停止采集。
					 if (Status != FR_OK)
					 {
						xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", Status);
						return Status;
					 }
					 return ret;
				}
				cmd_write_cnt += 1;
				REMAIN_SPACE-=len;
//				xil_printf("write_cnt:%u \r\n",cmd_write_cnt);
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

			for(i=0;i<NHC_NUM;i++)
			{
				while (nhc_queue_ept(i) == 0)
				{
					do {
						sts = nhc_cmd_sts(i);
					}while(sts == 0x01);
				}
			}
//			if(flag_tcp==1)  break;
			if(Stop_write==1)
			{
				Stop_write=0;
				xil_printf("Stop write!\r\n");
				ret = cmd_reply_AcqusionRet(0x02,0x11,0x0);//接收到停止采集指令，且已经准备好停止采集。
				if (ret != FR_OK)
				{
					xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", ret);
					return ret;
				}
				break;    
			}
		 }   // while
		 ret=f_close(&wfile);
		 if (ret != FR_OK)
		 {
			 xil_printf(" f_close Failed! %d\r\n",ret);
			 return ret;
		 }
		 return 0;
}


/*****************分包回放文件命令——1次读完*******************/
int run_cmd_d205(BYTE* name,uint8_t mode)
{
	  int i=0,x=0,Status,ret,h=0,time=1;
	  uint8_t sts=0;
	  int64_t size=0,LastPack_Size=0,len=0;
	  int br;
	  uint32_t  r_count=0,cmd_len=0;;
	  uint32_t  MODE=0;
	  uint32_t  buff_r=(void *)(0x90000000);

	  xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,name);
	  switch(mode)
	  {
	  	  case 0:
	  		 MODE=MODE_tcp;
	  		 break;

	  	  case 1:
	  		 MODE=MODE_1x;
	  	     break;

	  	  case 2:
	  		 MODE=MODE_8x;
	  		 break;

	  	  default:
			 break;
	  }
	  ret = f_open(&rfile,name, FA_OPEN_EXISTING |FA_READ);
//	  ret = f_open(&rfile,"D", FA_OPEN_EXISTING |FA_READ|FA_WRITE);
	  if (ret != FR_OK)
	  {
			xil_printf("f_open Failed! ret=%d\r\n", ret);
ret = cmd_reply_AcqusionRet(0x03,0x10,0x0);//接收到开始回放指令，但是没有准备好开始回放。
		    if (ret != FR_OK)
		    {
			    xil_printf("reply  Acqusion_Result Failed! ret=%d\r\n", ret);
			    return ret;
		    }
			return ret;
	  }
	  ret = cmd_reply_AcqusionRet(0x03,0x11,0x0);//接收到开始回放指令，但是没有准备好开始回放。
	  if (ret != FR_OK)
	  {
		  xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", ret);
		  return ret;
	  }
	  size=((f_size(&rfile)%4)==0?f_size(&rfile):(f_size(&rfile)/4+1)*4);
	  len=size;
	  if(size>Read_Packet_Size)
	  {
		  time=size/Read_Packet_Size+1;
		  len=Read_Packet_Size;
		  LastPack_Size=((size%Read_Packet_Size)/4+1)*4;
		  if((size%Read_Packet_Size)==0)
		  {
			  time=size/Read_Packet_Size;
			  len=Read_Packet_Size;
			  LastPack_Size=Read_Packet_Size;
		  }
	  }
	  //告诉fpga切换模式
	  DestinationBuffer_1[0]=DUMMY;
	  DestinationBuffer_1[1]=MODE;
//	  DestinationBuffer_1[1]=MODE_8x;
	  ret = TxSend(DestinationBuffer_1,8);
	  if (ret != XST_SUCCESS)
	  {
		  xil_printf("TxSend Failed! ret=%d\r\n", ret);
		  return ret;
	  }

//开始读取
	  while(1)
	  {
		  	ret = f_read1(
						&rfile,
						buff_r,
						len,
						&br
			);
			if (ret != FR_OK)
			{
					xil_printf("f_read Failed! ret=%d\r\n", ret);
					usleep(100000);
					ret = cmd_reply_AcqusionRet(0x04,0x11,0x1);//接收到停止回放指令，且已经准备好停止回放。
					if (ret != FR_OK)
					{
						xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", ret);
						return ret;
					}
					return ret;
			}
			r_count++;

			//sfifo发送长度
			DestinationBuffer_1[0]=len;
			DestinationBuffer_1[1]=buff_r;
//			XLLFIFO_SysInit();
//			Xil_L1DCacheFlush();

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

			for(i=0;i<NHC_NUM;i++)
			{
					while (nhc_queue_ept(i) == 0)
					{
						do {
							sts = nhc_cmd_sts(i);
						}while(sts == 0x01);
					}
			 }
			 if(r_count==time-1)
			 {
				 	len=LastPack_Size;
			 }

			 if(time==r_count)
			 {
					xil_printf("I/O Read or Write Test Finish!\r\n");
					xil_printf("r_count=%u r_size=%lu\r\n",r_count,f_size(&rfile));
					DestinationBuffer_1[0]=DUMMY;
				    DestinationBuffer_1[1]=READ_FINSHED;
				    ret = TxSend(DestinationBuffer_1,8);
				    if (ret != XST_SUCCESS)
				    {
					   xil_printf("TxSend Failed! ret=%d\r\n", ret);
					   return ret;
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
					break;
			}
			if(Stop_read==1)     
			{
				Stop_read=0;
				break;    
			}

	 }  //while
	 ret=f_close(&rfile);
	 if (ret != FR_OK)
	 {
		 xil_printf(" f_close Failed! %d\r\n",ret);
		 return ret;
	 }
	 usleep(100000);
	 ret = cmd_reply_AcqusionRet(0x04,0x11,0x1);//接收到停止回放指令，且已经准备好停止回放。
	 if (ret != FR_OK)
	 {
		 xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", ret);
		 return ret;
	 }
	 return 0;
}

/*****************分包回放文件命令——循环回读*******************/
int run_cmd_d205_2(BYTE* name,int read_time,uint8_t mode)
{
	 int i=0,x=0,Status,ret,h=0,time=1;
	 uint8_t sts;
	 int64_t size=0,LastPack_Size=0,len;
	 int br;
	 uint32_t  r_count=0,cmd_len=0;
	 uint32_t  MODE=0;
	 uint32_t  buff_r=(void *)(0x80000000);
	 int Reread_time=0;
	 xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,name);
	 switch(mode)
	 {
		  case 0:
			 MODE=MODE_tcp;
			 break;

		  case 1:
			 MODE=MODE_1x;
			 break;

		  case 2:
			 MODE=MODE_8x;
			 break;

		  default:
			 break;
	 }
	 ret = f_open(&rfile,name, FA_OPEN_EXISTING |FA_READ);
//	 ret = f_open(&rfile,"D", FA_OPEN_EXISTING |FA_READ|FA_WRITE);
	 if (ret != FR_OK)
	 {
			xil_printf("f_open Failed! ret=%d\r\n", ret);
			ret = cmd_reply_AcqusionRet(0x03,0x10,0x0);//接收到开始回放指令，但是没准备好开始回放。
		    if (ret != FR_OK)
		    {
			    xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", ret);
			    return ret;
		    }
			return ret;
	  }
	  ret = cmd_reply_AcqusionRet(0x03,0x11,0x0);//接收到开始回放指令，且已经准备好开始回放。
	  if (ret != FR_OK)
	  {
		  xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", ret);
		  return ret;
	  }
	  size=((f_size(&rfile)%4)==0?f_size(&rfile):(f_size(&rfile)/4+1)*4);
	  len=size;
	  if(size>Read_Packet_Size)
	  {
		  time=size/Read_Packet_Size+1;
		  len=Read_Packet_Size;
		  LastPack_Size=((size%Read_Packet_Size)/4+1)*4;
		  if((size%Read_Packet_Size)==0)
		  {
			  time=size/Read_Packet_Size;
			  len=Read_Packet_Size;
			  LastPack_Size=Read_Packet_Size;
		  }
	  }
	  //告诉fpga切换模式
	  DestinationBuffer_1[0]=DUMMY;
//	  DestinationBuffer_1[1]=MODE_8x;
	  DestinationBuffer_1[1]=MODE;
	  ret = TxSend(DestinationBuffer_1,8);
	  if (ret != XST_SUCCESS)
	  {
		  xil_printf("TxSend Failed! ret=%d\r\n", ret);
		  return ret;
	  }

//开始读取
	  while(1)
	  {
		  	ret = f_read1(
						&rfile,
						buff_r,
						len,
						&br
			);
			if (ret != FR_OK)
			{
					xil_printf("f_read Failed! ret=%d\r\n", ret);
					usleep(100000);
					ret = cmd_reply_AcqusionRet(0x04,0x11,0x1);//接收到停止回放指令，且已经准备好停止回放。
					if (ret != FR_OK)
					{
						xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", ret);
						return ret;
					}
					return ret;
			}
			r_count++;

			//sfifo发送长度
			DestinationBuffer_1[0]=len;
			DestinationBuffer_1[1]=buff_r;
//			XLLFIFO_SysInit();
//			Xil_L1DCacheFlush();

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

			for(i=0;i<NHC_NUM;i++)
			{
					while (nhc_queue_ept(i) == 0)
					{
						do {
							sts = nhc_cmd_sts(i);
						}while(sts == 0x01);
					}
			 }
			 if(r_count==time-1)
			 {
				 len=LastPack_Size;
			 }

			 if(time==r_count)
			 {
//					xil_printf("I/O Read 1 time Finish!\r\n");
//					xil_printf("r_count=%u\r\n",r_count);
					DestinationBuffer_1[0]=DUMMY;
				    DestinationBuffer_1[1]=READ_FINSHED;
				    r_count=0;
				    ret = TxSend(DestinationBuffer_1,8);
				    if (ret != XST_SUCCESS)
				    {
					   xil_printf("TxSend Failed! ret=%d\r\n", ret);
					   return ret;
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
					Reread_time++;
					if(read_time==Reread_time)
					{
						 xil_printf(" Recycle_read Finished! recycle time:%d\r\n",Reread_time);
						 break;
					}
			}
			if(Stop_read==1)     //读取到取消回读的标志
			{
				Stop_read=0;
				break;    //终止回读
			}

	 }  //while
	 ret=f_close(&rfile);
	 if (ret != FR_OK)
	 {
		 xil_printf(" f_close Failed! %d\r\n",ret);
		 return ret;
	 }
	 usleep(100000);
	 ret = cmd_reply_AcqusionRet(0x04,0x11,0x1);//接收到停止回放指令，且已经准备好停止回放。
	 if (ret != FR_OK)
	 {
		 xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", ret);
		 return ret;
	 }
	 return 0;
}

/*****************8x分包回放文件命令——1次读完*******************/
int run_cmd_d205_8x(BYTE* name)
{
	  int i=0,x=0,Status,ret,h=0,time=1;
	  uint8_t sts=0;
	  uint32_t num=0;
	  int64_t size=0,LastPack_Size=0,len=0;
	  int br;
	  uint32_t  r_count=0,cmd_len=0;;
	  uint32_t  MODE=0;
	  uint32_t  buff_r=(void *)(0x80000000);

	  xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,name);
	  ret = f_open(&rfile,name, FA_OPEN_EXISTING |FA_READ|FA_WRITE);
//	  ret = f_open(&rfile,"G", FA_OPEN_EXISTING |FA_READ|FA_WRITE);
	  if (ret != FR_OK)
	  {
			xil_printf("f_open Failed! ret=%d\r\n", ret);
	  		ret = cmd_reply_AcqusionRet(0x03,0x10,0x0);//接收到开始回放指令，但是没有准备好开始回放。
			if (ret != FR_OK)
			{
				xil_printf("reply  Acqusion_Result Failed! ret=%d\r\n", ret);
				return ret;
			}
			return ret;
	  }
	  ret = cmd_reply_AcqusionRet(0x03,0x11,0x0);//接收到开始回放指令，且已经准备好开始回放。
	  if (ret != FR_OK)
	  {
			xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", ret);
			return ret;
	  }
	  size=((f_size(&rfile)%4)==0?f_size(&rfile):(f_size(&rfile)/4+1)*4);
	  len=size;
	  if(size>Read_Packet_Size)
	  {
		  time=size/Read_Packet_Size+1;
		  len=Read_Packet_Size;
		  LastPack_Size=((size%Read_Packet_Size)/4+1)*4;
		  if((size%Read_Packet_Size)==0)
		  {
			  time=size/Read_Packet_Size;
			  len=Read_Packet_Size;
			  LastPack_Size=Read_Packet_Size;
		  }
	  }
	  //告诉fpga切换模式
	  DestinationBuffer_1[0]=DUMMY;
	  DestinationBuffer_1[1]=MODE_8x;
	  ret = TxSend(DestinationBuffer_1,8);
	  if (ret != XST_SUCCESS)
	  {
		  xil_printf("TxSend Failed! ret=%d\r\n", ret);
		  return ret;
	  }
//开始读取
	  while(1)
	  {
		  	ret = f_read1(
						&rfile,
						buff_r,
						len,
						&br
			);
			if (ret != FR_OK)
			{
					xil_printf("f_read Failed! ret=%d\r\n", ret);
					usleep(100000);
					ret = cmd_reply_AcqusionRet(0x04,0x11,0x1);// 准备好停止回放。
					if (ret != FR_OK)
					{
						xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", ret);
						return ret;
					}
					return ret;
			}
			r_count++;
//			xil_printf("r_count:%u\r\n", r_count);
//			if(r_count==15)
//			{
//				xil_printf("Write Finish!\r\n");
//				xil_printf("r_count = %u \r\n",r_count);
//				for(i=0;i<NHC_NUM;i++)
//				{
//					while (nhc_queue_ept(i) == 0)
//					{
//						do {
//							sts = nhc_cmd_sts(i);
//						}while(sts == 0x01);
//					}
//				}
//				break;
//			}
			//sfifo发送长度
			DestinationBuffer_1[0]=len;
			DestinationBuffer_1[1]=buff_r;
//			XLLFIFO_SysInit();
//			Xil_L1DCacheFlush();
//			if(r_count<3)
//			{
//				usleep(100000);
//			}
			ret = TxSend(DestinationBuffer_1,8);
			if (ret != XST_SUCCESS)
			{
				 xil_printf("TxSend Failed! ret=%d\r\n", ret);
				 return ret;
			}
//			RxReceive(DestinationBuffer_1,&cmd_len);
//			num=XLlFifo_iTxVacancy(&Fifo0);
//			xil_printf("num=%d\r\n", num);
//			if(r_count>3)
			if(r_count>5)
			{
//				while(XLlFifo_IsRxEmpty(&Fifo0)!=FALSE);
				do
				{
					RxReceive(DestinationBuffer_1,&cmd_len);
				}while(!(0xaa55aa55 == DestinationBuffer_1[0]));
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
		     buff_r+=len;
		     if(buff_r>=0xDFFFFFFF)   buff_r=(void *)(0x80000000);

			 if(r_count==time-1)
			 {
				 	len=LastPack_Size;
			 }

			 if(time==r_count)
			 {
					xil_printf("I/O Read or Write Test Finish!\r\n");
					xil_printf("r_count=%u r_size=%llu\r\n",r_count,f_size(&rfile));
					DestinationBuffer_1[0]=DUMMY;
				    DestinationBuffer_1[1]=READ_FINSHED;
				    ret = TxSend(DestinationBuffer_1,8);
				    if (ret != XST_SUCCESS)
				    {
					   xil_printf("TxSend Failed! ret=%d\r\n", ret);
					   return ret;
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
					break;
			}
			if(Stop_read==1)     //读取到取消回读的标志
			{
				Stop_read=0;
				break;    //终止回读
			}

	 }  //while
	 ret=f_close(&rfile);
	 if (ret != FR_OK)
	 {
		 xil_printf(" f_close Failed! %d\r\n",ret);
		 return ret;
	 }
	 usleep(100000);
	 ret = cmd_reply_AcqusionRet(0x04,0x11,0x1);//接收到停止回放指令，且已经准备好停止回放。
	 if (ret != FR_OK)
	 {
		xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", ret);
		return ret;
	 }
	 return 0;
}

/*****************分包回放文件命令——循环回读*******************/
int run_cmd_d205_2_8x(BYTE* name,int read_time)
{
	 int i=0,x=0,Status,ret,h=0,time=1;
	 uint8_t sts;
	 int64_t size=0,LastPack_Size=0,len=0;
	 int br;
	 uint32_t  r_count=0,cmd_len=0;
	 uint32_t  MODE=0;
	 uint32_t  buff_r=(void *)(0x80000000);
	 int Reread_time=0;
	 xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,name);

	 ret = f_open(&rfile,name, FA_OPEN_EXISTING |FA_READ|FA_WRITE);
//	 ret = f_open(&rfile,"D", FA_OPEN_EXISTING |FA_READ|FA_WRITE);
	 if (ret != FR_OK)
	 {
			xil_printf("f_open Failed! ret=%d\r\n", ret);
			ret = cmd_reply_AcqusionRet(0x03,0x10,0x0);//接收到开始回放指令，但是没有准备好。
			if (ret != FR_OK)
			{
				xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", ret);
				return ret;
			}		
			return ret;
	  }
	  ret = cmd_reply_AcqusionRet(0x03,0x11,0x0);//接收到开始回放指令，且已经准备好开始回放。
	  if (ret != FR_OK)
	  {
		  xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", ret);
		  return ret;
	  }
	  size=((f_size(&rfile)%4)==0?f_size(&rfile):(f_size(&rfile)/4+1)*4);
	  len=size;
	  if(size>Read_Packet_Size)
	  {
		  time=size/Read_Packet_Size+1;
		  len=Read_Packet_Size;
		  LastPack_Size=((size%Read_Packet_Size)/4+1)*4;
		  if((size%Read_Packet_Size)==0)
		  {
			  time=size/Read_Packet_Size;
			  len=Read_Packet_Size;
			  LastPack_Size=Read_Packet_Size;
		  }
	  }
	  //告诉fpga切换模式
	  DestinationBuffer_1[0]=DUMMY;
	  DestinationBuffer_1[1]=MODE_8x;
	  ret = TxSend(DestinationBuffer_1,8);
	  if (ret != XST_SUCCESS)
	  {
		  xil_printf("TxSend Failed! ret=%d\r\n", ret);
		  return ret;
	  }

//开始读取
	  while(1)
	  {
		  	ret = f_read1(
						&rfile,
						buff_r,
						len,
						&br
			);
			if (ret != FR_OK)
			{
					xil_printf("f_read Failed! ret=%d\r\n", ret);
					usleep(100000);
					ret = cmd_reply_AcqusionRet(0x04,0x11,0x1);//接收到停止回放指令，且已经准备好停止回放。
					if (ret != FR_OK)
					{
						xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", ret);
						return ret;
					}
					return ret;
			}
			r_count++;

			//sfifo发送长度
			DestinationBuffer_1[0]=len;
			DestinationBuffer_1[1]=buff_r;
//			XLLFIFO_SysInit();
//			Xil_L1DCacheFlush();

			ret = TxSend(DestinationBuffer_1,8);
			if (ret != XST_SUCCESS)
			{
				 xil_printf("TxSend Failed! ret=%d\r\n", ret);
				 return ret;
			}
			xil_printf("buff_r=%x len=%lu \n\r",buff_r,len);
			if(r_count>2)
			{
				do
				{
					RxReceive(DestinationBuffer_1,&cmd_len);
				}while(!(0xaa55aa55 == DestinationBuffer_1[0]));
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
			 buff_r+=len;
			 if(buff_r>=0xDFFFFFFF)   buff_r=(void *)(0x80000000);
			 if(r_count==time-1)
			 {
				 len=LastPack_Size;
			 }

			 if(time==r_count)
			 {
//					xil_printf("I/O Read 1 time Finish!\r\n");
//					xil_printf("r_count=%u\r\n",r_count);
					DestinationBuffer_1[0]=DUMMY;
				    DestinationBuffer_1[1]=READ_FINSHED;
				    r_count=0;
				    buff_r=(void *)(0x80000000);
				    ret = TxSend(DestinationBuffer_1,8);
				    if (ret != XST_SUCCESS)
				    {
					   xil_printf("TxSend Failed! ret=%d\r\n", ret);
					   return ret;
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
					Reread_time++;
					if(read_time==Reread_time)
					{
						 xil_printf(" Recycle_read Finished! recycle time:%d\r\n",Reread_time);
						 break;
					}
			}
			if(Stop_read==1)     //读取到取消回读的标志
			{
				Stop_read=0;
				break;    //终止回读
			}

	 }  //while
	 ret=f_close(&rfile);
	 if (ret != FR_OK)
	 {
		 xil_printf(" f_close Failed! %d\r\n",ret);
	 
		 return ret;
	 }
	 usleep(100000);
	 ret = cmd_reply_AcqusionRet(0x04,0x11,0x1);//接收到停止回放指令，且已经准备好停止回放。
	 if (ret != FR_OK)
	 {
		 xil_printf("reply Acqusion_Result Failed! ret=%d\r\n", ret);
		 return ret;
	 }
	 return 0;
}

//******回放文件数据命令*******//
int run_cmd_d204(StructMsg *pMsg)
{
     int temp=0,ret=0,i=0,x=0,h=0;
	 int Read_mode,Read_time;
	 u16 unicode_u16=0;
	 WCHAR cmd_str_1[1024]={0};
	 BYTE cmd_str_11[100]={0};
	 FIL file;
	 int x0=0,x1=0,x2=0;

     for (x = 0; x < 1024; x++)
     {
      	 unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
    	 cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
    	 if(cmd_str_1[x]<=0x7E)
		 {
			 cmd_str_11[h++]=cmd_str_1[x];
		 }
		 else
		 {
			 cmd_str_11[h++]=(cmd_str_1[x]>>8);
			 cmd_str_11[h++]=cmd_str_1[x];
		 }
         if (cmd_str_1[x] == '\0') break;
    }           // 文件路径
    xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_11);
//  i=temp+strlen(cmd_str_11)*2;
    i=temp+2048;
	Read_mode=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);
	i+=4;
	x0=(((Read_mode>>0)&0x1)==1)?1:0;     //x0=1为流式读取,x0=0为分包读取

	x1=(((Read_mode>>4)&0xf)==0)?0:		//x1=0为通过网口回传读数据,x1=1为通过GTH 1x回传数据,x1=2为通过GTH 8x回传数据
				(((Read_mode>>4)&0xf)==1)?1:2;
	x2=(((Read_mode>>8)&0x1)==1)?1:0;		//x2=0为一次读完 x2=1为循环回读
	Read_time=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);

	if((flag_tcp==1)&&(x1==0))
	{
		x1=0;
	}
	else if((flag_1x==1)&&(x1==0))
	{
		x1=1;
	}

#if 1   //
	switch(x0)
	{
		case 0x0:    // 分包读取;x1=0为通过网口回传读数据,x1=1为通过GTH 1x回传数据,x1=2为通过GTH 8x回传数据
		switch(x1)
		{
			case 0x0:   //网口分包回读
			switch(x2)
			{
				case 0x0:
					run_cmd_d205(cmd_str_11,x1);
//					run_cmd_d205_8x(cmd_str_11);
				break;

				case 0x1:
					run_cmd_d205_2(cmd_str_11,Read_time,x1);
//					run_cmd_d205_2_8x(cmd_str_11,Read_time);
				break;

				default:
					break;
			}
			break;

			case 0x1:   //1x分包回读
			switch(x2)
			{
				case 0x0:
					run_cmd_d205(cmd_str_11,x1);
				break;

				case 0x1:
					run_cmd_d205_2(cmd_str_11,Read_time,x1);
				break;

				default:
					break;
			}
			break;

			case 0x2:  // 8x分包回读
			switch(x2)
			{
				case 0x0:	//一次读完
					run_cmd_d205_8x(cmd_str_11);
//					run_cmd_d205(cmd_str_11,0);
				break;

				case 0x1:   //循环回读
					run_cmd_d205_2_8x(cmd_str_11,Read_time);
				break;

				default:
					break;
			}
			break;

			default:
				break;

		}
		break;

		case 0x1:    // 流式读取;x1=0为通过网口回传读数据,x1=1为通过GTH 1x回传数据,x1=2为通过GTH 8x回传数据
// 没有流式读取的功能
		break;

		default:
			break;

	}
#endif
	flag_1x=0;
	flag_tcp=0;
#if 0
	switch(x2)
	{
		case 0x0: //一次读完
			run_cmd_d205(cmd_str_11,x2);
			break;

		case 0x1: //循环回读
			run_cmd_d205_2(cmd_str_11,Read_time,x2);
			break;

		default:
	    	break;
	}
#endif
	return 0;
}

/*****************导出文件数据到本地命令*******************/
int run_cmd_d208(BYTE* name,uint8_t mode)
{
	  int i=0,Status,ret,h=0,time=1;
	  uint8_t sts=0;
	  int64_t size=0,LastPack_Size=0;
	  int br;
	  uint32_t  r_count=0,cmd_len=0;
	  uint32_t  len,MODE=0;
	  uint32_t  buff_r=(void *)(0x90000000);
	  uint32_t  Tx_time=0,Tx_len=0,Tx_lastsize=0;
	  xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,name);
	  switch(mode)
	  {
	  	  case 0:
	  		 MODE=MODE_tcp;
	  		 break;

	  	  case 1:
//	  		 MODE=;
	  	     break;

	  	  default:
			 break;
	  }
	  ret = f_open(&rfile,name, FA_OPEN_EXISTING |FA_READ);
//	  ret = f_open(&rfile,"D", FA_OPEN_EXISTING |FA_READ|FA_WRITE);
	  if (ret != FR_OK)
	  {
			xil_printf("f_open Failed! ret=%d\r\n", ret);
			//cmd_reply_a203_to_a201(pMsg->PackNum,pMsg->HandType,pMsg->HandId,0x10);  // lyh 2023.8.15
			return ret;
	  }
	  size=((f_size(&rfile)%4)==0?f_size(&rfile):(f_size(&rfile)/4+1)*4);
	  len=size;
	  if(size>Read_Packet_Size1)
	  {
		  time=size/Read_Packet_Size1+1;
		  len=Read_Packet_Size1;
		  LastPack_Size=((size%Read_Packet_Size1)/4+1)*4;
		  if((size%Read_Packet_Size1)==0)
		  {
			  time=size/Read_Packet_Size1;
			  len=Read_Packet_Size1;
			  LastPack_Size=Read_Packet_Size1;
		  }
	  }
	  //告诉fpga切换模式
	  DestinationBuffer_1[0]=DUMMY;
	  DestinationBuffer_1[1]=MODE;
//	  DestinationBuffer_1[1]=MODE_8x;
	  ret = TxSend(DestinationBuffer_1,8);
	  if (ret != XST_SUCCESS)
	  {
		  xil_printf("TxSend Failed! ret=%d\r\n", ret);
		  return ret;
	  }

//开始读取
	  while(1)
	  {
		  	ret = f_read1(
						&rfile,
						buff_r,
						len,
						&br
			);
			if (ret != FR_OK)
			{
					xil_printf("f_read Failed! ret=%d\r\n", ret);
					return ret;
			}
			r_count++;
			Internal_count(&len,&Tx_time,&Tx_lastsize,&Tx_len);
			for(int k=0;k<Tx_time;k++)
			{
				if(k==Tx_time-1)
				{
					Tx_len=Tx_lastsize;
				}
				usleep(10000);
				//sfifo发送长度
				DestinationBuffer_1[0]=Tx_len;
				DestinationBuffer_1[1]=buff_r;
	//			XLLFIFO_SysInit();
	//			Xil_L1DCacheFlush();

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
				buff_r+=Tx_len/2;
			}
			buff_r=(void *)(0x90000000);
			for(i=0;i<NHC_NUM;i++)
			{
					while (nhc_queue_ept(i) == 0)
					{
						do {
							sts = nhc_cmd_sts(i);
						}while(sts == 0x01);
					}
			 }
			 if(r_count==time-1)
			 {
					len=LastPack_Size;
			 }

			 if(time==r_count)
			 {
					xil_printf("I/O Read or Write Test Finish!\r\n");
					xil_printf("r_count=%u r_size=%lu\r\n",r_count,f_size(&rfile));
					DestinationBuffer_1[0]=DUMMY;
					DestinationBuffer_1[1]=READ_FINSHED;
					ret = TxSend(DestinationBuffer_1,8);
					if (ret != XST_SUCCESS)
					{
					   xil_printf("TxSend Failed! ret=%d\r\n", ret);
					   return ret;
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
					break;
			}

	 }  //while
	 ret=f_close(&rfile);
	 if (ret != FR_OK)
	 {
		 xil_printf(" f_close Failed! %d\r\n",ret);
		 return ret;
	 }
	 return 0;
}


//******导出文件数据到本地命令*******//
int run_cmd_d207(StructMsg *pMsg)
{
     int temp=0,i=0,x=0,h=0;
	 int Read_mode=0;
	 u16 unicode_u16=0;
	 WCHAR cmd_str_1[1024]={0};
	 BYTE cmd_str_11[100]={0};


     for (x = 0; x < 1024; x++)
     {
      	 unicode_u16=(pMsg->MsgData[i++]|pMsg->MsgData[i++]<<8);
    	 cmd_str_1[x] = ff_uni2oem(unicode_u16,FF_CODE_PAGE);
    	 if(cmd_str_1[x]<=0x7E)
		 {
			 cmd_str_11[h++]=cmd_str_1[x];
		 }
		 else
		 {
			 cmd_str_11[h++]=(cmd_str_1[x]>>8);
			 cmd_str_11[h++]=cmd_str_1[x];
		 }
         if (cmd_str_1[x] == '\0') break;
    }           // 文件路径
    xil_printf("%s %d  %s\r\n", __FUNCTION__, __LINE__,cmd_str_11);
    i=temp+2048;
	Read_mode=CW32(pMsg->MsgData[i+0],pMsg->MsgData[i+1],pMsg->MsgData[i+2],pMsg->MsgData[i+3]);

	switch(Read_mode)
	{
		case 0x0:    // 0:文件数据通过网口tcp导出到本地;1:文件数据通过光纤udp导出到本地
			run_cmd_d208(cmd_str_11,0);
		break;

		case 0x1:

		default:
			break;
	}
	return 0;
}

int run_cmd_f201(StructMsg *pMsg)
{
	int ret=0;
	ret=cmd_reply_health_f201();
	if(ret!=0)
	{
		xil_printf("f201 failed!\r\n");
		return ret;
	}
	return 0;
}

int cmd_reply_health_f201(void)
{
		int Status;
		StructHealthStatus SendStructHealthStatus;
		SendStructHealthStatus.Head = 0x55555555;
		SendStructHealthStatus.SrcId = SRC_ID;
		SendStructHealthStatus.DestId = DEST_ID;
		SendStructHealthStatus.HandType = 0xF2;
		SendStructHealthStatus.HandId = 0x1;
	//	SendStructHealthStatus.PackNum = ;

		Status=Storage_state1(&SendStructHealthStatus.TotalCap,&SendStructHealthStatus.UsedCap,
				&SendStructHealthStatus.RemainCap,&SendStructHealthStatus.FileNum);
		if(Status!=0)
		{
			   xil_printf("Storage state1 failed!\r\n");
			   return -1;
		}

		Storage_state2(&SendStructHealthStatus.WorkStatus,&SendStructHealthStatus.WorkTemp,
				&SendStructHealthStatus.Power,&SendStructHealthStatus.PowerUpNum);

		SendStructHealthStatus.CheckCode = SendStructHealthStatus.TotalCap + \
				SendStructHealthStatus.UsedCap + SendStructHealthStatus.RemainCap + \
				SendStructHealthStatus.FileNum + SendStructHealthStatus.WorkStatus + \
				SendStructHealthStatus.WorkTemp + SendStructHealthStatus.Power + \
				SendStructHealthStatus.PowerUpNum;
		SendStructHealthStatus.Tail = 0xAAAAAAAA;

#if  10   //改变字节顺序
		SendStructHealthStatus.SrcId = SW32(SRC_ID);
		SendStructHealthStatus.DestId = SW32(DEST_ID);
		SendStructHealthStatus.HandType = SW32(0xF2);
		SendStructHealthStatus.HandId = SW32(0x1);
//		SendStructHealthStatus.PackNum

//		SendStructHealthStatus.TotalCap	= SW64(SendStructHealthStatus.TotalCap);
//		SendStructHealthStatus.UsedCap	= SW64(SendStructHealthStatus.UsedCap);
//		SendStructHealthStatus.RemainCap = SW64(SendStructHealthStatus.RemainCap);
		SendStructHealthStatus.TotalCap	= Reverse_u64(SendStructHealthStatus.TotalCap);
		SendStructHealthStatus.UsedCap	= Reverse_u64(SendStructHealthStatus.UsedCap);
		SendStructHealthStatus.RemainCap = Reverse_u64(SendStructHealthStatus.RemainCap);
		SendStructHealthStatus.FileNum= SW32(SendStructHealthStatus.FileNum);

		SendStructHealthStatus.WorkStatus = SW32(SendStructHealthStatus.WorkStatus);
		SendStructHealthStatus.WorkTemp = SW32(SendStructHealthStatus.WorkTemp);
		SendStructHealthStatus.Power = SW32(SendStructHealthStatus.Power);
		SendStructHealthStatus.PowerUpNum = SW32(SendStructHealthStatus.PowerUpNum);

		SendStructHealthStatus.CheckCode = SW32(SendStructHealthStatus.CheckCode);

#endif
		if(flag_1x==1)
		{
			AxiDma.TxBdRing.HasDRE=1;
			Status = XAxiDma_SimpleTransfer(&AxiDma,(UINTPTR)&SendStructHealthStatus,
					sizeof(SendStructHealthStatus), XAXIDMA_DMA_TO_DEVICE);

			if (Status != XST_SUCCESS)
			{
				xil_printf("SimpleTransfer failed!\r\n");
				return XST_FAILURE;
			}
		}
		else if(flag_tcp==1)
		{
			AxiDma1.TxBdRing.HasDRE=1;
			Status = XAxiDma_SimpleTransfer(&AxiDma1,(UINTPTR)&SendStructHealthStatus,
					sizeof(SendStructHealthStatus), XAXIDMA_DMA_TO_DEVICE);

			if (Status != XST_SUCCESS)
			{
				xil_printf("SimpleTransfer failed!\r\n");
				return XST_FAILURE;
			}
		}
#ifdef	STM32_UART
		XUartLite_Send(&UartLite, &SendStructHealthStatus.RemainCap, 8);
#endif
		return 0;
}

void Internal_count(uint32_t *LEN,uint32_t *TIME,uint32_t *LastSize,uint32_t *Length)
{
	  uint32_t len=*LEN;
	  uint32_t time=0,lastSize=0,length=0;
	  if(len>0x1000)
	  {
		  time=len/0x1000+1;
		  length=0x1000;
		  lastSize=((len%0x1000)/4+1)*4;
		  if((len%0x1000)==0)
		  {
			  time=len/0x1000;
			  length=0x1000;
			  lastSize=0x1000;
		  }
	  }
	  else if(len==0x1000)
	  {
		  time=1;
		  lastSize=0x1000;
		  length=0x1000;
	  }
	  *TIME=time;
	  *LastSize=lastSize;
	  *Length=length;
}
//equip:1 slot:9  ->192.168.0.32
//equip:1 slot:11 ->192.168.0.33
//equip:2 slot:9  ->192.168.0.34
//equip:2 slot:11 ->192.168.0.35
int IpSet(int equip,int slot)
{
		int IPADDR=0,ret=0,i=0;
		u32 sendBuffer[3]={0};
		if(equip==0x1 && slot==11)
		{
			IPADDR=0x1;
		}
		else if(equip==0x2 && slot==9)
		{
			IPADDR=0x2;
		}
		else if(equip==0x2 && slot==11)
		{
			IPADDR=0x3;
		}


		switch(IPADDR)
		{
			case 1:
				sendBuffer[0]=0x00000000;
				sendBuffer[1]=0x55AA55AA;
				sendBuffer[2]=0x1;
			break;

			case 2:
				sendBuffer[0]=0x00000000;
				sendBuffer[1]=0x55AA55AA;
				sendBuffer[2]=0x2;
			break;

			case 3:
				sendBuffer[0]=0x00000000;
				sendBuffer[1]=0x55AA55AA;
				sendBuffer[2]=0x3;
			break;

//			case 4:
//				sendBuffer[0]=0x00000000;
//				sendBuffer[1]=0x55AA55AA;
//				sendBuffer[2]=0x4;
//			break;

			default:
				break;
		}

		TxSend(sendBuffer,12);
		return 0;
}
void Reply_REMAIN_SPACE(void)
{
	uint64_t used_Cap=0;
	REMAIN_SPACE=0;
	get_Dir_size("0:",&used_Cap);
	REMAIN_SPACE= TOTAL_CAP-used_Cap-LOSS;
}
