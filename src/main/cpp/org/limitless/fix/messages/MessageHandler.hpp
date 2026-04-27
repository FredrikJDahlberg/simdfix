//
// Created by Fredrik Dahlberg on 2026-04-25.
//

#ifndef SIMD_FIX_MESSAGE_HANDLER_HPP
#define SIMD_FIX_MESSAGE_HANDLER_HPP

#include "org/limitless/fix/messages/LogonDecoder.hpp"

namespace org::limitless::fix::generated {

template <typename Derived>
class MessageHandler
{
public:
    template <typename Event>
    void receive(Event&& event)
    {
        static_cast<Derived*>(this)->handle(std::forward<Event>(event));
    }

    void handle(MessageDecoder& message)
    {
        switch (message.type())
        {
            case 'A':
                receive(reinterpret_cast<generated::LogonDecoder&>(message).wrap());
                break;
            default:
                break;
        }
    }

protected:
    void handle(LogonDecoder* m) {}
};

}

#endif //SIMD_FIX_MESSAGEHANDLER_HPP
