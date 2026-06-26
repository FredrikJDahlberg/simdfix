//
// Created by Fredrik Dahlberg on 2026-06-26.
//

#ifndef SIMD_FIX_PAYLOAD_HANDLER_HPP
#define SIMD_FIX_PAYLOAD_HANDLER_HPP

#include <concepts>

#include "org/limitless/fix/TokenizedMessage.hpp"

namespace org::limitless::fix::decoder
{

/**
 * Constrains the Handler passed to PayloadDecoder::parse(): once a buffer is
 * tokenized, the decoder hands the whole TokenizedMessage to the handler via
 * handle(message), expecting a Result back. The handler recovers the MsgType
 * itself with message.messageId(). The generated MessageHandler and the session
 * engine layered on it both model this concept.
 *
 * Lives in its own lightweight header (TokenizedMessage + Result only) so that
 * handler authors and the session engine can name the contract without pulling
 * in the SIMD tokenizer in PayloadDecoder.hpp.
 */
template <typename Handler>
concept PayloadHandler = requires(Handler handler, const TokenizedMessage& message)
{
    { handler.handle(message) } -> std::same_as<Result>;
};

}

#endif //SIMD_FIX_PAYLOAD_HANDLER_HPP