#pragma once
#include <Arduino.h>
#include "driver/twai.h"

void setup_serial();
void debug_print_can_frame(const char *prefix, const twai_message_t &msg);
void debug_print_text(const char *text);
