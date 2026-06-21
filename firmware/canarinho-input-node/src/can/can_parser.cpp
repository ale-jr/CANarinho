#include "can_parser.h"

CanMessage parse_can_frame(const twai_message_t &frame)
{
    CanMessage msg{};

    const uint32_t id = frame.identifier;

    msg.priority = can_priority_from_id(id);
    msg.src = can_src_from_id(id);
    msg.dst = can_dst_from_id(id);
    msg.type = can_type_from_id(id);
    msg.channel = can_channel_from_id(id);

    msg.payload_len = frame.data_length_code;

    memcpy(msg.payload, frame.data, msg.payload_len);

    return msg;
}

 bool can_id_is_valid_29(uint32_t can_id)
{
    return (can_id & ~0x1FFFFFFFUL) == 0;
}

 MessagePriority can_priority_from_id(uint32_t can_id)
{
    return static_cast<MessagePriority>((can_id >> 26) & 0x07);
}
 uint8_t can_src_from_id(uint32_t can_id)
{
    return (can_id >> 18) & 0xFF;
}
 uint8_t can_dst_from_id(uint32_t can_id)
{
    return (can_id >> 10) & 0xFF;
}
 MessageType can_type_from_id(uint32_t can_id)
{
    return static_cast<MessageType>((can_id >> 6) & 0x0F);
}
 uint8_t can_channel_from_id(uint32_t can_id)
{
    return can_id & 0x3F;
}