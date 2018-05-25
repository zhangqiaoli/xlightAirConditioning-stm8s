#include "_global.h"
#include "rf24l01.h"
#include "MyMessage.h"
#include "xliNodeConfig.h"
#include "ProtocolParser.h"
#include "timer_4.h"
#include "timer_2.h"
#include "Uart2Dev.h"
#include "IR.h"
//#include "IR.h"
/*
License: MIT

Auther: Baoshi Sun
Email: bs.sun@datatellit.com, bs.sun@uwaterloo.ca
Github: https://github.com/sunbaoshi1975
Please visit xlight.ca for product details

RF24L01 connector pinout:
GND    VCC
CE     CSN
SCK    MOSI
MISO   IRQ

Connections:
  PC3 -> CE
  PC4 -> CSN
  PC7 -> MISO
  PC6 -> MOSI
  PC5 -> SCK
  PC2 -> IRQ

*/

#ifdef TEST
void testio()
{
  GPIO_Init(GPIOB , GPIO_PIN_5 , GPIO_MODE_OUT_PP_LOW_SLOW);
  GPIO_Init(GPIOB , GPIO_PIN_4 , GPIO_MODE_OUT_PP_LOW_SLOW);
  GPIO_Init(GPIOB , GPIO_PIN_3 , GPIO_MODE_OUT_PP_LOW_SLOW);
  GPIO_Init(GPIOB , GPIO_PIN_2 , GPIO_MODE_OUT_PP_LOW_SLOW);
  GPIO_Init(GPIOB , GPIO_PIN_1 , GPIO_MODE_OUT_PP_LOW_SLOW);
  GPIO_Init(GPIOD , GPIO_PIN_1 , GPIO_MODE_OUT_PP_LOW_SLOW);
  GPIO_Init(GPIOD , GPIO_PIN_7 , GPIO_MODE_OUT_PP_LOW_SLOW);
}
#endif

// Starting Flash block number of backup config
#define BACKUP_CONFIG_BLOCK_NUM         2
#define BACKUP_CONFIG_ADDRESS           (FLASH_DATA_START_PHYSICAL_ADDRESS + BACKUP_CONFIG_BLOCK_NUM * FLASH_BLOCK_SIZE)
#define STATUS_DATA_NUM                 4
#define STATUS_DATA_ADDRESS             (FLASH_DATA_START_PHYSICAL_ADDRESS + STATUS_DATA_NUM * FLASH_BLOCK_SIZE)

// RF channel for the sensor net, 0-127
#define RF24_CHANNEL	   		100

// Window Watchdog
// Uncomment this line if in debug mode
#define DEBUG_NO_WWDG
#define WWDG_COUNTER                    0x7f
#define WWDG_WINDOW                     0x77
#define DEBUG_LOG
// System Startup Status
#define SYS_INIT                        0
#define SYS_RESET                       1
#define SYS_WAIT_NODEID                 2
#define SYS_WAIT_PRESENTED              3
#define SYS_RUNNING                     5

// Keep alive message interval, around 6 seconds
#define RTE_TM_KEEP_ALIVE               1500   // about 15s (1500 * 10ms)
#define MAX_RF_FAILED_TIME              10     // Reset RF module when reach max failed times of sending

// Sensor reading duration
#define SEN_MAX_SEND_INTERVAL           6000   // about 60s (6000 * 10ms)

// For Gu'an Demo Classroom
#define ONOFF_RESET_TIMES               5     // on / off times to reset device, regular value is 3

#define RAPID_PRESENTATION                     // Don't wait for presentation-ack
#define REGISTER_RESET_TIMES            30     // default 5, super large value for show only to avoid ID mess


// Unique ID
#if defined(STM8S105) || defined(STM8S005) || defined(STM8AF626x)
  #define     UNIQUE_ID_ADDRESS         (0x48CD)
#endif
#if defined(STM8S103) || defined(STM8S003) ||  defined(STM8S903)
  #define     UNIQUE_ID_ADDRESS         (0x4865)
#endif

const UC RF24_BASE_RADIO_ID[ADDRESS_WIDTH] = {0x00,0x54,0x49,0x54,0x44};

