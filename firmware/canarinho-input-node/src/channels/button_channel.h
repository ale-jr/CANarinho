#pragma once
#include <Arduino.h>
#include "./can/can_message.h"

enum class ButtonPressType : uint8_t
{
    Press = 0,
    Release = 1,
    Click = 2,
    LongClick = 3,
};

enum class ButtonActionType : uint8_t
{
    None,
    SendCommand,
    SendEvent
};

struct ButtonAction
{
    ButtonPressType pressType;
    ButtonActionType actionType = ButtonActionType::None;
    uint8_t command_dst = 0;
    uint8_t command_channel = 0;
    uint8_t command_payload[8] = {};
    uint8_t command_payload_len = 0;
    uint8_t event_dst = 0;
};

class ButtonChannel
{
public:
    ButtonChannel(int pin, uint8_t channel);
    void setup();
    void loop();

    void register_action(ButtonAction action);
    void handle_message(CanMessage &message);

private:
    int _pin;
    uint8_t _channel;
    static constexpr uint8_t MAX_ACTIONS = 10;
    ButtonAction _actions[MAX_ACTIONS];
    uint8_t _action_count = 0;

    void handle_press();
    void handle_release();
    bool _last_state = HIGH;
    unsigned long _last_change_ms = 0;
    bool _pressed = false;
    uint32_t _press_time = 0;

    void emit_event(ButtonPressType type);
    void handle_action(ButtonAction &action);
};