#ifndef SIMD_FIX_GRAMMAR_HPP
#define SIMD_FIX_GRAMMAR_HPP

#include "org/limitless/fix/decoder/Dictionary.hpp"

namespace org::limitless::fix::protocols {

using namespace org::limitless::fix::decoder;

struct Logon {
    static constexpr uint16_t Tags[] = {
        34,
        49,
        52,
        56,
        98,
        108,
        500,
        501,
        627,
        628,
        629,
        630,
    };

    static constexpr Dictionary Grammar[] = {
        { 34, 1, Presence::Null, Category::Component },
        { 49, 8, Presence::Null, Category::Component },
        { 52, 1, Presence::Null, Category::Component },
        { 56, 8, Presence::Null, Category::Component },
        { 98, 1, Presence::Optional, Category::Message },
        { 108, 1, Presence::Null, Category::Message },
        { 500, 0, Presence::Optional, Category::Group },
        { 501, 10, Presence::Null, Category::Group },
        { 627, 0, Presence::Optional, Category::Component },
        { 628, 8, Presence::Optional, Category::Group },
        { 629, 1, Presence::Optional, Category::Group },
        { 630, 1, Presence::Optional, Category::Group },
    };
};

struct Logout {
    static constexpr uint16_t Tags[] = {
        34,
        49,
        52,
        56,
        58,
        500,
        501,
        627,
        628,
        629,
        630,
    };

    static constexpr Dictionary Grammar[] = {
        { 34, 1, Presence::Null, Category::Component },
        { 49, 8, Presence::Null, Category::Component },
        { 52, 1, Presence::Null, Category::Component },
        { 56, 8, Presence::Null, Category::Component },
        { 58, 8, Presence::Null, Category::Message },
        { 500, 0, Presence::Optional, Category::Group },
        { 501, 10, Presence::Null, Category::Group },
        { 627, 0, Presence::Optional, Category::Component },
        { 628, 8, Presence::Optional, Category::Group },
        { 629, 1, Presence::Optional, Category::Group },
        { 630, 1, Presence::Optional, Category::Group },
    };
};

struct Heartbeat {
    static constexpr uint16_t Tags[] = {
        34,
        49,
        52,
        56,
        112,
        500,
        501,
        627,
        628,
        629,
        630,
    };

    static constexpr Dictionary Grammar[] = {
        { 34, 1, Presence::Null, Category::Component },
        { 49, 8, Presence::Null, Category::Component },
        { 52, 1, Presence::Null, Category::Component },
        { 56, 8, Presence::Null, Category::Component },
        { 112, 32, Presence::Optional, Category::Message },
        { 500, 0, Presence::Optional, Category::Group },
        { 501, 10, Presence::Null, Category::Group },
        { 627, 0, Presence::Optional, Category::Component },
        { 628, 8, Presence::Optional, Category::Group },
        { 629, 1, Presence::Optional, Category::Group },
        { 630, 1, Presence::Optional, Category::Group },
    };
};

struct TestRequest {
    static constexpr uint16_t Tags[] = {
        34,
        49,
        52,
        56,
        112,
        500,
        501,
        627,
        628,
        629,
        630,
    };

    static constexpr Dictionary Grammar[] = {
        { 34, 1, Presence::Null, Category::Component },
        { 49, 8, Presence::Null, Category::Component },
        { 52, 1, Presence::Null, Category::Component },
        { 56, 8, Presence::Null, Category::Component },
        { 112, 32, Presence::Optional, Category::Message },
        { 500, 0, Presence::Optional, Category::Group },
        { 501, 10, Presence::Null, Category::Group },
        { 627, 0, Presence::Optional, Category::Component },
        { 628, 8, Presence::Optional, Category::Group },
        { 629, 1, Presence::Optional, Category::Group },
        { 630, 1, Presence::Optional, Category::Group },
    };
};

}

#endif //SIMD_FIX_GRAMMAR_HPP
