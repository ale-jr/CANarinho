void Node::handle_message(
    const CanMessage &msg)
{
    if (msg.channel == 0)
    {
        // heartbeat
        // query global
        return;
    }

    for (uint8_t i = 0; i < _channel_count; i++)
    {
        if (_channels[i]->channel() ==
            msg.channel)
        {
            _channels[i]->handle_message(msg);
            return;
        }
    }
}