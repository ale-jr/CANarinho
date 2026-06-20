#include <Arduino.h>
#include "driver/twai.h"
#include <stdint.h>
#include <string.h>

#define BROADCAST_DST_ID 255
#define NODE_ID 1

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

static constexpr gpio_num_t CAN_TX_PIN = (gpio_num_t)0;
static constexpr gpio_num_t CAN_RX_PIN = (gpio_num_t)1;
static constexpr uint32_t SERIAL_BAUD = 2000000;
static constexpr bool DEBUG_SERIAL = false;

static inline MessagePriority can_priority_from_id(uint32_t can_id)
{
  return static_cast<MessagePriority>((can_id >> 26) & 0x07);
}
static inline uint8_t can_src_from_id(uint32_t can_id)
{
  return (can_id >> 18) & 0xFF;
}
static inline uint8_t can_dst_from_id(uint32_t can_id)
{
  return (can_id >> 10) & 0xFF;
}
static inline MessageType can_type_from_id(uint32_t can_id)
{
  return static_cast<MessageType>((can_id >> 6) & 0x0F);
}
static inline uint8_t can_channel_from_id(uint32_t can_id)
{
  return can_id & 0x3F;
}
static inline bool can_id_is_valid_29(uint32_t can_id)
{
  return (can_id & ~0x1FFFFFFFUL) == 0;
}

static void debug_print_hex_byte(uint8_t value)
{
  if (value < 0x10)
  {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}
static void debug_print_can_frame(const char *prefix, const twai_message_t &msg)
{
  if (!DEBUG_SERIAL)
  {
    return;
  }

  Serial.print(prefix);
  Serial.print(" id=0x");
  Serial.print(msg.identifier, HEX);
  Serial.print(" extd=");
  Serial.print(msg.extd ? 1 : 0);
  Serial.print(" rtr=");
  Serial.print(msg.rtr ? 1 : 0);
  Serial.print(" dlc=");
  Serial.print(msg.data_length_code);
  Serial.print(" data=");

  for (uint8_t i = 0; i < msg.data_length_code; i++)
  {
    if (i > 0)
    {
      Serial.print(' ');
    }
    debug_print_hex_byte(msg.data[i]);
  }

  Serial.println();
}
static void debug_print_text(const char *text)
{
  if (!DEBUG_SERIAL)
  {
    return;
  }

  Serial.println(text);
}
static bool should_accept_msg(uint32_t can_id)
{

  const uint8_t dst = can_dst_from_id(can_id);
  return (dst == NODE_ID) || (dst == BROADCAST_DST_ID);
}

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
  twai_message_t frame{};

  frame.extd = 1;
  frame.identifier = build_can_id(msg);

  frame.data_length_code = msg.payload_len;

  memcpy(
      frame.data,
      msg.payload,
      msg.payload_len);

  return twai_transmit(&frame, pdMS_TO_TICKS(100)) == ESP_OK;
}

static bool send_heartbeat()
{
  return send_message({
      .priority = MessagePriority::Heartbeat,
      .src = NODE_ID,
      .dst = BROADCAST_DST_ID,
      .type = MessageType::HeartBeat,
      .channel = 0,
      .payload_len = 0,
  });
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

enum class ButtonActionType : uint8_t
{
  None,
  SendCommand,
  SendEvent
};

enum class LightAction : uint8_t
{
  Off,
  On,
  Toggle,
  SetLevel,
  ToggleLevel,
  StepUp,
  StepDown
};

struct ButtonAction
{
  ButtonActionType type;

  uint8_t dst;
  uint8_t channel;

  uint8_t action;
  uint8_t value;
};

struct Button
{
  uint8_t gpio;
  uint8_t channel;

  ButtonAction click;
  ButtonAction double_click;
  ButtonAction long_click;
};

Button buttons[] = {
    {.gpio = 5,
     .channel = 1,
     .click = {
         .type = ButtonActionType::SendCommand,
         .dst = 255,
         .channel = 3,
         .action = 1,
         .value = 255},
     .double_click = {
         .type = ButtonActionType::SendEvent,
         .dst = 255,
     }}};

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

    // TODO: PROCESS MESSAGE
  }
}

static bool init_can()
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

  return true;
}

void setup()
{
  delay(200);
  Serial.begin(SERIAL_BAUD);
  debug_print_text("[NODE] boot");

  const bool can_ok = init_can();
  if (!can_ok)
  {
    debug_print_text("[NODE] CAN init failed");
  }
  else
  {
    debug_print_text("[NODE] CAN started");
  }
}

void loop()
{
  process_can_rx();
  delay(1);
}
