#include "button_channel.h"
#include <Arduino.h>
#include "./can/can_tx.h"
#include "./config/constants.h"
#include "./debug_serial.h"
#define DEBOUNCE_TIMEOUT_MS 30
#define LONG_PRESS_MS 500

ButtonChannel::ButtonChannel(int pin, uint8_t channel)
{
    _pin = pin;
    _channel = channel;
}

void ButtonChannel::setup()
{

    pinMode(_pin, INPUT_PULLUP);
}

void ButtonChannel::loop()
{
    bool current_state =
        digitalRead(_pin);

    if (current_state != _last_state)
    {
        unsigned long now = millis();

        if (now - _last_change_ms >= DEBOUNCE_TIMEOUT_MS)
        {
            _last_change_ms = now;
            _last_state = current_state;

            if (current_state == LOW)
            {
                handle_press();
            }
            else
            {
                handle_release();
            }
        }
    }
}

void ButtonChannel::register_action(ButtonAction action)
{
    if (_action_count >= MAX_ACTIONS)
    {
        return;
    }

    _actions[_action_count] = action;
    _action_count++;
}
void ButtonChannel::handle_message(CanMessage &message)
{
}

void ButtonChannel::handle_press()
{
    _pressed = true;
    _press_time = millis();
    emit_event(ButtonPressType::Press);
}

void ButtonChannel::handle_release()
{
    _pressed = false;
    emit_event(ButtonPressType::Release);
    uint32_t duration = millis() - _press_time;

    if (duration >= LONG_PRESS_MS)
    {
        emit_event(ButtonPressType::LongClick);
    }
    else
    {
        emit_event(ButtonPressType::Click);
    }
}

void ButtonChannel::emit_event(ButtonPressType type)
{
    for (uint8_t i = 0; i < _action_count; i++)
    {
        ButtonAction &action = _actions[i];
        if (action.pressType == type)
        {
            ButtonChannel::handle_action(action);
        }
    }
}

void ButtonChannel::handle_action(ButtonAction &action)
{
    switch (action.actionType)
    {
    case ButtonActionType::SendCommand:
    {
        CommandMessage commandMessage = {};
        commandMessage.channel = action.command_channel;
        commandMessage.dst = action.command_dst;

        commandMessage.payload_len = action.command_payload_len;
        memcpy(
            commandMessage.payload,
            action.command_payload,
            action.command_payload_len);
        send_command(commandMessage);

        break;
    }
    case ButtonActionType::SendEvent:
    {
        EventMessage eventMessage = {};
        eventMessage.channel = _channel;
        eventMessage.dst = action.event_dst;
        eventMessage.payload[0] = static_cast<uint8_t>(action.pressType);
        eventMessage.payload_len = 1;
        send_event(eventMessage);
        break;
    }
    default:
        break;
    }
}
