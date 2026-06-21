#include "dimmer_channel.h"
#include <Arduino.h>
#include "./can/can_tx.h"
#include "./can/can_message.h"
#include "./config/constants.h"
#include "./debug_serial.h"

#define PWM_FREQUENCY_HZ 2000
#define PWM_RESOLUTION 8
constexpr uint32_t DEFAULT_TRANSITION_MS = 500;
constexpr uint8_t FADE_STEP = 5;
constexpr uint16_t FADE_INTERVAL_MS = 20;

static constexpr uint32_t PUSH_DIM_INTERVAL_MS = 50;
static constexpr uint32_t PUSH_DIM_TIMEOUT_MS = 5000;
static constexpr uint8_t DEFAULT_PUSH_DIM_STEP = 2;

static uint8_t next_pwm_channel = 0;

DimmerChannel::DimmerChannel(int pin, uint8_t channel, uint8_t default_brightness)
{
    _pin = pin;
    _channel = channel;
    _default_brightness = default_brightness;
    _pwm_channel = next_pwm_channel++;
}

void DimmerChannel::setup()
{

    pinMode(_pin, OUTPUT);
    ledcSetup(_pwm_channel, PWM_FREQUENCY_HZ, PWM_RESOLUTION);
    ledcAttachPin(_pin, _pwm_channel);
    // TODO: get it from memory
    setBrightness(0, 0);
}

void DimmerChannel::loop()
{
    brightnessUpdateLoop();
}

void DimmerChannel::handle_message(CanMessage &message)
{
    if (message.channel != 0 && message.channel != _channel)
    {
        return;
    }

    if (message.type == MessageType::Query)
    {
        sendStatusUpdate();
        return;
    }

    if (message.channel != _channel)
    {
        return;
    }

    if (message.type == MessageType::Command)
    {
        handleCommand(message.payload, message.payload_len);
    }
}

void DimmerChannel::setBrightness(uint8_t brightness, uint32_t transition_ms)
{
    _start_brightness =
        _current_brightness;

    _target_brightness =
        brightness;

    _transition_start_ms =
        millis();

    _transition_duration_ms =
        transition_ms;

    _transition_active = true;
    sendStatusUpdate();

    // Serial.print("target ");
    // Serial.print(_target_brightness);
    // Serial.println("");
}

void DimmerChannel::sendStatusUpdate()
{

    StatusMessage message = {};
    message.channel = _channel;
    message.payload[0] = _target_brightness;
    message.payload_len = 1;
    send_status(message);
}

void DimmerChannel::pushDimLoop()
{
    if (!_push_dim_active)
    {
        return;
    }

    uint32_t now = millis();

    if (now - _push_dim_started_ms >
        PUSH_DIM_TIMEOUT_MS)
    {
        _last_push_dim_direction =
            _push_dim_direction;

        _push_dim_active = false;

        return;
    }

    if (now - _last_push_dim_ms <
        PUSH_DIM_INTERVAL_MS)
    {
        return;
    }

    _last_push_dim_ms = now;

    int16_t brightness =
        _current_brightness +
        (_push_dim_direction *
         _push_dim_step);

    brightness =
        constrain(
            brightness,
            0,
            255);

    _current_brightness =
        brightness;

    _target_brightness =
        brightness;

    Serial.print(brightness);
    ledcWrite(
        _pwm_channel,
        brightness);
}

void DimmerChannel::brightnessUpdateLoop()
{
    pushDimLoop();

    if (!_transition_active)
    {
        return;
    }

    uint32_t now = millis();

    uint32_t elapsed =
        now - _transition_start_ms;

    if (elapsed >=
        _transition_duration_ms)
    {
        _current_brightness =
            _target_brightness;

        ledcWrite(
            _pwm_channel,
            _current_brightness);

        _transition_active = false;

        return;
    }

    float progress =
        (float)elapsed /
        (float)_transition_duration_ms;

    _current_brightness =
        _start_brightness +
        ((_target_brightness -
          _start_brightness) *
         progress);

    ledcWrite(
        _pwm_channel,
        _current_brightness);
}

void DimmerChannel::toggleBrightness(uint8_t brightness, uint32_t transition_ms)
{
    setBrightness(_current_brightness > 0 ? 0 : brightness, transition_ms);
}

void DimmerChannel::toggle(uint32_t transition_ms)
{
    setBrightness(_current_brightness > 0 ? 0 : _default_brightness, transition_ms);
}

void DimmerChannel::stepBrightness(int8_t value, uint32_t transition_ms)
{

    int newValue = _current_brightness + value;

    if (newValue < 0)
    {
        newValue = 0;
    }
    else if (newValue > 255)
    {
        newValue = 255;
    }

    setBrightness(newValue, transition_ms);
}

void DimmerChannel::handleCommand(uint8_t payload[8], uint8_t payload_len)
{
    uint8_t rawType = payload[0];
    uint8_t value = payload[1];
    uint32_t transition_ms =
        (payload_len >= 3)
            ? payload[2] * 100
            : DEFAULT_TRANSITION_MS;

    DimmerActionType type = static_cast<DimmerActionType>(rawType);

    switch (type)
    {
    case DimmerActionType::Off:
        setBrightness(0, transition_ms);
        break;
    case DimmerActionType::On:
        setBrightness(_default_brightness, transition_ms);
        break;
    case DimmerActionType::SetLevel:
        setBrightness(value, transition_ms);
        break;
    case DimmerActionType::StepDown:
        stepBrightness(value * -1, transition_ms);
        break;
    case DimmerActionType::StepUp:
        stepBrightness(value, transition_ms);
        break;
    case DimmerActionType::Toggle:
        toggle(transition_ms);
        break;
    case DimmerActionType::ToggleLevel:
        toggleBrightness(value, transition_ms);
        break;
    case DimmerActionType::StartPushDimUp:
        startPushDimUp(payload_len < 2 ? DEFAULT_PUSH_DIM_STEP : value);
        break;
    case DimmerActionType::StartPushDimDown:
        startPushDimDown(payload_len < 2 ? DEFAULT_PUSH_DIM_STEP : value);
        break;
    case DimmerActionType::StartPushDimToggle:
        startPushDimToggle(payload_len < 2 ? DEFAULT_PUSH_DIM_STEP : value);
        break;
    case DimmerActionType::StopPushDim:
        stopPushDim();
        break;
    default:
        break;
    }
}

void DimmerChannel::startPushDimUp(uint8_t step)
{
    _transition_active = false;
    _push_dim_active = true;
    _push_dim_direction = 1;
    _push_dim_step = step;
    _push_dim_started_ms = millis();
}

void DimmerChannel::startPushDimDown(uint8_t step)
{
    _transition_active = false;
    _push_dim_active = true;
    _push_dim_direction = -1;
    _push_dim_step = step;
    _push_dim_started_ms = millis();
}

void DimmerChannel::startPushDimToggle(uint8_t step)
{
    Serial.print(step);
    debug_print_text("push dim toggle");

    _transition_active = false;
    _push_dim_active = true;
    _push_dim_direction = -_last_push_dim_direction;
    _push_dim_step = step;
    _push_dim_started_ms = millis();
}

void DimmerChannel::stopPushDim()
{
    debug_print_text("stop push dim");

    _last_push_dim_direction = _push_dim_direction;
    _push_dim_active = false;
}
