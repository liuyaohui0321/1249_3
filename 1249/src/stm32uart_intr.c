/******************************************************************************
*
* Copyright (C) 2002 - 2015 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/
/******************************************************************************/
/**
*
* @file xuartlite_intr_example.c
*
* This file contains a design example using the UartLite driver (XUartLite) and
* hardware device using the interrupt mode.
*
* @note
*
* The user must provide a physical loopback such that data which is
* transmitted will be received.
*
* MODIFICATION HISTORY:
* <pre>
* Ver   Who  Date     Changes
* ----- ---- -------- -----------------------------------------------
* 1.00a jhl  02/13/02 First release
* 1.00b rpm  10/01/03 Made XIntc declaration global
* 1.00b sv   06/09/05 Minor changes to comply to Doxygen and coding guidelines
* 2.00a ktn  10/20/09 Updated to use HAL Processor APIs and minor changes
*		      for coding guidelnes.
* 3.2   ms   01/23/17 Added xil_printf statement in main function to
*                     ensure that "Successfully ran" and "Failed" strings
*                     are available in all examples. This is a fix for
*                     CR-965028.
* </pre>
******************************************************************************/


/***************************** Include Files *********************************/


#include "xaxidma.h"
#include "xparameters.h"
#include "xil_exception.h"
#include "xdebug.h"
#include "xuartlite.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "simple_dma.h"
#include "cmd.h"
#include "xuartlite_l.h"
#include "Ring_Buffer/ringbuffer_u8.h"
#ifdef XPAR_UARTNS550_0_BASEADDR
#include "xuartns550_l.h"       /* to use uartns550 */
#endif


#ifdef XPAR_INTC_0_DEVICE_ID
#include "xintc.h"
#else
 #include "xscugic.h"
#endif

/************************** Constant Definitions *****************************/
/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are defined here such that a user can easily
 * change all the needed parameters in one place.
 */
#define UARTLITE_DEVICE_ID      XPAR_UARTLITE_2_DEVICE_ID   //XPAR_UARTLITE_2_DEVICE_ID
#define INTC_DEVICE_ID          XPAR_INTC_0_DEVICE_ID
#define UARTLITE_INT_IRQ_ID     XPAR_INTC_0_UARTLITE_2_VEC_ID

#define INTC		XIntc
#define INTC_HANDLER	XIntc_InterruptHandler
/*
 * The following constant controls the length of the buffers to be sent
 * and received with the UartLite device.
 */
#define TEST_BUFFER_SIZE        100
#define RX_NOEMPTY XUL_SR_RX_FIFO_VALID_DATA // 接收 FIFO 非空
#define UART_TX_BUFFER_BASE		(0x80000000 + 0x00900000)
u8 *UartRxBufferPtr;
INTC Intc;
extern uint8_t FinishFLAG;
extern uint32_t slotNum;
extern uint32_t equipNum;
extern uint32_t temper_power;
XUartLite UartLite;            /* The instance of the UartLite Device */

#define  buffersize   64
u8 UartRxBuffer[buffersize]={0};
u8_ring_buffer_t rb_handler={0};

/************************** Function Prototypes ******************************/

int SetupInterruptSystem1(XUartLite *UartLitePtr);
int SetupIntrUartSystem(INTC * IntcInstancePtr,XUartLite *UartLitePtr, u16 IntrId);

