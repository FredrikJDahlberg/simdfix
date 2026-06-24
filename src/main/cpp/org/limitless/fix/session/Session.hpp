//
// Created by Fredrik Dahlberg on 2026-06-24.
//

#ifndef SIMDFIX_SESSION_HPP
#define SIMDFIX_SESSION_HPP

#include "org/limitless/fix/messages/FixMessageHandler.hpp"

namespace org::limitless::fix::session {

class  Session : messages::MessageHandler<Session>
{
    using MessageHandler::handle;

    Result::Values handle(messages::LogonDecoder& logon)
    {
        return Result::Success;
    }
};

}

#endif //SIMDFIX_SESSION_HPP
