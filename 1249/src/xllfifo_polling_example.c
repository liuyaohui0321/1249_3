/******************************************************************************
* Copyright (C) 2013 - 2020 Xilinx, Inc.  All rights reserved.
* SPDX-License-Identifier: MIT
******************************************************************************/

/*****************************************************************************/
/**
 *
 * @file XLlFifo_polling_example.c
 * This file demonstrates how to use the Streaming fifo driver on the xilinx AXI
 * Streaming FIFO IP.The AXI4-Stream FIFO core allows memory mapped access to a
 * AXI-Stream interface. The core can be used to interface to AXI Streaming IPs
 * similar to the LogiCORE IP AXI Ethernet core, without having to use full DMA
 * solution.
 *
 * This is the polling example for the FIFO it assumes that at the
 * h/w level FIFO is connected in loopback.In these we write known amount of
 * data to the FIFO and Receive the data and compare with the data transmitted.
 *
 * Note: The TDEST Must be enabled in the H/W design inorder to
 * get correct RDR value.
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date     Changes
 * ----- ---- -------- -------------------------------------------------------
 * 3.00a adk 08/10/2013 initial release CR:727787
 * 5.1   ms  01/23/17   Modified xil_printf statement in main function to
 *                      ensure that "Successfully ran" and "Failed" strings
 *                      are available in all examples. This is a fix for
 *                      CR-965028.
 *       ms  04/05/17   Added tabspace for return statements in functions for
 *                      proper documentation and Modified Comment lines
 *                      to consider it as a documentation block while
 *                      generating doxygen.
 * 5.3  rsp 11/08/18    Modified TxSend to fill SourceBuffer with non-zero
 *                      data otherwise the test can return a false positive
 *                      because DestinationBuffer is initialized with zeros.
 *                      In fact, fixing this exposed a bug in RxReceive and
 *                      caused the test to start failing. According to the
 *                      product guide (pg080) for the AXI4-Stream FIFO, the
 *                      RDFO should be read before reading RLR. Reading RLR
 *                      first will result in the RDFO being reset to zero and
 *                      no data being received.
 * </pre>
 *
 * ***************************************************************************
 */

/***************************** Include Files *********************************/
#include "xparameters.h"
#include "xil_exception.h"
#include "xstreamer.h"
#include "xil_cache.h"
#include "xllfifo.h"
#include "xstatus.h"
#include "xllfifo_drv.h"

#ifdef XPAR_UARTNS550_0_BASEADDR
#include "xuartns550_l.h"       /* to use uartns550 */
#endif

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

#define FIFO_RECV_DEV_ID	   	XPAR_AXI_FIFO_0_DEVICE_ID
#define FIFO_SEND_DEV_ID	   	XPAR_AXI_FIFO_0_DEVICE_ID



#undef DEBUG

/************************** Function Prototypes ******************************/
#ifdef XPAR_UARTNS550_0_BASEADDR
static void Uart550_Setup(void);
#endif

/************************** Variable Definitions *****************************/
/*
 * Device instance definitions
 */
//XLlFifo Send_FifoInstance;
//XLlFifo Recv_FifoInstance;
//
//XLlFifo Send_FifoInstance_1;
//XLlFifo Recv_FifoInstance_1;
XLlFifo Fifo0;
//XLlFifo Fifo1;
int XLlFifoInit(XLlFifo *InstancePtr, u16 DeviceId);
/*****************************************************************************/
/**
*
* Main function
*
* This function is the main entry of the Axi FIFO Polling test.
*
* @param	None
*
* @return
*		- XST_SUCCESS if tests pass
* 		- XST_FAILURE if fails.
*
* @note		None
*
******************************************************************************/
#if 0
int main()
{
	int Status;

	xil_printf("--- Entering main() ---\n\r");

	Status = XLlFifoPollingExample(&FifoInstance, FIFO_DEV_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("Axi Streaming FIFO Polling Example Test Failed\n\r");
		xil_printf("--- Exiting main() ---\n\r");
		return XST_FAILURE;
	}

	xil_printf("Successfully ran Axi Streaming FIFO Polling Example\n\r");
	xil_printf("--- Exiting main() ---\n\r");

	return XST_SUCCESS;
}
#endif
int XLLFIFO_SysInit(void)
{
//	XLlFifoInit(&Send_FifoInstance,FIFO_SEND_DEV_ID);
//	XLlFifoInit(&Recv_FifoInstance,FIFO_SEND_DEV_ID);
//
//	XLlFifoInit(&Send_FifoInstance_1,FIFO_SEND_DEV_ID_1);
//	XLlFifoInit(&Recv_FifoInstance_1,FIFO_RECV_DEV_ID_1);
	XLlFifoInit(&Fifo0,FIFO_SEND_DEV_ID);
	return 0;
}

