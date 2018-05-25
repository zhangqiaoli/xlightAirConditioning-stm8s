#ifndef __TIMER_TWO_H
#define __TIMER_TWO_H

// 10us interupt
void TIM2_Init(void);
void Delayms(u16 _ms);
void Delay10Us(u8 _delay);
void Delay_50Us(u16 nTime);

#endif // __TIMER_TWO_H