// Public variables
Config_t gConfig;
MyMessage_t sndMsg, rcvMsg;
uint8_t *psndMsg = (uint8_t *)&sndMsg;
uint8_t *prcvMsg = (uint8_t *)&rcvMsg;
bool gIsChanged = FALSE;
bool gNeedSaveBackup = FALSE;
bool gIsStatusChanged = FALSE;
bool gResetRF = FALSE;
bool gResetNode = FALSE;
uint8_t _uniqueID[UNIQUE_ID_LEN];

// Moudle variables
uint8_t mStatus = SYS_INIT;
bool mGotNodeID = FALSE;
uint8_t mutex = 0;

// Keep Alive Timer
uint16_t mTimerKeepAlive = 0;
uint8_t m_cntRFReset = 0;
uint8_t m_cntRFSendFailed = 0;
// avoid flash write operator reentry
uint8_t flashWritting = 0;

void printlog(uint8_t *pBuf)
{
#ifdef DEBUG_LOG
  Uart2SendString(pBuf);
#endif
}

// Initialize Window Watchdog
void wwdg_init() {
#ifndef DEBUG_NO_WWDG  
  WWDG_Init(WWDG_COUNTER, WWDG_WINDOW);
#endif  
}

// Feed the Window Watchdog
void feed_wwdg(void) {
#ifndef DEBUG_NO_WWDG    
  uint8_t cntValue = WWDG_GetCounter() & WWDG_COUNTER;
  if( cntValue < WWDG_WINDOW ) {
    WWDG_SetCounter(WWDG_COUNTER);
  }
#endif  
}


int8_t wait_flashflag_status(uint8_t flag,uint8_t status)
{
    uint16_t timeout = 60000;
    while( FLASH_GetFlagStatus(flag)== status && timeout--);
    if(!timeout) 
    {
      printlog("timeout!");
      return 1;
    }
    return 0;
}


void Flash_ReadBuf(uint32_t Address, uint8_t *Buffer, uint16_t Length) {
  assert_param(IS_FLASH_ADDRESS_OK(Address));
  assert_param(IS_FLASH_ADDRESS_OK(Address+Length));
  
  for( uint16_t i = 0; i < Length; i++ ) {
    Buffer[i] = FLASH_ReadByte(Address+i);
  }
}

bool Flash_WriteBuf(uint32_t Address, uint8_t *Buffer, uint16_t Length) {
  assert_param(IS_FLASH_ADDRESS_OK(Address));
  assert_param(IS_FLASH_ADDRESS_OK(Address+Length));
  if(flashWritting == 1)
  {
    return FALSE;
  }
  flashWritting = 1;
  // Init Flash Read & Write
  FLASH_SetProgrammingTime(FLASH_PROGRAMTIME_STANDARD);
  FLASH_Unlock(FLASH_MEMTYPE_DATA);
  //while (FLASH_GetFlagStatus(FLASH_FLAG_DUL) == RESET);
  if(wait_flashflag_status(FLASH_FLAG_DUL,RESET)) return FALSE;
  
  // Write byte by byte
  bool rc = TRUE;
  uint8_t bytVerify, bytAttmpts;
  for( uint16_t i = 0; i < Length; i++ ) {
    bytAttmpts = 0;
    while(++bytAttmpts <= 3) {
      FLASH_ProgramByte(Address+i, Buffer[i]);
      FLASH_WaitForLastOperation(FLASH_MEMTYPE_DATA);
      
      // Read and verify the byte we just wrote
      bytVerify = FLASH_ReadByte(Address+i);
      if( bytVerify == Buffer[i] ) break;
    }
    if( bytAttmpts > 3 ) {
      rc = FALSE;
      break;
    }
  }
  FLASH_Lock(FLASH_MEMTYPE_DATA);
  flashWritting = 0;
  return rc;
}
 
