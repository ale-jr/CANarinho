#pragma once

#include "can_message.h"
#include "driver/twai.h"

CanMessage parse_can_frame(
    const twai_message_t &frame);

 uint8_t can_dst_from_id(uint32_t id);
 uint8_t can_src_from_id(uint32_t id);
 MessageType can_type_from_id(uint32_t can_id);
 uint8_t can_channel_from_id(uint32_t id);
 MessagePriority can_priority_from_id(uint32_t can_id);
