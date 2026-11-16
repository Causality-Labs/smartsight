#ifndef SMARTSIGHT_MESSAGE_H
#define SMARTSIGHT_MESSAGE_H

#include <stdint.h>

#define SS_PROTOCOL_VERSION 0x01

#define SS_OP_MOVE 0x00
#define SS_OP_CTRL 0x01
#define SS_OP_TELEMETRY 0x80

#define SS_CTRL_ENABLE 0x01
#define SS_CTRL_DISABLE 0x02
#define SS_CTRL_HOME 0x03

#define SS_STATUS_ENABLED (1u << 0)
#define SS_STATUS_SERVO_ERROR (1u << 1)

typedef struct
{
    uint8_t version;
    uint8_t opcode;
    uint16_t sequence;
} ss_header_t;

typedef struct
{
    int32_t pan_cdeg;
    int32_t tilt_cdeg;
} ss_position_payload_t;

typedef struct
{
    uint8_t ctrl;
    uint8_t reserved_0;
    uint16_t reserved_1;
} ss_ctrl_payload_t;

typedef struct
{
    ss_header_t header;
    union
    {
        ss_position_payload_t position;
        ss_ctrl_payload_t ctrl;
    } payload;
} ss_cmd_t;

typedef struct
{
    ss_header_t header;// 4
    uint32_t timestamp_ms; // 4
    int32_t pan_cdeg; // 4
    int32_t tilt_cdeg; //4
    uint16_t status; //2
    uint16_t reserved; // 2
} ss_tel_t;

_Static_assert(sizeof(ss_header_t) == 4,
               "ss_header_t does not match spec");
_Static_assert(sizeof(ss_position_payload_t) == 8,
               "ss_position_payload_t does not match spec");
_Static_assert(sizeof(ss_ctrl_payload_t) == 4,
              "ss_ctrl_payload_t does not match spec");
_Static_assert(sizeof(ss_cmd_t) == 12,
               "ss_cmd_t does not match spec");
_Static_assert(sizeof(ss_tel_t) == 20,
               "ss_tel_t does not match spec");


#endif /* SMARTSIGHT_MESSAGE_H */
