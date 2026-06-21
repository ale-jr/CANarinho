#include <Arduino.h>
#include "driver/twai.h"
#include "can_tx.h"
#include "../config/constants.h"
#include "./debug_serial.h"
#include "./node/node.h"

uint32_t build_can_id(const CanMessage &msg)
{
    return ((static_cast<uint32_t>(msg.priority) & 0x07) << 26) |
           ((msg.src & 0xFF) << 18) |
           ((msg.dst & 0xFF) << 10) |
           ((static_cast<uint32_t>(msg.type) & 0x0F) << 6) |
           (msg.channel & 0x3F);
}

bool send_message(const CanMessage &msg)
{
    if (ENABLE_DEBUG_LOOPBACK)
    {
        handle_message(const_cast<CanMessage &>(msg));
    }

    twai_message_t frame{};

    frame.extd = 1;
    frame.identifier = build_can_id(msg);

    frame.data_length_code = msg.payload_len;

    memcpy(
        frame.data,
        msg.payload,
        msg.payload_len);
    debug_print_can_frame("[NODE] send message", frame);

    esp_err_t result = twai_transmit(&frame, pdMS_TO_TICKS(100));
    debug_print_text(String(result).c_str());
    return result == ESP_OK;
}

unsigned long next_heartbeat_ms = 0;

bool send_heartbeat()
{
    next_heartbeat_ms = millis() + HEARTBEAT_INTERVAL_MS;

    CanMessage message = {};
    message.priority = MessagePriority::Heartbeat;
    message.src = NODE_ID;
    message.dst = BROADCAST_NODE_ID;
    message.type = MessageType::HeartBeat;
    message.channel = 0;
    message.payload_len = 0;

    return send_message(message);
}

bool send_event(const EventMessage &event)
{
    CanMessage message = {};

    message.src = NODE_ID;
    message.type = MessageType::Event;
    message.priority = MessagePriority::Command;

    message.channel = event.channel;
    message.dst = event.dst;

    message.payload_len = event.payload_len;
    memcpy(
        message.payload,
        event.payload,
        event.payload_len);

    return send_message(message);
}

bool send_command(const CommandMessage &command)
{
    CanMessage message = {};

    message.src = NODE_ID;
    message.type = MessageType::Command;
    message.priority = MessagePriority::Command;

    message.channel = command.channel;
    message.dst = command.dst;

    message.payload_len = command.payload_len;
    memcpy(
        message.payload,
        command.payload,
        command.payload_len);

    return send_message(message);
}

bool send_status(const StatusMessage &status)
{
    CanMessage message = {};

    message.src = NODE_ID;
    message.type = MessageType::Status;
    message.priority = MessagePriority::Status;

    message.dst = BROADCAST_NODE_ID;

    message.channel = status.channel;
    message.payload_len = status.payload_len;
    memcpy(
        message.payload,
        status.payload,
        status.payload_len);

    return send_message(message);
}
