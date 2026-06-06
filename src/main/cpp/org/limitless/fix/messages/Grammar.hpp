#ifndef SIMD_FIX_GRAMMAR_HPP
#define SIMD_FIX_GRAMMAR_HPP

#include "org/limitless/fix/decoder/Dictionary.hpp"

namespace org::limitless::fix::protocols {

using namespace org::limitless::fix::decoder;

struct Hops {
    static constexpr uint16_t Tags[] = {
        628,
        629,
        630,
    };

    static constexpr Dictionary Grammar[] = {
        { 628, 8, Presence::Optional },
        { 629, 1, Presence::Optional },
        { 630, 1, Presence::Optional },
    };
};

struct StandardHeader {
    static constexpr uint16_t Tags[] = {
        34,
        49,
        52,
        56,
        627,
        628,
        629,
        630,
    };

    static constexpr Dictionary Grammar[] = {
        { 34, 1, Presence::Required },
        { 49, 8, Presence::Required },
        { 52, 1, Presence::Required },
        { 56, 8, Presence::Required },
        { 627, 0, Presence::Optional },
        { 628, 8, Presence::Optional },
        { 629, 1, Presence::Optional },
        { 630, 1, Presence::Optional },
    };
};

struct Logon {
    static constexpr uint16_t Tags[] = {
        34,
        49,
        52,
        56,
        98,
        108,
        627,
        628,
        629,
        630,
    };

    static constexpr Dictionary Grammar[] = {
        { 34, 1, Presence::Required },
        { 49, 8, Presence::Required },
        { 52, 1, Presence::Required },
        { 56, 8, Presence::Required },
        { 98, 1, Presence::Optional },
        { 108, 1, Presence::Required },
        { 627, 0, Presence::Optional },
        { 628, 8, Presence::Optional },
        { 629, 1, Presence::Optional },
        { 630, 1, Presence::Optional },
    };
};

struct Logout {
    static constexpr uint16_t Tags[] = {
        34,
        49,
        52,
        56,
        58,
        627,
        628,
        629,
        630,
    };

    static constexpr Dictionary Grammar[] = {
        { 34, 1, Presence::Required },
        { 49, 8, Presence::Required },
        { 52, 1, Presence::Required },
        { 56, 8, Presence::Required },
        { 58, 8, Presence::Required },
        { 627, 0, Presence::Optional },
        { 628, 8, Presence::Optional },
        { 629, 1, Presence::Optional },
        { 630, 1, Presence::Optional },
    };
};

struct Heartbeat {
    static constexpr uint16_t Tags[] = {
        34,
        49,
        52,
        56,
        112,
        627,
        628,
        629,
        630,
    };

    static constexpr Dictionary Grammar[] = {
        { 34, 1, Presence::Required },
        { 49, 8, Presence::Required },
        { 52, 1, Presence::Required },
        { 56, 8, Presence::Required },
        { 112, 32, Presence::Required },
        { 627, 0, Presence::Optional },
        { 628, 8, Presence::Optional },
        { 629, 1, Presence::Optional },
        { 630, 1, Presence::Optional },
    };
};

}

#endif //SIMD_FIX_GRAMMAR_HPP