bool Flash_WriteDataBlock(uint16_t nStartBlock, uint8_t *Buffer, uint16_t Length) {
  // Init Flash Read & Write
  if(flashWritting == 1) 
  {
    return FALSE;
  }
  flashWritting = 1;
  FLASH_SetProgrammingTime(FLASH_PROGRAMTIME_STANDARD);
  FLASH_Unlock(FLASH_MEMTYPE_DATA);
  //while (FLASH_GetFlagStatus(FLASH_FLAG_DUL) == RESET);
  if(wait_flashflag_status(FLASH_FLAG_DUL,RESET)) return FALSE;
  
  uint8_t WriteBuf[FLASH_BLOCK_SIZE];
  uint16_t nBlockNum = (Length - 1) / FLASH_BLOCK_SIZE + 1;
  for( uint16_t block = nStartBlock; block < nStartBlock + nBlockNum; block++ ) {
    memset(WriteBuf, 0x00, FLASH_BLOCK_SIZE);
    uint8_t maxLen = FLASH_BLOCK_SIZE;
    if(block == nStartBlock + nBlockNum -1)
    {
      maxLen = Length - (nBlockNum -1)*FLASH_BLOCK_SIZE;
    }
    for( uint16_t i = 0; i < maxLen; i++ ) {
      WriteBuf[i] = Buffer[(block - nStartBlock) * FLASH_BLOCK_SIZE + i];
    }
    FLASH_ProgramBlock(block, FLASH_MEMTYPE_DATA, FLASH_PROGRAMMODE_STANDARD, WriteBuf);
    FLASH_WaitForLastOperation(FLASH_MEMTYPE_DATA);
  }
  
  FLASH_Lock(FLASH_MEMTYPE_DATA);
  flashWritting = 0;
  return TRUE;
}

uint8_t *Read_UniqueID(uint8_t *UniqueID, uint16_t Length)  
{
  Flash_ReadBuf(UNIQUE_ID_ADDRESS, UniqueID, Length);
  return UniqueID;
}

bool isIdentityEmpty(const UC *pId, UC nLen)
{
  for( int i = 0; i < nLen; i++ ) { if(pId[i] > 0) return FALSE; }
  return TRUE;
}

bool isIdentityEqual(const UC *pId1, const UC *pId2, UC nLen)
{
  for( int i = 0; i < nLen; i++ ) { if(pId1[i] != pId2[i]) return FALSE; }
  return TRUE;
}

bool isNodeIdRequired()
{
  return( isIdentityEmpty(gConfig.NetworkID, ADDRESS_WIDTH) || isIdentityEqual(gConfig.NetworkID, RF24_BASE_RADIO_ID, ADDRESS_WIDTH) );
}

// Save config to Flash
void SaveBackupConfig()
{
  if( gNeedSaveBackup ) {
    // Overwrite entire config FLASH
    if(Flash_WriteDataBlock(BACKUP_CONFIG_BLOCK_NUM, (uint8_t *)&gConfig, sizeof(gConfig)))
    {
      gNeedSaveBackup = FALSE;
    }
  }
}

// Save status to Flash
void SaveStatusData()
{
    // Skip the first byte (version)
    uint8_t pData[50] = {0};
    uint16_t nLen = (uint16_t)(&(gConfig.nodeID)) - (uint16_t)(&gConfig);
    memcpy(pData, (uint8_t *)&gConfig, nLen);
    if(Flash_WriteDataBlock(STATUS_DATA_NUM, pData, nLen))
    {
      gIsStatusChanged = FALSE;
    }
}

// Save config to Flash
void SaveConfig()
{
  if( gIsStatusChanged ) {
    // Overwrite only Static & status parameters
    SaveStatusData();
    gIsChanged = TRUE;
  }
#ifdef TEST
  PB3_High;
#endif
  if( gIsChanged ) {
    // Overwrite entire config FLASH
    if( !isNodeIdRequired() ) gNeedSaveBackup = TRUE;
    uint8_t Attmpts = 0;
    while(++Attmpts <= 3) {
      if(Flash_WriteDataBlock(0, (uint8_t *)&gConfig, sizeof(gConfig)))
      {
        gIsStatusChanged = FALSE;
        gIsChanged = FALSE;
        break;
      }
    }
  }
#ifdef TEST
  PB3_Low;
#endif
}

// Initialize Node Address and look forward to being assigned with a valid NodeID by the SmartController
void InitNodeAddress() {
  // Whether has preset node id
  gConfig.nodeID = XLA_PRODUCT_NODEID;
  memcpy(gConfig.NetworkID, RF24_BASE_RADIO_ID, ADDRESS_WIDTH);
}