int UartLiteIntr(void)
{
	int Status;
//	UartRxBufferPtr = (u8 *)UART_TX_BUFFER_BASE;
	u8_ring_buffer_init(&rb_handler,UartRxBuffer,buffersize);
	/*
	 * Initialize the UartLite driver so that it's ready to use.
	 */
	Status = XUartLite_Initialize(&UartLite, UARTLITE_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Connect the UartLite to the interrupt subsystem such that interrupts can
	 * occur. This function is application specific.
	 */
	Status = SetupIntrUartSystem(&Intc,&UartLite,UARTLITE_INT_IRQ_ID);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Enable the interrupt of the UartLite so that interrupts will occur.
	 */
	XUartLite_EnableInterrupt(&UartLite);

	/*
	 * Start receiving data before sending it since there is a loopback.
	 */
//	XUartLite_Recv(&UartLite, ReceiveBuffer, TEST_BUFFER_SIZE);
//	while(1);
	return XST_SUCCESS;
}


void uart_handler(void *CallbackRef)//中断处理函数
{
    u8 Read_data;
    u8 header[4] = {0};
    u8 type[4] = {0};
    u8 result[4] = {0};
//    u8 slot[4] = {0};
    u8 tail[4] = {0};
    u32 isr_status;
	u32 value=0;
    int ret = 0;
    XUartLite *InstancePtr= (XUartLite *)CallbackRef;
    //读取状态寄存器
    isr_status = XUartLite_ReadReg(InstancePtr->RegBaseAddress ,
                                   XUL_STATUS_REG_OFFSET);
    if(isr_status & RX_NOEMPTY)//接收 FIFO 中有数据
	{ 
    	//读取数据
        Read_data=XUartLite_ReadReg(InstancePtr->RegBaseAddress ,
                                    XUL_RX_FIFO_OFFSET);
        u8_ring_buffer_queue_arr(&rb_handler, &Read_data, 1);
//        //发送数据
//        XUartLite_WriteReg(InstancePtr->RegBaseAddress ,
//                           XUL_TX_FIFO_OFFSET, Read_data);
        if(u8_ring_buffer_num_items(&rb_handler) >= 16) // 接收满一帧数据
        {
        	ret = u8_ring_buffer_dequeue_arr(&rb_handler, header, 4);
			if(ret != 4)
			{
				xil_printf("read ring buffer fail \r\n");
				return ret;
			}
			else
			{
				if(0x55555555==CW32(header[0],header[1],header[2],header[3]))
				{
					 ret = u8_ring_buffer_dequeue_arr(&rb_handler, type, 4);
					 if(ret != 4)
					 {
						xil_printf("read ring buffer fail \r\n");
						return ret;
					 }
					 ret = u8_ring_buffer_dequeue_arr(&rb_handler, result, 4);
					 if(ret != 4)
					 {
						xil_printf("read ring buffer fail \r\n");
						return ret;
					 }
					 ret = u8_ring_buffer_dequeue_arr(&rb_handler, tail, 4);
					 if(ret != 4)
					 {
						xil_printf("read ring buffer fail \r\n");
						return ret;
					 }
					 else
					 {
						 if(0xAAAAAAAA==CW32(tail[0],tail[1],tail[2],tail[3]))
						 {
							  if(0xA5==CW32(type[0],type[1],type[2],type[3]))
							  {
								  FinishFLAG=0x1;
								  value=CW32(result[0],result[1],result[2],result[3]);
								  slotNum = (u8)(value >> 16);
								  equipNum = (u8)(value & 0xFFFF);
								  xil_printf("equipNum::%u slotNum:%u\r\n",equipNum,slotNum);
							  }
							  else if(0xC5==CW32(type[0],type[1],type[2],type[3]))
							  {
								  FinishFLAG=0x1;
								  temper_power=CW32(result[0],result[1],result[2],result[3]);
//								  xil_printf("temper_power:%u \r\n",temper_power);
								  u16 power=(u16)(temper_power);
								  u16 temper=(u16)(temper_power>>16);
								  xil_printf("power:%u W  temper:%u .c  \r\n",power,temper);
							  }
						 }
				     }
				}
			}
        }
    }
}
void uart_handler1(void *CallbackRef)//中断处理函数
{
    u8 Read_data;
    u8 header[4] = {0};
    u8 slot[4] = {0};
    u8 tail[4] = {0};
    u32 isr_status;
    int ret = 0;
    XUartLite *InstancePtr= (XUartLite *)CallbackRef;
    //读取状态寄存器
    isr_status = XUartLite_ReadReg(InstancePtr->RegBaseAddress ,
                                   XUL_STATUS_REG_OFFSET);
    if(isr_status & RX_NOEMPTY){ //接收 FIFO 中有数据
    	//读取数据
        Read_data=XUartLite_ReadReg(InstancePtr->RegBaseAddress ,
                                    XUL_RX_FIFO_OFFSET);
        u8_ring_buffer_queue_arr(&rb_handler, &Read_data, 1);
//        //发送数据
//        XUartLite_WriteReg(InstancePtr->RegBaseAddress ,
//                           XUL_TX_FIFO_OFFSET, Read_data);
        if(u8_ring_buffer_num_items(&rb_handler) >= 12) // 接收满一帧数据
        {
        	ret = u8_ring_buffer_dequeue_arr(&rb_handler, header, 4);
			if(ret != 4)
			{
				xil_printf("read ring buffer fail \r\n");
				return ret;
			}
			else
			{
				if(0x55555555==CW32(header[0],header[1],header[2],header[3]))
				{
					 ret = u8_ring_buffer_dequeue_arr(&rb_handler, slot, 4);
					 if(ret != 4)
					 {
						xil_printf("read ring buffer fail \r\n");
						return ret;
					 }
					 ret = u8_ring_buffer_dequeue_arr(&rb_handler, tail, 4);
					 if(ret != 4)
					 {
						xil_printf("read ring buffer fail \r\n");
						return ret;
					 }
					 else
					 {
						 if(0xAAAAAAAA==CW32(tail[0],tail[1],tail[2],tail[3]))
						 {

							  FinishFLAG=0x1;
							  slotNum=CW32(slot[0],slot[1],slot[2],slot[3]);
						 }
				     }
				}
			}
        }
    }
}

int SendToMcu(XUartLite *drive,u8 data)
{
//	XUartLite_WriteReg(drive->RegBaseAddress,
//			                           XUL_TX_FIFO_OFFSET, data);
	XUartLite_SendByte(drive->RegBaseAddress,data);
}

/****************************************************************************/
/**
*
* This function setups the interrupt system such that interrupts can occur
* for the UartLite device. This function is application specific since the
* actual system may or may not have an interrupt controller. The UartLite
* could be directly connected to a processor without an interrupt controller.
* The user should modify this function to fit the application.
*
* @param    UartLitePtr contains a pointer to the instance of the UartLite
*           component which is going to be connected to the interrupt
*           controller.
*
* @return   XST_SUCCESS if successful, otherwise XST_FAILURE.
*
* @note     None.
*
****************************************************************************/
int SetupIntrUartSystem(INTC * IntcInstancePtr,
		 XUartLite *UartLitePtr, u16 IntrId)
{
		int Status;

		/* Initialize the interrupt controller and connect the ISRs */
		do
		{
			Status = XIntc_Initialize(IntcInstancePtr, INTC_DEVICE_ID);
			if (Status != XST_SUCCESS) {

				xil_printf("Failed init intc\r\n");
				return XST_FAILURE;
			}
			usleep(100000);
		} while(Status != XST_SUCCESS);

		/*
		 * Connect a device driver handler that will be called when an interrupt
		 * for the device occurs, the device driver handler performs the
		 * specific interrupt processing for the device.
		 */
		Status = XIntc_Connect(IntcInstancePtr, IntrId,
				   (XInterruptHandler)uart_handler,
				   (void *)UartLitePtr);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		/*
		 * Start the interrupt controller such that interrupts are enabled for
		 * all devices that cause interrupts, specific real mode so that
		 * the UartLite can cause interrupts through the interrupt controller.
		 */
		Status = XIntc_Start(IntcInstancePtr, XIN_REAL_MODE);
		if (Status != XST_SUCCESS) {
			return XST_FAILURE;
		}

		/*
		 * Enable the interrupt for the UartLite device.
		 */
		XIntc_Enable(IntcInstancePtr, IntrId);

		/*
		 * Initialize the exception table.
		 */
		Xil_ExceptionInit();

		/*
		 * Register the interrupt controller handler with the exception table.
		 */
		Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
				 (Xil_ExceptionHandler)XIntc_InterruptHandler,
				 IntcInstancePtr);

		/*
		 * Enable exceptions.
		 */
		Xil_ExceptionEnable();

		return XST_SUCCESS;
}




