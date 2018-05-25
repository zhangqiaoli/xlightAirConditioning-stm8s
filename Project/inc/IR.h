#ifndef IR_H_
#define IR_H_
#include <stm8s.h> 

typedef struct
{
  uint16_t code                        :16;        // Type of ac
  uint8_t aircondStatus[14];
}aircondOp_t;

bool IRInit();
bool SendIR(uint16_t code,uint8_t onoff,uint8_t temp,uint8_t mode,uint8_t fanlevel);
bool CheckACK();
extern aircondOp_t lastOp;
#endif // IR_H_