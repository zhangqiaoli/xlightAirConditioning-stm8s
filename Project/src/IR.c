#include "IR.h"
#include "Uart2Dev.h"
#include "timer_2.h"
#include "_global.h"

#define ARC_CMD          2
#define ARC_SUCCESS      0x82
#define ARC_ERROR        0xC2
#define START_SIG        0x16
#define CMD_LEN          16
#define END_SIG          0x08

#define MAX_UART_BUF_SIZE 5
uint8_t uartReceiveDataBuf[MAX_UART_BUF_SIZE];
uint8_t uartDataPtr = 0;
uint8_t uart_step = 0;

aircondOp_t lastOp;
uint8_t needAck=0;

bool IRInit()
{
  // Init serial ports
  uart2_config(19200);
  memset(uartReceiveDataBuf,0x00,sizeof(uartReceiveDataBuf));
  uartDataPtr = 0;
  memset(&lastOp,0x00,sizeof(aircondOp_t));
  return TRUE;
}
bool AddIR(uint16_t code,uint8_t *arrAirstatus,uint8_t statusLen)
{
    lastOp.code = code;
    memcpy(lastOp.aircondStatus,arrAirstatus,statusLen);
    lastOp.status_size = statusLen;
    return TRUE;
}
// 15byte£¬onoff,mode,temp,fanlevel,...
bool SendIR()
{
  if(lastOp.status_size == 0)
  {
    return TRUE;
  }
  uint8_t data[CMD_LEN];
  memset(data,0x00,sizeof(data));
  uint16_t sum = 0;
  data[0] = lastOp.code&(0xFF);
  data[1] = lastOp.code>>8;
  if(lastOp.status_size == 4)
  {
    data[2] = lastOp.aircondStatus[1];  
    data[3] = lastOp.aircondStatus[2]-16;
    data[4] = lastOp.aircondStatus[3]; 
    data[5] = 1;
    //data[8] = 0x0F;
    data[12] = 0x20 | lastOp.aircondStatus[0];
  }
  else if(lastOp.status_size>4)
  {
    memcpy(&data[2],lastOp.aircondStatus,lastOp.status_size);
  }
  else
  {
    return FALSE;
  }
  Uart2SendByte(0x00);
  Delayms(50);
  Uart2SendByte(START_SIG);
  sum += START_SIG;
  Uart2SendByte(ARC_CMD);
  sum += ARC_CMD;
  Uart2SendByte(CMD_LEN);
  sum += CMD_LEN;
  for(uint8_t i=0;i<CMD_LEN;i++)
  {
    sum += data[i];
    Uart2SendByte(data[i]);
  }
  uint8_t checksum = 0;
  checksum = sum%256;
  Uart2SendByte(checksum);
  Uart2SendByte(END_SIG);
  lastOp.status_size = 0;
  return TRUE;
}

bool CheckACK()
{
  bool ret = FALSE;
  if(uartDataPtr>=2 )
  {
    if(uartReceiveDataBuf[1] == ARC_SUCCESS)
    {
      ret = TRUE;
      gConfig.aircondCode = lastOp.code;
      memcpy(&gConfig.aircondStatus,&lastOp.aircondStatus,sizeof(gConfig.aircondStatus));
      gIsStatusChanged = TRUE;
    }
    else if(uartReceiveDataBuf[1] == ARC_ERROR)
    {
    }
  }
  return ret;
}

#define UART_STEP_WAIT_HEAD      0
#define UART_STEP_WAIT_CMD       1
#define UART_STEP_WAIT_LEN       2
#define UART_STEP_WAIT_PAYL      3
#define UART_STEP_WAIT_CHECKSUM  4
#define UART_STEP_WAIT_END       5
#define MESSAGE_HEAD       0x16

INTERRUPT_HANDLER(UART2_RX_IRQHandler, 21)
{
  /* In order to detect unexpected events during development,
  it is recommended to set a breakpoint on the following instruction.
  */
  u8 data;
  if( UART2_GetITStatus(UART2_IT_RXNE) == SET ) {
    if(uartDataPtr >= MAX_UART_BUF_SIZE)
    {
      uart_step = UART_STEP_WAIT_HEAD;
      uartDataPtr = 0;
    }
    data = UART2_ReceiveData8();
    switch( uart_step ) {
    case UART_STEP_WAIT_HEAD:
      if( data == MESSAGE_HEAD ) 
      {
        uartDataPtr = 0;
        uart_step = UART_STEP_WAIT_CMD;
        uartReceiveDataBuf[uartDataPtr++] = data;
      }
      break;
    case UART_STEP_WAIT_CMD:
      if( data > 0 )
      {
        uart_step = UART_STEP_WAIT_LEN;
        uartReceiveDataBuf[uartDataPtr++] = data;
      }
      else
      {
        uartDataPtr = 0;
        uart_step = UART_STEP_WAIT_HEAD;
      }
      break;
    case UART_STEP_WAIT_LEN:
      if( data > 1 && data < MAX_UART_BUF_SIZE ) {
        uartReceiveDataBuf[uartDataPtr++] = data;
        uart_step = UART_STEP_WAIT_PAYL;
      } else if(data == 0) {
        uartReceiveDataBuf[uartDataPtr++] = data;
        uart_step = UART_STEP_WAIT_CHECKSUM;
      }else {
        uartDataPtr = 0;
        uart_step = UART_STEP_WAIT_HEAD;
      }
      break;
    case UART_STEP_WAIT_PAYL:   
      uartReceiveDataBuf[uartDataPtr++] = data;    
      if( uartDataPtr == uartReceiveDataBuf[2]+UART_STEP_WAIT_PAYL ) uart_step = UART_STEP_WAIT_CHECKSUM;
      break;
    case UART_STEP_WAIT_CHECKSUM:
      uartReceiveDataBuf[uartDataPtr++] = data;
      uart_step = UART_STEP_WAIT_END;
      break;
    case UART_STEP_WAIT_END:
      uartReceiveDataBuf[uartDataPtr++] = data;
      uart_step = UART_STEP_WAIT_HEAD;
      CheckACK();
      uartDataPtr = 0;
      break;
    default:
      uartDataPtr = 0;
      uart_step = UART_STEP_WAIT_HEAD;
      break;
    }
    UART2_ClearITPendingBit(UART2_IT_RXNE);
  }
}
