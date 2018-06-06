#ifndef IR_H_
#define IR_H_
#include <stm8s.h> 

typedef struct
{
  uint16_t code                        :16;        // Type of ac
  uint8_t aircondStatus[14];
  uint8_t status_size;
}aircondOp_t;

bool IRInit();
bool AddIR(uint16_t code,uint8_t *arrAirstatus,uint8_t statusLen);
bool SendIR();
bool CheckACK();
extern aircondOp_t lastOp;
#endif // IR_H_