//
// Created by Fredrik Dahlberg on 2026-04-25.
//

#ifndef SIMD_FIX_MESSAGEHANDLER_HPP
#define SIMD_FIX_MESSAGEHANDLER_HPP

#include "org/limitless/fix/messages/Logon.hpp"

namespace org::limitless::fix::messages {

struct MessageHandler
{
    void handle(const Message* message)
    {
#if 0
        auto type = message->messageType();
        switch (type)
        {
            case MessageType::Logon:
                handleLogon(reinterpret_cast<const Logon*>(message));
                break;
        }
#endif
    }

    void handleLogon(const Logon* message)
    {

    }
};

}

#endif //SIMD_FIX_MESSAGEHANDLER_HPP