bool IsConfigInvalid() {
  return( gConfig.version > XLA_VERSION || gConfig.version < XLA_MIN_VER_REQUIREMENT 
       || gConfig.nodeID == 0 || !IS_AC_NODEID(gConfig.nodeID)
       || gConfig.rfPowerLevel > RF24_PA_MAX || gConfig.rfChannel > 127 || gConfig.rfDataRate > RF24_250KBPS );
}

// Load config from Flash
void LoadConfig()
{
  // Load the most recent settings from FLASH
  Flash_ReadBuf(FLASH_DATA_START_PHYSICAL_ADDRESS, (uint8_t *)&gConfig, sizeof(gConfig));
  if( IsConfigInvalid() ) {
      // If config is OK, then try to load config from backup area
      Flash_ReadBuf(BACKUP_CONFIG_ADDRESS, (uint8_t *)&gConfig, sizeof(gConfig));
      if( IsConfigInvalid() ) {
        // If neither valid, then initialize config with default settings
        memset(&gConfig, 0x00, sizeof(gConfig));
        gConfig.version = XLA_VERSION;
        InitNodeAddress();
        gConfig.subID = 0;
        gConfig.type = XLA_PRODUCT_Type;
        gConfig.rptTimes = 1;
        gConfig.rfChannel = RF24_CHANNEL;
        gConfig.rfPowerLevel = RF24_PA_MAX;
        gConfig.rfDataRate = RF24_250KBPS;
      }
      gIsChanged = TRUE;
    } else {
      uint8_t bytVersion;
      Flash_ReadBuf(BACKUP_CONFIG_ADDRESS, (uint8_t *)&bytVersion, sizeof(bytVersion));
      if( bytVersion != gConfig.version ) gNeedSaveBackup = TRUE;
    }
    // Load the most recent status from FLASH
    uint8_t pData[50] = {0};
    uint16_t nLen = (uint16_t)(&(gConfig.nodeID)) - (uint16_t)(&gConfig);
    Flash_ReadBuf(STATUS_DATA_ADDRESS, pData, nLen);
    if(pData[0] >= XLA_MIN_VER_REQUIREMENT && pData[0] <= XLA_VERSION)
    {
      memcpy(&gConfig,pData,nLen);
    }
    // Start ZenSensor
    gConfig.state = 1;
    gConfig.nodeID = XLA_PRODUCT_NODEID;

    gConfig.rfChannel = 99;
#ifdef EN_PANEL_BUTTONS
    if( gConfig.btnAction[0][0].action > 0x0F || gConfig.btnAction[1][0].action > 0x0F ) {
      memset(gConfig.btnAction, 0x00, sizeof(Button_Action_t) * MAX_NUM_BUTTONS);
    }
#endif    
}

void UpdateNodeAddress(uint8_t _tx) {
  memcpy(rx_addr, gConfig.NetworkID, ADDRESS_WIDTH);
  rx_addr[0] = gConfig.nodeID;
  memcpy(tx_addr, gConfig.NetworkID, ADDRESS_WIDTH);
  if( _tx == NODEID_RF_SCANNER ) {
    tx_addr[0] = NODEID_RF_SCANNER;
  } else {
    tx_addr[0] = (isNodeIdRequired() ? BASESERVICE_ADDRESS : NODEID_GATEWAY);
  }
  RF24L01_setup(gConfig.rfChannel, gConfig.rfDataRate, gConfig.rfPowerLevel, BROADCAST_ADDRESS);
}

bool WaitMutex(uint32_t _timeout) {
  while(_timeout--) {
    if( mutex > 0 ) return TRUE;
    feed_wwdg();
  }
  return FALSE;
}

bool NeedUpdateRFAddress(uint8_t _dest) {
  bool rc = FALSE;
  if( sndMsg.header.destination == NODEID_RF_SCANNER && tx_addr[0] != NODEID_RF_SCANNER ) {
    UpdateNodeAddress(NODEID_RF_SCANNER);
    rc = TRUE;
  } else if( sndMsg.header.destination != NODEID_RF_SCANNER && tx_addr[0] != NODEID_GATEWAY ) {
    UpdateNodeAddress(NODEID_GATEWAY);
    rc = TRUE;
  }
  return rc;
}

// reset rf
void ResetRFModule()
{
  if(gResetRF)
  {
    RF24L01_init();
    NRF2401_EnableIRQ();
    UpdateNodeAddress(NODEID_GATEWAY);
    gResetRF=FALSE;
  }
  if(gResetNode)
  {
    mStatus = SYS_RESET;
    gResetNode=FALSE;
  }
}

