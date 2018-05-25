#include <stm8s.h>
#include "timer_2.h"
#include "_global.h"

void TIM2_Init(void)
{
  CLK_PeripheralClockConfig(CLK_PERIPHERAL_TIMER2, ENABLE);  //enable timer 2 clk
  TIM2_TimeBaseInit(TIM2_PRESCALER_1, 160);     // 10us
  TIM2_SetCounter(0);
  TIM2_ARRPreloadConfig(DISABLE);
}

void Delayms(u16 _ms) {
  u16 ms_10us = _ms * 100;
  
  TIM2_ARRPreloadConfig(ENABLE);
  TIM2_Cmd(ENABLE);
  while(ms_10us--) {
    while( TIM2_GetFlagStatus(TIM2_FLAG_UPDATE) == RESET );
    TIM2_ClearFlag(TIM2_FLAG_UPDATE);
    feed_wwdg();
  }
  TIM2_Cmd(DISABLE);
}

void Delay10Us(u8 _delay) {
  TIM2_ARRPreloadConfig(ENABLE);
  TIM2_Cmd(ENABLE);
  while(_delay--) {
    while( TIM2_GetFlagStatus(TIM2_FLAG_UPDATE) == RESET );
    TIM2_ClearFlag(TIM2_FLAG_UPDATE);
    feed_wwdg();
  }
  TIM2_Cmd(DISABLE);
}

void Delay_50Us(u16 nTime)
{
  u16 n_10us = nTime * 5;
  
  TIM2_ARRPreloadConfig(ENABLE);
  TIM2_Cmd(ENABLE);
  while(n_10us--) {
    while( TIM2_GetFlagStatus(TIM2_FLAG_UPDATE) == RESET );
    TIM2_ClearFlag(TIM2_FLAG_UPDATE);
    feed_wwdg();
  }
  TIM2_Cmd(DISABLE);
}
