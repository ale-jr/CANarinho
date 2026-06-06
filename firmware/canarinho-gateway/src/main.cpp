#include <Arduino.h>
#include "driver/twai.h"
#include <stdint.h>
#include <string.h>

static constexpr gpio_num_t CAN_TX_PIN = (gpio_num_t)0;
static constexpr gpio_num_t CAN_RX_PIN = (gpio_num_t)1;
static constexpr uint32_t SERIAL_BAUD = 2000000;
static constexpr bool DEBUG_SERIAL = false;

static constexpr size_t MAX_CAN_DATA_LEN = 8;
static constexpr size_t MAX_RAW_FRAME_LEN = 64;
static constexpr size_t MAX_COBS_FRAME_LEN = MAX_RAW_FRAME_LEN + (MAX_RAW_FRAME_LEN / 254) + 2;

static constexpr uint8_t PROTO_VERSION = 0x01;

enum MsgType : uint8_t {
  MSG_CAN_FRAME = 0x01,
  MSG_CTRL_SET_MODE = 0x02,
  MSG_CTRL_GET_STATUS = 0x03,
  MSG_STATUS = 0x04,
  MSG_CTRL_ACK = 0x05,
  MSG_CTRL_ERR = 0x06,
};

enum GatewayMode : uint8_t {
  MODE_STRICT = 0x00,
  MODE_MONITOR = 0x01,
};

enum Direction : uint8_t {
  DIR_CAN_TO_PC = 0x00,
  DIR_PC_TO_CAN = 0x01,
};

enum CtrlErrCode : uint8_t {
  ERR_BAD_FORMAT = 0x01,
  ERR_BAD_DIR = 0x02,
  ERR_BAD_DLC = 0x03,
  ERR_BAD_CAN_ID = 0x04,
  ERR_TWAI_TX_FAIL = 0x05,
  ERR_UNSUPPORTED = 0x06,
};

struct Counters {
  uint32_t rx_can = 0;
  uint32_t tx_can = 0;
  uint32_t drop_filter = 0;
  uint32_t rx_serial_err = 0;
  uint32_t tx_can_err = 0;
};

static GatewayMode g_mode = MODE_STRICT;
static Counters g_cnt;

static uint8_t serial_raw_buf[MAX_RAW_FRAME_LEN];
static size_t serial_raw_len = 0;

static inline uint8_t can_dst_from_id(uint32_t can_id) {
  return (uint8_t)((can_id >> 10) & 0xFF);
}

static inline bool can_id_is_valid_29(uint32_t can_id) {
  return (can_id & ~0x1FFFFFFFUL) == 0;
}

static void debug_print_hex_byte(uint8_t value) {
  if (value < 0x10) {
    Serial.print('0');
  }
  Serial.print(value, HEX);
}

