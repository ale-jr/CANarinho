#include <Arduino.h>
#include "driver/twai.h"

#include "debug_serial.h"
#include "./config/constants.h"

void setup_serial()
{
    Serial.begin(SERIAL_BAUD);
}

static void debug_print_hex_byte(uint8_t value)
{
    if (value < 0x10)
    {
        Serial.print('0');
    }
    Serial.print(value, HEX);
}
void debug_print_can_frame(const char *prefix, const twai_message_t &msg)
{
    return;
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
void debug_print_text(const char *text)
{
    if (!DEBUG_SERIAL)
    {
        return;
    }

    Serial.println(text);
}
