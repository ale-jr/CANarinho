#include "node.h"
#include "./channels/button_channel.h"
#include "./config/constants.h"

ButtonChannel button1(6, 1);

void setup_node()
{

    // Start button 1 setup
    button1.setup();
    ButtonAction button1Click = {};
    button1Click.pressType = ButtonPressType::Click;
    button1Click.actionType = ButtonActionType::SendEvent;
    button1Click.event_dst = GATEWAY_NODE_ID;
    button1.register_action(button1Click);

    ButtonAction button2LongClick = {};
    button2LongClick.pressType = ButtonPressType::LongClick;
    button2LongClick.actionType = ButtonActionType::SendCommand;
    button2LongClick.command_channel = 1;
    button2LongClick.command_dst = GATEWAY_NODE_ID;
    button2LongClick.command_payload[0] = 255;
    button2LongClick.command_payload_len = 1;
    button1.register_action(button2LongClick);
    // End button 1 setup
}

void loop_node()
{

    button1.loop();
}

void handle_message(CanMessage &message)
{
    button1.handle_message(message);
}
