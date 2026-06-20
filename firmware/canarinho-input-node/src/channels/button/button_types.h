enum class ButtonActionType : uint8_t
{
    None,
    SendCommand,
    SendEvent
};

struct ButtonAction
{
    ButtonActionType type;

    uint8_t dst;
    uint8_t channel;

    uint8_t action;
    uint8_t value;
};

struct Button
{
    uint8_t gpio;
    uint8_t channel;

    ButtonAction click;
    ButtonAction double_click;
    ButtonAction long_click;
};