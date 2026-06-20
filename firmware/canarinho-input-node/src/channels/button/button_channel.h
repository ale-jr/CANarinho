#pragma once

#include "../ichannel.h"

class ButtonChannel : public IChannel
{
public:
    ButtonChannel(
        uint8_t channel,
        uint8_t pin);

    uint8_t channel() const override;

    void setup() override;
    void loop() override;

private:
    uint8_t _channel;
    uint8_t _pin;
};