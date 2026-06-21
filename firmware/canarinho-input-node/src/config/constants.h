#pragma once
#include <Arduino.h>

static constexpr uint8_t BROADCAST_NODE_ID = 255;
static constexpr uint8_t GATEWAY_NODE_ID = 254;
static constexpr uint8_t NODE_ID = 1;
static constexpr bool DEBUG_SERIAL = true;
static constexpr bool ENABLE_DEBUG_LOOPBACK = true;

static constexpr gpio_num_t CAN_TX_PIN = (gpio_num_t)0;
static constexpr gpio_num_t CAN_RX_PIN = (gpio_num_t)1;
static constexpr uint32_t SERIAL_BAUD = 2000000;

static constexpr unsigned long HEARTBEAT_INTERVAL_MS = 30000;
static constexpr unsigned long HEARTBEAT_JITTER_MS = NODE_ID * 150;