int SetupInterruptSystem1(XUartLite *UartLitePtr)
{

	int Status;


	/*
	 * Initialize the interrupt controller driver so that it is ready to
	 * use.
	 */
//	Status = XIntc_Initialize(&InterruptController, INTC_DEVICE_ID);
//	if (Status != XST_SUCCESS) {
//		return XST_FAILURE;
//	}


	/*
	 * Connect a device driver handler that will be called when an interrupt
	 * for the device occurs, the device driver handler performs the
	 * specific interrupt processing for the device.
	 */
	Status = XIntc_Connect(&Intc, UARTLITE_INT_IRQ_ID,
			   (XInterruptHandler)uart_handler,
			   (void *)UartLitePtr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Start the interrupt controller such that interrupts are enabled for
	 * all devices that cause interrupts, specific real mode so that
	 * the UartLite can cause interrupts through the interrupt controller.
	 */
	Status = XIntc_Start(&Intc, XIN_REAL_MODE);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Enable the interrupt for the UartLite device.
	 */
	XIntc_Enable(&Intc, UARTLITE_INT_IRQ_ID);

	/*
	 * Initialize the exception table.
	 */
	Xil_ExceptionInit();

	/*
	 * Register the interrupt controller handler with the exception table.
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			 (Xil_ExceptionHandler)XIntc_InterruptHandler,
			 &Intc);

	/*
	 * Enable exceptions.
	 */
	Xil_ExceptionEnable();

	return XST_SUCCESS;
}



#if  0   //old version
int UartLiteIntr(void)
{
	int Status;
	int Index;

	/*
	 * Initialize the UartLite driver so that it's ready to use.
	 */
	Status = XUartLite_Initialize(&UartLite, UARTLITE_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Connect the UartLite to the interrupt subsystem such that interrupts can
	 * occur. This function is application specific.
	 */
	Status = SetupInterruptSystem(&UartLite);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Setup the handlers for the UartLite that will be called from the
	 * interrupt context when data has been sent and received, specify a
	 * pointer to the UartLite driver instance as the callback reference so
	 * that the handlers are able to access the instance data.
	 */
	XUartLite_SetSendHandler(&UartLite, SendHandler, &UartLite);
	XUartLite_SetRecvHandler(&UartLite, RecvHandler, &UartLite);

	/*
	 * Enable the interrupt of the UartLite so that interrupts will occur.
	 */
	XUartLite_EnableInterrupt(&UartLite);

//	/*
//	 * Initialize the send buffer bytes with a pattern to send and the
//	 * the receive buffer bytes to zero to allow the receive data to be
//	 * verified.
//	 */
//	for (Index = 0; Index < TEST_BUFFER_SIZE; Index++) {
//		SendBuffer[Index] = Index;
////		ReceiveBuffer[Index] = 0;
//	}

	/*
	 * Start receiving data before sending it since there is a loopback.
	 */
	XUartLite_Recv(&UartLite, ReceiveBuffer, TEST_BUFFER_SIZE);

//	/*
//	 * Send the buffer using the UartLite.
//	 */
//	XUartLite_Send(&UartLite, SendBuffer, TEST_BUFFER_SIZE);

	/*
	 * Wait for the entire buffer to be received, letting the interrupt
	 * processing work in the background, this function may get locked
	 * up in this loop if the interrupts are not working correctly.
	 */
//	while ((TotalReceivedCount != TEST_BUFFER_SIZE) ||
//		(TotalSentCount != TEST_BUFFER_SIZE)) {
//	}

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* This function is the handler which performs processing to send data to the
* UartLite. It is called from an interrupt context such that the amount of
* processing performed should be minimized. It is called when the transmit
* FIFO of the UartLite is empty and more data can be sent through the UartLite.
*
* This handler provides an example of how to handle data for the UartLite,
* but is application specific.
*
* @param	CallBackRef contains a callback reference from the driver.
*		In this case it is the instance pointer for the UartLite driver.
* @param	EventData contains the number of bytes sent or received for sent
*		and receive events.
*
* @return	None.
*
* @note		None.
*
****************************************************************************/
void SendHandler(void *CallBackRef, unsigned int EventData)
{

}

/****************************************************************************/
/**
*
* This function is the handler which performs processing to receive data from
* the UartLite. It is called from an interrupt context such that the amount of
* processing performed should be minimized.  It is called data is present in
* the receive FIFO of the UartLite such that the data can be retrieved from
* the UartLite. The size of the data present in the FIFO is not known when
* this function is called.
*
* This handler provides an example of how to handle data for the UartLite,
* but is application specific.
*
* @param	CallBackRef contains a callback reference from the driver, in
*		this case it is the instance pointer for the UartLite driver.
* @param	EventData contains the number of bytes sent or received for sent
*		and receive events.
*
* @return	None.
*
* @note		None.
*
****************************************************************************/
void RecvHandler(void *CallBackRef, unsigned int EventData)
{
	int i=0;
	uint32_t g_slot_address=0;
	Xil_DCacheFlushRange((UINTPTR)ReceiveBuffer, TEST_BUFFER_SIZE);
	if(0x55555555 == CW32(ReceiveBuffer[i+0],ReceiveBuffer[i+1],ReceiveBuffer[i+2],ReceiveBuffer[i+3]))
	{
		i+=4;
		g_slot_address = CW32(ReceiveBuffer[i+0],ReceiveBuffer[i+1],ReceiveBuffer[i+2],ReceiveBuffer[i+3]);
		//锟斤拷锟斤拷锟竭硷拷
	}
}

/****************************************************************************/
/**
*
* This function setups the interrupt system such that interrupts can occur
* for the UartLite device. This function is application specific since the
* actual system may or may not have an interrupt controller. The UartLite
* could be directly connected to a processor without an interrupt controller.
* The user should modify this function to fit the application.
*
* @param    UartLitePtr contains a pointer to the instance of the UartLite
*           component which is going to be connected to the interrupt
*           controller.
*
* @return   XST_SUCCESS if successful, otherwise XST_FAILURE.
*
* @note     None.
*
****************************************************************************/
int SetupInterruptSystem(XUartLite *UartLitePtr)
{

	int Status;


	/*
	 * Initialize the interrupt controller driver so that it is ready to
	 * use.
	 */
//	Status = XIntc_Initialize(&InterruptController, INTC_DEVICE_ID);
//	if (Status != XST_SUCCESS) {
//		return XST_FAILURE;
//	}


	/*
	 * Connect a device driver handler that will be called when an interrupt
	 * for the device occurs, the device driver handler performs the
	 * specific interrupt processing for the device.
	 */
	Status = XIntc_Connect(&Intc, UARTLITE_INT_IRQ_ID,
			   (XInterruptHandler)XUartLite_InterruptHandler,
			   (void *)UartLitePtr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Start the interrupt controller such that interrupts are enabled for
	 * all devices that cause interrupts, specific real mode so that
	 * the UartLite can cause interrupts through the interrupt controller.
	 */
	Status = XIntc_Start(&Intc, XIN_REAL_MODE);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Enable the interrupt for the UartLite device.
	 */
	XIntc_Enable(&Intc, UARTLITE_INT_IRQ_ID);

	/*
	 * Initialize the exception table.
	 */
	Xil_ExceptionInit();

	/*
	 * Register the interrupt controller handler with the exception table.
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			 (Xil_ExceptionHandler)XIntc_InterruptHandler,
			 &Intc);

	/*
	 * Enable exceptions.
	 */
	Xil_ExceptionEnable();

	return XST_SUCCESS;
}
#endif