// Send message and switch back to receive mode
bool SendMyMessage() {
  if( bMsgReady ) {
    
    // Change tx destination if necessary
    NeedUpdateRFAddress(sndMsg.header.destination);
      
    uint8_t lv_tried = 0;
    uint16_t delay;
    while (lv_tried++ <= gConfig.rptTimes ) {
      
      mutex = 0;
      if(RF24L01_set_mode_TX_timeout() == -1) 
        break;
      if(RF24L01_write_payload_timeout(psndMsg, PLOAD_WIDTH) == -1) 
        break;
      WaitMutex(0x1FFFF);
      if (mutex == 1) {
        m_cntRFSendFailed = 0;
        m_cntRFReset = 0;
        break; // sent sccessfully
      } else {
        m_cntRFSendFailed++;
        if( m_cntRFSendFailed >= MAX_RF_FAILED_TIME ) {
          m_cntRFSendFailed = 0;
          m_cntRFReset++;
          if( m_cntRFReset >= 3 ) {
            // Cold Reset
            WWDG->CR = 0x80;       
            m_cntRFReset = 0;
            break;
          } else if( m_cntRFReset >= 2 ) {
            // Reset whole node
            mStatus = SYS_RESET;
            break;
          }

          // Reset RF module
          //RF24L01_DeInit();
          delay = 0x1FFF;
          while(delay--)feed_wwdg();
          RF24L01_init();
          NRF2401_EnableIRQ();
          UpdateNodeAddress(NODEID_GATEWAY);
          continue;
        }
      }
      
      //The transmission failed, Notes: mutex == 2 doesn't mean failed
      //It happens when rx address defers from tx address
      //asm("nop"); //Place a breakpoint here to see memory
      // Repeat the message if necessary
      delay = 0xFFF;
      while(delay--)feed_wwdg();
    }
    
    // Switch back to receive mode
    bMsgReady = 0;
    RF24L01_set_mode_RX();
    
    // Reset Keep Alive Timer
    mTimerKeepAlive = 0;
  }

  return(mutex > 0);
}

void GotNodeID() {
  mGotNodeID = TRUE;
  UpdateNodeAddress(NODEID_GATEWAY);
  gIsChanged = TRUE;
  //SaveConfig();
}

void GotPresented() {
  mStatus = SYS_RUNNING;
  gConfig.swTimes = 0;
  gIsStatusChanged = TRUE;
  //SaveConfig();  
}

bool SayHelloToDevice(bool infinate) {
  uint8_t _count = 0;
  uint8_t _presentCnt = 0;
  bool _doNow = FALSE;

  // Update RF addresses and Setup RF environment
  UpdateNodeAddress(NODEID_GATEWAY);

  while(mStatus < SYS_RUNNING) {
    ////////////rfscanner process///////////////////////////////
    ProcessOutputCfgMsg(); 
    SendMyMessage();
    ResetRFModule();
    SaveConfig();
    ////////////rfscanner process/////////////////////////////// 
    if( _count++ == 0 ) {
      
      if( isNodeIdRequired() ) {
        mStatus = SYS_WAIT_NODEID;
        mGotNodeID = FALSE;
        // Request for NodeID
        Msg_RequestNodeID();
      } else {
        mStatus = SYS_WAIT_PRESENTED;
        // Send Presentation Message
        Msg_Presentation();
        _presentCnt++;
#ifdef RAPID_PRESENTATION
        // Don't wait for ack
        mStatus = SYS_RUNNING;
#endif       
      }
           
      if( !SendMyMessage() ) {
        if( !infinate ) return FALSE;
      } else {
        // Wait response
        uint16_t tick = 0xBFFF;
        while(tick-- && mStatus < SYS_RUNNING) {
          // Feed the Watchdog
          feed_wwdg();
          if( mStatus == SYS_WAIT_NODEID && mGotNodeID ) {
            mStatus = SYS_WAIT_PRESENTED;
            _presentCnt = 0;
            _doNow = TRUE;
            break;
          }
        }
      }
    }

    if( mStatus == SYS_RUNNING ) return TRUE;
    
    // Can't presented for a few times, then try request NodeID again
    // Either because SmartController is off, or changed
    if(  mStatus == SYS_WAIT_PRESENTED && _presentCnt >= REGISTER_RESET_TIMES && REGISTER_RESET_TIMES < 100 ) {
      _presentCnt = 0;
      // Reset RF Address
      InitNodeAddress();
      UpdateNodeAddress(NODEID_GATEWAY);
      mStatus = SYS_WAIT_NODEID;
      _doNow = TRUE;
    }
    
    // Reset switch count
    if( _count >= 10 && gConfig.swTimes > 0 ) {
      gConfig.swTimes = 0;
      gIsStatusChanged = TRUE;
      SaveConfig();
    }
    
    // Feed the Watchdog
    feed_wwdg();

    if( _doNow ) {
      // Send Message Immediately
      _count = 0;
      continue;
    }
    
    // Failed or Timeout, then repeat init-step
    //delay_ms(400);
    mutex = 0;
    WaitMutex(0x1FFFF);
    _count %= 20;  // Every 10 seconds
  }
  
  return TRUE;
}

