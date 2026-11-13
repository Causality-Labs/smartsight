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
    uint16_t pan_cdeg;
    uint16_t tilt_cdeg;
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
    ss_header_t header;
    uint32_t timestamp_ms;
    uint16_t pan_cdeg;
    uint16_t tilt_cdeg;
    uint16_t status;
    uint16_t reserved;
} ss_tel_t;

#endif /* SMARTSIGHT_MESSAGE_H */
