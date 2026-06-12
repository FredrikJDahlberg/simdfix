//
// Created by Fredrik Dahlberg on 2026-06-05.
//

#ifndef SIMD_FIX_COMPONENT_DECODER_HPP
#define SIMD_FIX_COMPONENT_DECODER_HPP

#include "FieldDecoder.hpp"

namespace org::limitless::fix::decoder {

/**
 * Base for generated decoders of FIX components: reusable field groupings
 * shared across multiple message types. Holds a reference to the enclosing
 * message's FieldDecoder so component field accessors resolve with
 * ParentType::Component against the whole message.
 */
struct ComponentDecoder
{
protected:
    FieldDecoder& m_decoder;

public:

    /**
     * @param decoder field decoder over the message containing this component
     */
    explicit ComponentDecoder(FieldDecoder& decoder) : m_decoder{decoder}
    {
    }
};

}
#endif //SIMD_FIX_COMPONENT_DECODER_HPP
