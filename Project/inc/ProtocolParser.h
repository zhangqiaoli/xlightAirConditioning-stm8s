#ifndef __PROTOCOL_PARSER_H
#define __PROTOCOL_PARSER_H

#include "_global.h"

extern uint8_t bMsgReady;
extern uint16_t delaySendTick;
extern uint8_t delaySend;

uint8_t ParseProtocol();
void build(uint8_t _destination, uint8_t _sensor, uint8_t _command, uint8_t _type, bool _enableAck, bool _isAck);
void Msg_NodeConfigAck(uint8_t _to, uint8_t _ncf);
void Msg_NodeConfigData(uint8_t _to);
void Msg_RequestNodeID();
void Msg_Presentation();
void Msg_DevOnOff(uint8_t _to);
void Msg_DevStatus(uint8_t _to);
void Msg_Relay_KeyMap(uint8_t _to);
void Msg_Relay_Ack(uint8_t _to, uint8_t _type, uint8_t _state);

void MsgScanner_ProbeAck();
void MsgScanner_ConfigAck(uint8_t offset,uint8_t cfglen,bool _isStart); 
bool ProcessOutputCfgMsg();

#ifdef EN_SENSOR_ALS
void Msg_SenALS(uint8_t _value);
#endif

#ifdef EN_SENSOR_ALS
void Msg_SenMIC(uint16_t _value);
#endif

#ifdef EN_SENSOR_PIR
void Msg_SenPIR(bool _sw);
#endif

#ifdef EN_SENSOR_IRKEY
void Msg_SenIRKey(uint8_t _sw);
#endif

#ifdef EN_SENSOR_PM25
void Msg_SenPM25(uint16_t _value);
#endif

#ifdef EN_SENSOR_DHT
void Msg_SenDHT(s16 dht_t,s16 dht_h,u8 type);
#endif

#ifdef MULTI_SENSOR
void Msg_SenTemHum(s16 tem,s16 hum);
void Msg_SenAirQuality(uint16_t pm25,uint16_t pm10,uint16_t tvoc,uint16_t ch2o,uint16_t co2);
#endif

#endif /* __PROTOCOL_PARSER_H */