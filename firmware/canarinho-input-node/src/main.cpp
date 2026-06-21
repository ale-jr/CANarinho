#include <Arduino.h>
#include "driver/twai.h"
#include <stdint.h>
#include <string.h>
#include "./config/constants.h"
#include "debug_serial.h"
#include "./can/can_bus.h"
#include "./node/node.h"

void setup()
{
  setup_serial();
  delay(200);

  debug_print_text("[NODE] boot");

  const bool can_ok = setup_can();
  if (!can_ok)
  {
    debug_print_text("[NODE] CAN init failed");
  }
  else
  {
    debug_print_text("[NODE] CAN started");
  }

  setup_node();
}

void loop()
{
  loop_can();
  loop_node();
  delay(1);
}
