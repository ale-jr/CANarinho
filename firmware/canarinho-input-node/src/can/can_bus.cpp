#include <Arduino.h>
#include "driver/twai.h"
#include <stdint.h>
#include <string.h>
#include "can_bus.h"
#include "../config/constants.h"
#include "debug_serial.h"
#include "can_parser.h"
#include "can_tx.h"
#include "./node/node.h"

static bool should_accept_msg(uint32_t can_id)
{

    const uint8_t dst = can_dst_from_id(can_id);
    return (dst == NODE_ID) || (dst == BROADCAST_NODE_ID);
}

bool setup_can()
{
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        CAN_TX_PIN,
        CAN_RX_PIN,
        TWAI_MODE_NORMAL);

    g_config.rx_queue_len = 32;
    g_config.tx_queue_len = 16;
    g_config.alerts_enabled = TWAI_ALERT_NONE;
    g_config.clkout_divider = 0;

    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_50KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK)
    {
        return false;
    }

    if (twai_start() != ESP_OK)
    {
        twai_driver_uninstall();
        return false;
    }

    next_heartbeat_ms = millis() + HEARTBEAT_JITTER_MS;

    return true;
}

static void route_message(const CanMessage &msg)
{
    switch (msg.type)
    {
    case MessageType::Query:
        break;
    default:
        break;
    }
}

static void handle_query_message(const CanMessage &msg)
{
    if (msg.channel == 0)
    {
        send_heartbeat();
    }
}

static void process_can_rx()
{
    twai_message_t msg;
    while (twai_receive(&msg, 0) == ESP_OK)
    {
        debug_print_can_frame("[NODE] RX CAN", msg);
        if (!msg.extd)
        {
            debug_print_text("[NODE] drop: non-extended frame");
            continue;
        }

        if (!should_accept_msg(msg.identifier))
        {
            debug_print_text("[NODE] drop: filtered by DST");
            continue;
        }

        CanMessage parsedMessage = parse_can_frame(msg);
        handle_message(parsedMessage);
    }
}

void loop_can()
{
    process_can_rx();

    unsigned long now = millis();
    if (now >= next_heartbeat_ms)
    {
        send_heartbeat();
    }
}