int main( void ) {
      
  //After reset, the device restarts by default with the HSI clock divided by 8.
  //CLK_DeInit();
  /* High speed internal clock prescaler: 1 */
  CLK_SYSCLKConfig(CLK_PRESCALER_HSIDIV1);  // now: HSI=16M prescale = 1; sysclk = 16M

  // Load config from Flash
  FLASH_DeInit();
  Read_UniqueID(_uniqueID, UNIQUE_ID_LEN);
  LoadConfig();

  // on / off 3 times to reset device
  gConfig.swTimes++;
  if( gConfig.swTimes >= ONOFF_RESET_TIMES ) {
    gConfig.swTimes = 0;
    InitNodeAddress();
  }
  
  // Init Watchdog
  wwdg_init();
  
  gIsStatusChanged = TRUE;
  SaveConfig(); 

  //printlog("start...\r\n");
  // Init timer
  TIM4_10ms_handler = tmrProcess;
  Time4_Init();
  TIM2_Init();
#ifdef TEST
   testio();
#endif
   IRInit();
  while(1) {
    // Go on only if NRF chip is presented
    disableInterrupts();
    gConfig.present = 0;
    RF24L01_init();
    u16 timeoutRFcheck = 0;
    while(!NRF24L01_Check()) {
      if( timeoutRFcheck > 50 ) {
        WWDG->CR = 0x80;
        break;
      }
      feed_wwdg();
    }
    // IRQ
    NRF2401_EnableIRQ();
    // Must establish connection firstly
    SayHelloToDevice(TRUE);  
    while (mStatus == SYS_RUNNING) {
      
      // Feed the Watchdog
      feed_wwdg();
      //WaitMutex(0xFFFFF);  
      //SendIR(25,1,0,26,0);
      // reset rf
      CheckACK();
      if(delaySendTick == 0 && delaySend)
      {
        Msg_DevStatus(NODEID_GATEWAY);
        delaySend = FALSE;
      }
      SendMyMessage();
      
      ResetRFModule();

      SaveConfig();
      
      // ToDo: Check heartbeats
      // mStatus = SYS_RESET, if timeout or received a value 3 times consecutively
    }
  }
}

// Execute timer operations
void tmrProcess() {
  // Tick 
  if(delaySendTick > 0)
  {
    delaySendTick--;
  }
  // Save config into backup area
   SaveBackupConfig();
}


INTERRUPT_HANDLER(EXTI_PORTC_IRQHandler, 5) {
#ifdef TEST
  PD7_High;
#endif
  if(RF24L01_is_data_available()) {
    //Packet was received
    RF24L01_clear_interrupts();
    RF24L01_read_payload(prcvMsg, PLOAD_WIDTH);
    bMsgReady = ParseProtocol();
#ifdef TEST
    PD7_Low;
#endif
    return;
  }
 
  uint8_t sent_info;
  if (sent_info = RF24L01_was_data_sent()) {
    //Packet was sent or max retries reached
    RF24L01_clear_interrupts();
    mutex = sent_info;
#ifdef TEST
    PD7_Low;
#endif    
    return;
  }

   RF24L01_clear_interrupts();
#ifdef TEST
   PD7_Low;
#endif   
}
