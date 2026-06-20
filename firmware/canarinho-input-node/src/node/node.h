#pragma once

#include "../channels/ichannel.h"

class Node
{
public:
    void setup();
    void loop();

    void add_channel(
        IChannel* channel);

    void handle_message(
        const CanMessage& msg);

private:
    IChannel* _channels[16];
    uint8_t _channel_count = 0;
};