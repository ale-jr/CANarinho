#pragma once

enum class MessageType : uint8_t
{
    Command = 0x0,
    Status = 0x1,
    Event = 0x2,
    Query = 0x3,
    Acknowledge = 0x4,
    HeartBeat = 0x5,
    Alarm = 0x6
};

enum class MessagePriority : uint8_t
{
    Alarm = 0,
    Command = 3,
    Status = 5,
    Heartbeat = 7
};

struct CanMessage
{
    MessagePriority priority;
    uint8_t src;
    uint8_t dst;
    MessageType type;
    uint8_t channel;
    uint8_t payload[8];
    uint8_t payload_len;
};