#ifndef __XLI_NODECONFIG_H
#define __XLI_NODECONFIG_H

// Node config protocol
/// MySensor Cmd = C_INTERNAL
/// MySensor Type = I_CONFIG

/// Use MySensor Sensor as Node Config Field (NCF), and payload as config data
#define NCF_QUERY                       0       // Query NCF, payload length = 0 (query) or n (ack
#define NCF_MAP_SENSOR                  1       // Sensor Bitmap, payload length = 2
#define NCF_MAP_FUNC                    2       // Function Bitmap, payload length = 2

#define NCF_DEV_ASSOCIATE               10      // Associate node to device(s), payload length = 2 to 8, a device per uint16_t
#define NCF_DEV_EN_SDTM                 11      // Simple Direct Test Mode flag, payload length = 2
#define NCF_DEV_MAX_NMRT                12      // Max. Node Message Repeat Times, payload length = 2
#define NCF_DEV_SET_SUBID               13      // Set device subid, payload length = 2
#define NCF_DEV_CONFIG_MODE             14      // Put Device into Config Mode, payload length = 2
#define NCF_DEV_SET_RELAY_NODE          15      // Set relay node id & subID, payload length = 2
#define NCF_DEV_SET_RELAY_KEYS          16      // Set relay keys, payload length = 2 to 4

#define NCF_PAN_SET_BTN_1               20      // Set Panel Button Action, payload length = 2
#define NCF_PAN_SET_BTN_2               21      // Set Panel Button Action, payload length = 2
#define NCF_PAN_SET_BTN_3               22      // Set Panel Button Action, payload length = 2
#define NCF_PAN_SET_BTN_4               23      // Set Panel Button Action, payload length = 2

#define NCF_DATA_ALS_RANGE              50      // Lower and upper threshholds of ALS, payload length = 2
#define NCF_DATA_TEMP_RANGE             51      // Tempreture threshholds, payload length = 2
#define NCF_DATA_HUM_RANGE              52      // Humidity threshholds, payload length = 2
#define NCF_DATA_PM25_RANGE             53      // PM2.5 threshholds, payload length = 2
#define NCF_DATA_PIR_RANGE              54      // PIR control brightness (off br, on br), payload length = 2
#define NCF_DATA_FN_SCENARIO            60      // Scenario ID for Remote Fn keys (b1=fn_id, b2=scenario_id), payload length = 2
#define NCF_DATA_FN_HUE                 61      // Hue for Remote Fn keys (b1_7-4=bmDevice, b1_3-0=fn_id, b2-11=Hue), payload length = 12

#define NCF_LEN_DATA_FN_HUE             12

// Key operation style
#define KEY_OP_STYLE_PRESS              'p'      // Normal press, 200ms
#define KEY_OP_STYLE_FAST_PRESS         'f'      // Fast press, 100ms
#define KEY_OP_STYLE_LONG_PRESS         'l'      // Long-press, 500ms
#define KEY_OP_STYLE_VLONG_PRESS        'v'      // Very long press, 2550ms
#define KEY_OP_STYLE_HOLD               'h'      // Hold
#define KEY_OP_STYLE_RELEASE            'r'      // Release
#define KEY_OP_STYLE_DBL_CLICK          'd'      // Double-click

// Key interval delimitor
#define KEY_DELI_NO_PAUSE               '+'      // No delay
#define KEY_DELI_SMALL_PAUSE            ','      // Small pause, 200ms
#define KEY_DELI_NORMAL_PAUSE           '.'      // Normal pause, 600ms
#define KEY_DELI_LONG_PAUSE             '='      // Long pause, 2s
#define KEY_DELI_VLONG_PAUSE            '~'      // Very long pause, 5s
#define KEY_DELI_SAME_TIME              '_'      // Press two keys at the same time

// Panel Button Operation(bit:7,6,5), Object(bit:4,3,2) & Action(bit:1,0)
#define BTN_OP_SHORT_PRESS              0       // Short press
#define BTN_OP_DBL_PRESS                1       // Double press
#define BTN_OP_LONG_HOLD                2       // Long hold
#define BTN_OP_LONG_RELEASE             3       // Long hold-release
#define BTN_OP_VLONG_HOLD               4       // Very long hold
#define BTN_OP_VLONG_RELEASE            5       // Very long hold-release

#define BTN_OBJ_DEFAULT_KEY             0       // corresponding relay key
#define BTN_OBJ_SCAN_KEY_MAP            1       // scan key map and act on keys one by one
#define BTN_OBJ_LOOP_KEY_MAP            2       // get one key from key map, act on it, and move to the next key

#define BTN_ACT_TOGGLE                  0       // toggle relay key
#define BTN_ACT_ON                      1       // turn on relay key
#define BTN_ACT_OFF                     2       // turn off relay key

typedef enum
{
  sensorDHT             = ((uint16_t)0x0001),
  sensorALS             = ((uint16_t)0x0002),
  sensorMIC             = ((uint16_t)0x0004),
  sensorVIBRATION       = ((uint16_t)0x0008),
  sensorPIR             = ((uint16_t)0x0010),
  sensorSMOKE           = ((uint16_t)0x0020),
  sensorGAS             = ((uint16_t)0x0040),
  sensorDUST            = ((uint16_t)0x0080),
  sensorLEAK            = ((uint16_t)0x0100),
  sensorBEAT            = ((uint16_t)0x0200),
  sensorIRKey           = ((uint16_t)0x8000),
  sensor_All            = ((uint8_t)0xFFFF)
}sensors_bit_t;

typedef enum
{
  controlPIR            = ((uint16_t)0x0001),   // Use PIR to control device on / off
  controlALS            = ((uint16_t)0x0002),   // Use ALS threshold to control device on / off
  constALS              = ((uint16_t)0x0004),   // Constant brightness level
  constTEMP             = ((uint16_t)0x0008),   // Constant tempreture
  constHUM              = ((uint16_t)0x0010),   // Constant humidity
  constPM25             = ((uint16_t)0x0020),   // Constant air quality
  constMIC              = ((uint16_t)0x0040),   // Use MIC threshold to control device on / off
  func_All              = ((uint8_t)0xFFFF)
}functions_bit_t;

#endif /* __XLI_NODECONFIG_H */