static void debug_print_can_frame(const char *prefix, const twai_message_t &msg) {
  if (!DEBUG_SERIAL) {
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

  for (uint8_t i = 0; i < msg.data_length_code; i++) {
    if (i > 0) {
      Serial.print(' ');
    }
    debug_print_hex_byte(msg.data[i]);
  }

  Serial.println();
}

static void debug_print_text(const char *text) {
  if (!DEBUG_SERIAL) {
    return;
  }

  Serial.println(text);
}

static bool gateway_accept_can_to_serial(uint32_t can_id) {
  if (g_mode == MODE_MONITOR) {
    return true;
  }

  const uint8_t dst = can_dst_from_id(can_id);
  return (dst == 0xFE) || (dst == 0xFF);
}

static size_t cobs_encode(const uint8_t *input, size_t length, uint8_t *output, size_t out_capacity) {
  if (length == 0 || out_capacity < 2) {
    return 0;
  }

  size_t read_index = 0;
  size_t write_index = 1;
  size_t code_index = 0;
  uint8_t code = 1;

  while (read_index < length) {
    if (write_index >= out_capacity) {
      return 0;
    }

    if (input[read_index] == 0) {
      output[code_index] = code;
      code = 1;
      code_index = write_index++;
      read_index++;
    } else {
      output[write_index++] = input[read_index++];
      code++;

      if (code == 0xFF) {
        output[code_index] = code;
        code = 1;
        code_index = write_index++;
      }
    }
  }

  if (code_index >= out_capacity || write_index > out_capacity) {
    return 0;
  }

  output[code_index] = code;
  return write_index;
}

static size_t cobs_decode(const uint8_t *input, size_t length, uint8_t *output, size_t out_capacity) {
  if (length == 0) {
    return 0;
  }

  size_t read_index = 0;
  size_t write_index = 0;

  while (read_index < length) {
    uint8_t code = input[read_index];
    if (code == 0) {
      return 0;
    }

    read_index++;

    for (uint8_t i = 1; i < code; i++) {
      if (read_index >= length || write_index >= out_capacity) {
        return 0;
      }
      output[write_index++] = input[read_index++];
    }

    if (code != 0xFF && read_index < length) {
      if (write_index >= out_capacity) {
        return 0;
      }
      output[write_index++] = 0;
    }
  }

  return write_index;
}

static void serial_send_cobs_frame(const uint8_t *raw, size_t raw_len) {
  if (DEBUG_SERIAL) {
    Serial.print("[GW] TX serial type=0x");
    debug_print_hex_byte(raw[1]);
    Serial.print(" len=");
    Serial.println(raw_len);
  }

  uint8_t enc[MAX_COBS_FRAME_LEN];
  const size_t enc_len = cobs_encode(raw, raw_len, enc, sizeof(enc));
  if (enc_len == 0) {
    return;
  }

  Serial.write(enc, enc_len);
  Serial.write((uint8_t)0x00);
}

static void pack_u16_le(uint8_t *dst, uint16_t value) {
  dst[0] = (uint8_t)(value & 0xFF);
  dst[1] = (uint8_t)((value >> 8) & 0xFF);
}

static uint16_t unpack_u16_le(const uint8_t *src) {
  return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

static void pack_u32_le(uint8_t *dst, uint32_t value) {
  dst[0] = (uint8_t)(value & 0xFF);
  dst[1] = (uint8_t)((value >> 8) & 0xFF);
  dst[2] = (uint8_t)((value >> 16) & 0xFF);
  dst[3] = (uint8_t)((value >> 24) & 0xFF);
}

static uint32_t unpack_u32_le(const uint8_t *src) {
  return (uint32_t)src[0] |
         ((uint32_t)src[1] << 8) |
         ((uint32_t)src[2] << 16) |
         ((uint32_t)src[3] << 24);
}

static void send_ctrl_ack(uint16_t seq) {
  uint8_t raw[4];
  raw[0] = PROTO_VERSION;
  raw[1] = MSG_CTRL_ACK;
  pack_u16_le(&raw[2], seq);
  serial_send_cobs_frame(raw, sizeof(raw));
}

static void send_ctrl_err(uint16_t seq, CtrlErrCode code) {
  uint8_t raw[5];
  raw[0] = PROTO_VERSION;
  raw[1] = MSG_CTRL_ERR;
  pack_u16_le(&raw[2], seq);
  raw[4] = (uint8_t)code;
  serial_send_cobs_frame(raw, sizeof(raw));
}

static void send_status_frame() {
  uint8_t raw[1 + 1 + 1 + 5 * 4];
  size_t idx = 0;

  raw[idx++] = PROTO_VERSION;
  raw[idx++] = MSG_STATUS;
  raw[idx++] = (uint8_t)g_mode;

  pack_u32_le(&raw[idx], g_cnt.rx_can); idx += 4;
  pack_u32_le(&raw[idx], g_cnt.tx_can); idx += 4;
  pack_u32_le(&raw[idx], g_cnt.drop_filter); idx += 4;
  pack_u32_le(&raw[idx], g_cnt.rx_serial_err); idx += 4;
  pack_u32_le(&raw[idx], g_cnt.tx_can_err); idx += 4;

  serial_send_cobs_frame(raw, idx);
}

static void send_can_frame_to_pc(const twai_message_t &msg) {
  uint8_t raw[1 + 1 + 1 + 1 + 4 + 1 + MAX_CAN_DATA_LEN];
  size_t idx = 0;

  raw[idx++] = PROTO_VERSION;
  raw[idx++] = MSG_CAN_FRAME;
  raw[idx++] = DIR_CAN_TO_PC;

  uint8_t flags = 0;
  if (msg.extd) flags |= (1u << 0);
  if (msg.rtr) flags |= (1u << 1);
  raw[idx++] = flags;

  pack_u32_le(&raw[idx], msg.identifier); idx += 4;

  uint8_t dlc = msg.data_length_code;
  if (dlc > MAX_CAN_DATA_LEN) dlc = MAX_CAN_DATA_LEN;
  raw[idx++] = dlc;

  for (uint8_t i = 0; i < dlc; i++) {
    raw[idx++] = msg.data[i];
  }

  serial_send_cobs_frame(raw, idx);
}

static CtrlErrCode can_send_frame_from_pc(const uint8_t *decoded, size_t len, uint16_t *out_seq) {
  if (len < 11) {
    return ERR_BAD_FORMAT;
  }

  const uint8_t dir = decoded[2];
  if (dir != DIR_PC_TO_CAN) {
    return ERR_BAD_DIR;
  }

  const uint8_t flags = decoded[3];
  const bool is_extended = (flags & (1u << 0)) != 0;
  const bool is_rtr = (flags & (1u << 1)) != 0;

  const uint32_t can_id = unpack_u32_le(&decoded[4]);
  const uint8_t dlc = decoded[8];
  const uint16_t seq = unpack_u16_le(&decoded[9]);
  *out_seq = seq;

  if (dlc > MAX_CAN_DATA_LEN) {
    return ERR_BAD_DLC;
  }

  if (len != (size_t)(11 + dlc)) {
    return ERR_BAD_FORMAT;
  }

  if (!can_id_is_valid_29(can_id)) {
    return ERR_BAD_CAN_ID;
  }

  twai_message_t tx_msg = {};
  tx_msg.identifier = can_id;
  tx_msg.extd = is_extended ? 1 : 0;
  tx_msg.rtr = is_rtr ? 1 : 0;
  tx_msg.data_length_code = dlc;

  if (!is_rtr && dlc > 0) {
    memcpy(tx_msg.data, &decoded[11], dlc);
  }

  if (twai_transmit(&tx_msg, pdMS_TO_TICKS(5)) == ESP_OK) {
    g_cnt.tx_can++;
    return (CtrlErrCode)0;
  }

  g_cnt.tx_can_err++;
  return ERR_TWAI_TX_FAIL;
}

static void handle_decoded_frame(const uint8_t *decoded, size_t len) {
  if (len < 2) {
    g_cnt.rx_serial_err++;
    return;
  }

  if (decoded[0] != PROTO_VERSION) {
    g_cnt.rx_serial_err++;
    return;
  }

  const uint8_t msg_type = decoded[1];

  switch (msg_type) {
    case MSG_CAN_FRAME: {
      uint16_t seq = 0xFFFF;
      CtrlErrCode err = can_send_frame_from_pc(decoded, len, &seq);
      if ((uint8_t)err == 0) {
        send_ctrl_ack(seq);
      } else {
        g_cnt.rx_serial_err++;
        send_ctrl_err(seq, err);
      }
      break;
    }

    case MSG_CTRL_SET_MODE:
      if (len != 3) {
        g_cnt.rx_serial_err++;
        return;
      }
      if (decoded[2] == MODE_STRICT || decoded[2] == MODE_MONITOR) {
        g_mode = (GatewayMode)decoded[2];
        send_status_frame();
      } else {
        g_cnt.rx_serial_err++;
      }
      break;

    case MSG_CTRL_GET_STATUS:
      if (len != 2) {
        g_cnt.rx_serial_err++;
        return;
      }
      send_status_frame();
      break;

    default:
      g_cnt.rx_serial_err++;
      send_ctrl_err(0xFFFF, ERR_UNSUPPORTED);
      break;
  }
}

static void process_serial_stream() {
  while (Serial.available() > 0) {
    const int byte_in = Serial.read();
    if (byte_in < 0) {
      break;
    }

    const uint8_t b = (uint8_t)byte_in;

    if (b == 0x00) {
      if (serial_raw_len > 0) {
        uint8_t decoded[MAX_RAW_FRAME_LEN];
        const size_t dec_len = cobs_decode(serial_raw_buf, serial_raw_len, decoded, sizeof(decoded));

        if (dec_len == 0) {
          g_cnt.rx_serial_err++;
        } else {
          handle_decoded_frame(decoded, dec_len);
        }
      }
      serial_raw_len = 0;
      continue;
    }

    if (serial_raw_len >= sizeof(serial_raw_buf)) {
      serial_raw_len = 0;
      g_cnt.rx_serial_err++;
      continue;
    }

    serial_raw_buf[serial_raw_len++] = b;
  }
}

static void process_can_rx() {
  twai_message_t msg;
  while (twai_receive(&msg, 0) == ESP_OK) {
    g_cnt.rx_can++;
    debug_print_can_frame("[GW] RX CAN", msg);

    if (!msg.extd) {
      g_cnt.drop_filter++;
      debug_print_text("[GW] drop: non-extended frame");
      continue;
    }

    if (!gateway_accept_can_to_serial(msg.identifier)) {
      g_cnt.drop_filter++;
      debug_print_text("[GW] drop: filtered by DST");
      continue;
    }

    debug_print_text("[GW] forward CAN -> serial");
    send_can_frame_to_pc(msg);
  }
}

static bool init_can() {
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

  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
    return false;
  }

  if (twai_start() != ESP_OK) {
    twai_driver_uninstall();
    return false;
  }

  return true;
}

void setup() {
  delay(200);
  Serial.begin(SERIAL_BAUD);
  debug_print_text("[GW] boot");

  const bool can_ok = init_can();
  if (!can_ok) {
    g_cnt.tx_can_err++;
    debug_print_text("[GW] CAN init failed");
  } else {
    debug_print_text("[GW] CAN started");
  }

  send_status_frame();
}

void loop() {
  process_serial_stream();
  process_can_rx();
  delay(1);
}