/*****************************************************************************/
/**
*
* This function demonstrates the usage AXI FIFO
* It does the following:
*       - Set up the output terminal if UART16550 is in the hardware build
*       - Initialize the Axi FIFO Device.
*	- Transmit the data
*	- Receive the data from fifo
*	- Compare the data
*	- Return the result
*
* @param	InstancePtr is a pointer to the instance of the
*		XLlFifo component.
* @param	DeviceId is Device ID of the Axi Fifo Device instance,
*		typically XPAR_<AXI_FIFO_instance>_DEVICE_ID value from
*		xparameters.h.
*
* @return
*		-XST_SUCCESS to indicate success
*		-XST_FAILURE to indicate failure
*
******************************************************************************/
int XLlFifoInit(XLlFifo *InstancePtr, u16 DeviceId)
{
	XLlFifo_Config *Config;
	int Status;
	Status = XST_SUCCESS;

	/* Initial setup for Uart16550 */
#ifdef XPAR_UARTNS550_0_BASEADDR

	Uart550_Setup();

#endif

	/* Initialize the Device Configuration Interface driver */
	Config = XLlFfio_LookupConfig(DeviceId);
	if (!Config) {
		xil_printf("No config found for %d\r\n", DeviceId);
		return XST_FAILURE;
	}

	/*
	 * This is where the virtual address would be used, this example
	 * uses physical address.
	 */
	Status = XLlFifo_CfgInitialize(InstancePtr, Config, Config->BaseAddress);
	if (Status != XST_SUCCESS) {
		xil_printf("Initialization failed\n\r");
		return Status;
	}

	/* Check for the Reset value */
	Status = XLlFifo_Status(InstancePtr);
	XLlFifo_IntClear(InstancePtr,0xffffffff);
	Status = XLlFifo_Status(InstancePtr);
	if(Status != 0x0) {
		xil_printf("\n ERROR : Reset value of ISR0 : 0x%x\t"
			    "Expected : 0x0\n\r",
			    XLlFifo_Status(InstancePtr));
		return XST_FAILURE;
	}
#if 0
	/* Transmit the Data Stream */
	Status = TxSend(InstancePtr, SourceBuffer);
	if (Status != XST_SUCCESS){
		xil_printf("Transmission of Data failed\n\r");
		return XST_FAILURE;
	}

	/* Receive the Data Stream */
	Status = RxReceive(InstancePtr, DestinationBuffer);
	if (Status != XST_SUCCESS){
		xil_printf("Receiving data failed");
		return XST_FAILURE;
	}

	Error = 0;

	/* Compare the data send with the data received */
	xil_printf(" Comparing data ...\n\r");
	for( i=0 ; i<MAX_DATA_BUFFER_SIZE ; i++ ){
		if ( *(SourceBuffer + i) != *(DestinationBuffer + i) ){
			Error = 1;
			break;
		}

	}

	if (Error != 0){
		return XST_FAILURE;
	}
#endif
	return Status;
}

/*****************************************************************************/
/**
*
* TxSend routine, It will send the requested amount of data at the
* specified addr.
*
* @param	InstancePtr is a pointer to the instance of the
*		XLlFifo component.
*
* @param	SourceAddr is the address where the FIFO stars writing
*
* @return
*		-XST_SUCCESS to indicate success
*		-XST_FAILURE to indicate failure
*
* @note		None
*
******************************************************************************/
int TxSend(u32  *SourceAddr,u32 len)
{

	int j;
	XLlFifo *InstancePtr = &Fifo0;

	//xil_printf(" Transmitting Data ... \r\n");

	/* Writing into the FIFO Transmit Port Buffer */
	for (j=0 ; j < len/WORD_SIZE ; j++){
		if( XLlFifo_iTxVacancy(InstancePtr) ){
			XLlFifo_TxPutWord(InstancePtr,
				*(SourceAddr+j));
//			xil_printf("write 0x%x    \n",*(SourceAddr+j));
		}

	}
//	xil_printf("\n\none pack\n\n");
	/* Start Transmission by writing transmission length into the TLR */
	XLlFifo_iTxSetLen(InstancePtr, len);

	/* Check for Transmission completion */
	while( !(XLlFifo_IsTxDone(InstancePtr)) ){

	}

	/* Transmission Complete */
	return XST_SUCCESS;
}

