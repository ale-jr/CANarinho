#pragma once

#include "can_message.h"

bool send_message(
    const CanMessage& msg);

bool send_heartbeat();
bool send_event();
bool send_command();