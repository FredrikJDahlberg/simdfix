//
// Created by Fredrik Dahlberg on 2026-05-15.
//

#include <bit>

#include <gtest/gtest.h>
#include "org/limitless/simdifx/detail/simd/Uint8x16.hpp"

using namespace org::limitless::simdifx::detail::simd;

static void extract(const Uint8x16& v, uint8_t (&out)[16])
{
    v.get(0, out);
}

TEST(Uint8x16, BroadcastLoadStore)
{
    // Broadcast: every lane should hold the filler value.
    const Uint8x16 broadcast{0x42};
    uint8_t out[16]{};
    extract(broadcast, out);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(0x42, out[i]) << "broadcast mismatch at lane " << i;
    }

    // load / get round-trip: arbitrary per-byte values survive the round-trip.
    uint8_t src[16];
    for (int i = 0; i < 16; ++i)
    {
        src[i] = static_cast<uint8_t>(i * 17);
    }

    Uint8x16 loaded;
    loaded.load(src);
    uint8_t dst[16]{};
    extract(loaded, dst);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(src[i], dst[i]) << "load/get round-trip mismatch at lane " << i;
    }
}

TEST(Uint8x16, EqualityAndToUint64)
{
    // Bytes 0 and 4 equal '='; all others are zero.
    uint8_t data[16]{};
    data[0] = '=';
    data[4] = '=';
    Uint8x16 v;
    v.load(data);

    const Uint8x16 needle{'='};
    const uint64_t result = (v == needle).toUint64();

    // Byte 0 match → nibble 0 = 0xF (bits  0– 3)
    // Byte 4 match → nibble 4 = 0xF (bits 16–19)
    EXPECT_EQ(0x00000000000F000Full, result);
}

TEST(Uint8x16, ShiftLeftAndRight)
{
    uint8_t src[16];
    for (int i = 0; i < 16; ++i) src[i] = static_cast<uint8_t>(i + 1); // 1..16
    Uint8x16 v;
    v.load(src);

    uint8_t left[16]{};
    extract(v.shiftLeft<3>(), left);
    for (int i = 0; i < 13; ++i)
    {
        EXPECT_EQ(src[i + 3], left[i]) << "shiftLeft<3> mismatch at lane " << i;
    }
    EXPECT_EQ(0, left[13]);
    EXPECT_EQ(0, left[14]);
    EXPECT_EQ(0, left[15]);

    uint8_t right[16]{};
    extract(v.shiftRight<3>(), right);
    EXPECT_EQ(0, right[0]);
    EXPECT_EQ(0, right[1]);
    EXPECT_EQ(0, right[2]);
    for (int i = 3; i < 16; ++i)
    {
        EXPECT_EQ(src[i - 3], right[i]) << "shiftRight<3> mismatch at lane " << i;
    }
}

TEST(Uint8x16, BitwiseAndArithmetic)
{
    const Uint8x16 a{0b10110100};  // 0xB4 = 180
    const Uint8x16 b{0b01100111};  // 0x67 = 103
    uint8_t r[16]{};

    // AND
    extract(a & b, r);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(0xB4u & 0x67u, r[i]) << "AND mismatch at lane " << i;
    }

    // OR
    extract(a | b, r);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(0xB4u | 0x67u, r[i]) << "OR mismatch at lane " << i;
    }

    // Subtraction (wrapping): 180 - 103 = 77
    extract(a - b, r);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(77u, r[i]) << "SUB mismatch at lane " << i;
    }

    // Wrapping: 10 - 20 = 246 (mod 256)
    const Uint8x16 lo{10};
    const Uint8x16 hi{20};
    extract(lo - hi, r);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(static_cast<uint8_t>(10 - 20), r[i]) << "SUB wrap mismatch at lane " << i;
    }

    // Addition: 180 + 103 = 283 = 27 (mod 256)
    extract(a + b, r);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(static_cast<uint8_t>(180 + 103), r[i]) << "ADD mismatch at lane " << i;
    }
}

TEST(Uint8x16, WhenTrueAndSum)
{
    // whenTrue: pass-through where mask = 0xFF, zero otherwise.
    uint8_t mask_data[16], val_data[16];
    for (int i = 0; i < 16; ++i)
    {
        mask_data[i] = i % 2 == 0 ? 0xFF : 0x00;
        val_data[i]  = static_cast<uint8_t>(i + 1);  // 1..16
    }
    Uint8x16 mask, val;
    mask.load(mask_data);
    val.load(val_data);

    uint8_t selected[16]{};
    extract(mask.whenTrue(val), selected);
    for (int i = 0; i < 16; ++i)
    {
        const uint8_t expected = i % 2 == 0 ? val_data[i] : 0;
        EXPECT_EQ(expected, selected[i]) << "whenTrue mismatch at lane " << i;
    }

    // sum: 1 + 2 + ... + 16 = 136
    EXPECT_EQ(136u, val.sum());

    // sum of broadcast 7: 7 * 16 = 112
    EXPECT_EQ(112u, Uint8x16{7}.sum());
}