//int TxSend_1(u32  *SourceAddr,u32 len)
//{
//
//	int j;
//	XLlFifo *InstancePtr_1 = &Fifo1;
//
//	//xil_printf(" Transmitting Data ... \r\n");
//
//	/* Writing into the FIFO Transmit Port Buffer */
//	for (j=0 ; j < len/WORD_SIZE ; j++){
//		if( XLlFifo_iTxVacancy(InstancePtr_1) ){
//			XLlFifo_TxPutWord(InstancePtr_1,
//				*(SourceAddr+j));
//		}
//	}
//
//	/* Start Transmission by writing transmission length into the TLR */
//	XLlFifo_iTxSetLen(InstancePtr_1, len);
//
//	/* Check for Transmission completion */
//	while( !(XLlFifo_IsTxDone(InstancePtr_1)) ){
//
//	}
//
//	/* Transmission Complete */
//	return XST_SUCCESS;
//}
/*****************************************************************************/
/**
*
* RxReceive routine.It will receive the data from the FIFO.
*
* @param	InstancePtr is a pointer to the instance of the
*		XLlFifo instance.
*
* @param	DestinationAddr is the address where to copy the received data.
*
* @return
*		-XST_SUCCESS to indicate success
*		-XST_FAILURE to indicate failure
*
* @note		None
*
******************************************************************************/
int RxReceive (u32* DestinationBuffer,u32* len)
{

	int i;
	u32 RxWord;
	u32 ReceiveLength;
	XLlFifo *InstancePtr = &Fifo0;
	if (XLlFifo_iRxOccupancy(InstancePtr) == 0) {
		return XST_FAILURE;
	}
	//xil_printf(" Receiving data ....\n\r");
	/* Read Receive Length */
	ReceiveLength = (XLlFifo_iRxGetLen(InstancePtr))/WORD_SIZE;
	for (i=0; i < ReceiveLength; i++) {
		RxWord = XLlFifo_RxGetWord(InstancePtr);
		DestinationBuffer[i] = RxWord;
//		*((u32 *)(0xA0000000+i*4)) = RxWord;
	}
	//Xil_L1DCacheFlush();
#if 0
	Status = XLlFifo_IsRxDone(InstancePtr);
	if(Status != TRUE){
		xil_printf("Failing in receive complete ... \r\n");
		return XST_FAILURE;
	}
#endif
	*len = ReceiveLength;

	return XST_SUCCESS;
}


//int RxReceive_1 (u32* DestinationBuffer,u32* len)
//{
//
//	int i;
//	u32 RxWord;
//	u32 ReceiveLength;
//	XLlFifo *InstancePtr_1 = &Fifo1;
//
//	if (XLlFifo_iRxOccupancy(InstancePtr_1) == 0) {
//		return XST_FAILURE;
//	}
//	//xil_printf(" Receiving data ....\n\r");
//	/* Read Receive Length */
//	ReceiveLength = (XLlFifo_iRxGetLen(InstancePtr_1))/WORD_SIZE;
//	for (i=0; i < ReceiveLength; i++) {
//		RxWord = XLlFifo_RxGetWord(InstancePtr_1);
//		*(DestinationBuffer+i) = RxWord;
//	}
//#if 0
//	Status = XLlFifo_IsRxDone(InstancePtr_1);
//	if(Status != TRUE){
//		xil_printf("Failing in receive complete ... \r\n");
//		return XST_FAILURE;
//	}
//#endif
//	*len = ReceiveLength;
//
//	return XST_SUCCESS;
//}


#ifdef XPAR_UARTNS550_0_BASEADDR
/*****************************************************************************/
/*
*
* Uart16550 setup routine, need to set baudrate to 9600 and data bits to 8
*
* @param	None
*
* @return	None
*
* @note		None
*
******************************************************************************/
static void Uart550_Setup(void)
{

	XUartNs550_SetBaud(XPAR_UARTNS550_0_BASEADDR,
			XPAR_XUARTNS550_CLOCK_HZ, 9600);

	XUartNs550_SetLineControlReg(XPAR_UARTNS550_0_BASEADDR,
			XUN_LCR_8_DATA_BITS);
}
#endif
