#include "button_channel.h"
#include <Arduino.h>
#include "./can/can_tx.h"
#include "./config/constants.h"
#include "./debug_serial.h"
#define DEBOUNCE_TIMEOUT_US 50000
#define LONG_PRESS_MS 1000

ButtonChannel::ButtonChannel(int pin, uint8_t channel)
{
    _pin = pin;
    _channel = channel;
}

void ButtonChannel::setup()
{

    pinMode(_pin, INPUT_PULLUP);

    attachInterruptArg(_pin, ButtonChannel::isr, this, CHANGE);
}

void ButtonChannel::loop()
{
    if (!_state_changed)
    {
        return;
    }

    _state_changed = false;

    if (_current_state == LOW)
    {
        handle_press();
    }
    else
    {
        handle_release();
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
void IRAM_ATTR ButtonChannel::isr(void *arg)
{
    debug_print_text("ISR");
    auto *self = static_cast<ButtonChannel *>(arg);

    const uint64_t now =
        esp_timer_get_time();

    if (now - self->_last_interrupt_us < DEBOUNCE_TIMEOUT_US)
    {
        return;
    }

    self->_last_interrupt_us = now;

    self->_current_state =
        digitalRead(self->_pin);

    self->_state_changed = true;
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
    uint32_t duration =
        millis() - _press_time;

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