TEST(Uint8x16, Put)
{
    uint8_t src[16];
    for (int i = 0; i < 16; ++i)
    {
        src[i] = static_cast<uint8_t>(i + 1);
    }

    Uint8x16 v{0xAA};  // pre-fill with 0xAA
    // Short buffer: put() must load the valid bytes and zero-fill the rest.
    v.put(src, 15);
    uint8_t out[16]{};
    extract(v, out);
    for (int i = 0; i < 15; ++i)
    {
        EXPECT_EQ(src[i], out[i]) << "short put() load mismatch at lane " << i;
    }
    EXPECT_EQ(0u, out[15]) << "lane beyond the valid length must be zero";
    // Full buffer: put() must load the data.
    v.put(src, 16);
    extract(v, out);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(src[i], out[i]) << "put() load mismatch at lane " << i;
    }
}

TEST(Uint8x16, RangeComparison)
{
    const Uint8x16 data{50};
    const Uint8x16 eq{50};
    const Uint8x16 below{49};
    const Uint8x16 above{51};

    // All lanes satisfy >= 50 when value == 50 or above.
    EXPECT_EQ(0xFFFFFFFFFFFFFFFFull, (data >= eq).toUint64());
    EXPECT_EQ(0xFFFFFFFFFFFFFFFFull, (data >= below).toUint64());
    EXPECT_EQ(0x0000000000000000ull, (data >= above).toUint64());

    // All lanes satisfy <= 50 when value == 50 or below.
    EXPECT_EQ(0xFFFFFFFFFFFFFFFFull, (data <= eq).toUint64());
    EXPECT_EQ(0xFFFFFFFFFFFFFFFFull, (data <= above).toUint64());
    EXPECT_EQ(0x0000000000000000ull, (data <= below).toUint64());
}

TEST(Uint8x16, InPlaceOperators)
{
    uint8_t r[16]{};

    // operator-= : same result as operator-
    Uint8x16 sub{180};
    sub -= Uint8x16{103};
    extract(sub, r);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(77u, r[i]) << "-= mismatch at lane " << i;
    }
    // operator+= : same result as operator+, wrapping
    Uint8x16 add{180};
    add += Uint8x16{103};
    extract(add, r);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(static_cast<uint8_t>(180 + 103), r[i]) << "+= mismatch at lane " << i;
    }
    // operator&= : same result as operator&
    Uint8x16 andv{0xB4};
    andv &= Uint8x16{0x67};
    extract(andv, r);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(0xB4u & 0x67u, r[i]) << "&= mismatch at lane " << i;
    }
    // operator|= : same result as operator|
    Uint8x16 orv{0xB4};
    orv |= Uint8x16{0x67};
    extract(orv, r);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(0xB4u | 0x67u, r[i]) << "|= mismatch at lane " << i;
    }
}

TEST(Uint8x16, Uint16Lanes)
{
    // 8 16-bit values; lane 5 holds the key.
    const uint16_t values[8] = {10, 270, 522, 1128, 35, 553, 9, 108};
    const uint16_t key = 553;

    const Uint8x16 keys{key};
    const uint64_t bits = Uint8x16{}.put(values, sizeof(values)).equal(keys).toUint64();
    EXPECT_NE(0u, bits);
    EXPECT_EQ(5, std::countr_zero(bits) >> 3);

    // A lane matching in only one of its two bytes must not match:
    // 0x0102 vs 0x0103 share the high byte.
    const uint16_t partial[8] = {0x0103, 0x0103, 0x0103, 0x0103, 0x0103, 0x0103, 0x0103, 0x0103};
    const Uint8x16 needle{static_cast<uint16_t>(0x0102)};
    EXPECT_EQ(0u, Uint8x16{}.put(partial, sizeof(partial)).equal(needle).toUint64());

    // No match at all.
    const Uint8x16 missing{static_cast<uint16_t>(9999)};
    EXPECT_EQ(0u, Uint8x16{}.put(values, sizeof(values)).equal(missing).toUint64());
}

TEST(Uint8x16, EqualOutParamAndData)
{
    uint8_t buf[16]{};
    buf[3] = 0x01;
    buf[11] = 0x01;
    Uint8x16 v;
    v.load(buf);

    const Uint8x16 needle{0x01};
    Uint8x16 result{0};
    v.equal(needle, result);   // result = v == needle via out-param

    // Verify equal() matches operator==.
    uint8_t via_equal[16]{}, via_op[16]{};
    extract(result, via_equal);
    extract(v == needle, via_op);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(via_op[i], via_equal[i]) << "equal() vs operator== mismatch at lane " << i;
    }
    // data() round-trip through the value_type constructor.
    const Uint8x16 rebuilt{v.data()};
    uint8_t rebuilt_out[16]{};
    extract(rebuilt, rebuilt_out);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(buf[i], rebuilt_out[i]) << "data() round-trip mismatch at lane " << i;
    }
}

TEST(Uint8x16, CopyConstructorAndAssignment)
{
    Uint8x16 original{0x55};

    // Explicit copy constructor.
    const Uint8x16 copy_ctor{original};
    uint8_t out[16]{};
    extract(copy_ctor, out);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(0x55, out[i]) << "copy ctor mismatch at lane " << i;
    }
    // Copy assignment.
    Uint8x16 copy_assign{0};
    copy_assign = original;
    extract(copy_assign, out);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(0x55, out[i]) << "copy assign mismatch at lane " << i;
    }
    // Overwrite original: copies must remain 0x55.
    original = Uint8x16{0xAA};
    extract(copy_ctor, out);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(0x55, out[i]) << "copy ctor changed after source mutation at lane " << i;
    }
    extract(copy_assign, out);
    for (int i = 0; i < 16; ++i)
    {
        EXPECT_EQ(0x55, out[i]) << "copy assign changed after source mutation at lane " << i;
    }
}