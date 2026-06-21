#include "node.h"
#include "./channels/button_channel.h"
#include "./config/constants.h"
#include "./channels/dimmer_channel.h"

ButtonChannel button1(6, 1);
DimmerChannel dimmer1(8, 2, 255);

void setup_node()
{

    dimmer1.setup();

    // Start button 1 setup
    button1.setup();
    ButtonAction button1Click = {};
    button1Click.pressType = ButtonPressType::Click;
    button1Click.actionType = ButtonActionType::SendEvent;
    button1Click.event_dst = GATEWAY_NODE_ID;
    button1.register_action(button1Click);

    button1.setup();
    ButtonAction button1ClickLed = {};
    button1ClickLed.pressType = ButtonPressType::Click;
    button1ClickLed.actionType = ButtonActionType::SendCommand;
    button1ClickLed.command_channel = 2;
    button1ClickLed.command_dst = NODE_ID;
    button1ClickLed.command_payload[0] = 2;
    button1ClickLed.command_payload_len = 1;
    button1.register_action(button1ClickLed);

    ButtonAction buttonPress = {};
    buttonPress.pressType = ButtonPressType::ContinuousPress;
    buttonPress.actionType = ButtonActionType::SendCommand;
    buttonPress.command_channel = 2;
    buttonPress.command_dst = NODE_ID;
    buttonPress.command_payload[0] = 1;
    buttonPress.command_payload[1] = 20;
    buttonPress.command_payload_len = 2;
    button1.register_action(buttonPress);

    ButtonAction buttonRelease = {};
    buttonPress.pressType = ButtonPressType::Release;
    buttonPress.actionType = ButtonActionType::SendCommand;
    buttonPress.command_channel = 2;
    buttonPress.command_dst = NODE_ID;
    buttonPress.command_payload[0] = 0;
    buttonPress.command_payload_len = 1;
    button1.register_action(buttonPress);

    // ButtonAction button2LongClick = {};
    // button2LongClick.pressType = ButtonPressType::LongClick;
    // button2LongClick.actionType = ButtonActionType::SendCommand;
    // button2LongClick.command_channel = 1;
    // button2LongClick.command_dst = NODE_ID;
    // button2LongClick.command_payload[0] = 255;
    // button2LongClick.command_payload_len = 1;
    // button1.register_action(button2LongClick);
    // End button 1 setup
}

void loop_node()
{

    button1.loop();
    dimmer1.loop();
}

void handle_message(CanMessage &message)
{
    button1.handle_message(message);
    dimmer1.handle_message(message);
}
