#pragma once

#include "can_message.h"

bool send_heartbeat();

bool send_event(const EventMessage &event);

bool send_command(const CommandMessage &command);

bool send_message(const CanMessage &msg);

extern unsigned long next_heartbeat_ms;