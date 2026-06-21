#pragma once
#include <Arduino.h>
#include "./can/can_message.h"

enum class DimmerActionType : uint8_t
{
    Off = 0,
    On = 1,
    Toggle = 2,
    SetLevel = 3,
    ToggleLevel = 4,
    StepUp = 5,
    StepDown = 6,
    StartPushDimUp = 7,
    StartPushDimDown = 8,
    StartPushDimToggle = 9,
    StopPushDim = 10,

};

class DimmerChannel
{
public:
    DimmerChannel(int pin, uint8_t channel, uint8_t default_brightness);
    void setup();
    void loop();

    void handle_message(CanMessage &message);

private:
    int _pin;
    uint8_t _channel;
    uint8_t _pwm_channel;
    uint8_t _default_brightness;

    uint8_t _current_brightness = 0;
    uint8_t _start_brightness = 0;
    uint8_t _target_brightness = 0;
    uint32_t _transition_start_ms = 0;
    uint32_t _transition_duration_ms = 500;
    bool _transition_active = false;

    void setBrightness(uint8_t brightness, uint32_t transition_ms);

    void sendStatusUpdate();

    void brightnessUpdateLoop();

    void pushDimLoop();

    void toggleBrightness(uint8_t brightness, uint32_t transition_ms);

    void toggle(uint32_t transition_ms);

    void stepBrightness(int8_t value, uint32_t transition_ms);

    void handleCommand(uint8_t payload[8], uint8_t payload_len);

    bool _push_dim_active = false;
    int8_t _push_dim_direction = 0;
    unsigned long _last_push_dim_ms = 0;
    int8_t _last_push_dim_direction = 1;
    unsigned long _push_dim_started_ms = 0;
    uint8_t _push_dim_step = 2;

    void startPushDimUp(uint8_t step);
    void startPushDimDown(uint8_t step);
    void startPushDimToggle(uint8_t step);
    void stopPushDim();
};