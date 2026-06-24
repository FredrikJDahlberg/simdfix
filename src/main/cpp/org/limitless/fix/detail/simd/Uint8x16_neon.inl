//
// ARM NEON backend for Uint8x16 / ChecksumAccumulator.
// Included by Uint8x16.hpp — do not include directly.
//

#if !defined(__aarch64__) && !defined(_M_ARM64)
#error "Uint8x16_neon.inl requires ARM NEON (aarch64)"
#endif

#include <concepts>
#include <cstdio>

#include <arm_neon.h>

namespace org::limitless::fix::detail::simd {

struct Uint8x16
{
    typedef uint8x16_t value_type;

    static uint32_t constexpr Size = 16;

    static inline const value_type Ones = vdupq_n_u8(255);
    static inline const value_type Zeros = vdupq_n_u8(0);

    static inline const value_type AsciiZeros = vdupq_n_u8('0');
    static inline const value_type AsciiNines = vdupq_n_u8('9');

    value_type m_block{};

    Uint8x16() : Uint8x16{0}
    {
    }

    explicit Uint8x16(const Uint8x16& block)
    {
        m_block = block.m_block;
    }

    /// Wraps an existing NEON 128-bit register value.
    explicit Uint8x16(const value_type block) : m_block(block)
    {
    }

    /// Broadcasts a single byte across all 16 lanes (vdupq_n_u8).
    explicit Uint8x16(const uint8_t filler)
    {
        m_block = vdupq_n_u8(filler);
    }

    /// Broadcasts a 16-bit value across all 8 lanes (vdupq_n_u16).
    /// Constrained to exact uint16_t so int/char literals select the byte constructor.
    explicit Uint8x16(const std::same_as<uint16_t> auto filler)
    {
        m_block = vreinterpretq_u8_u16(vdupq_n_u16(filler));
    }

    /**
     * Loads up to 16 bytes from memory, zero-filling any trailing lanes.
     * @param buffer data
     * @param length valid bytes
     */
    explicit Uint8x16(const uint8_t* buffer, const size_t length)
    {
        put(buffer, length);
    }

    /**
     * Loads up to 16 bytes from memory, zero-filling any trailing lanes.
     * @param buffer data
     * @param length valid bytes
     * @return this vector
     */
    Uint8x16& put(const uint8_t* buffer, const size_t length)
    {
        if (length >= 16) [[likely]]
        {
            m_block = vld1q_u8(buffer);
        }
        else [[unlikely]]
        {
            alignas(16) uint8_t aligned[16] = {0};
            for (size_t i = 0; i < length; ++i)
            {
                aligned[i] = buffer[i];
            }
            m_block = vld1q_u8(aligned);
        }
        return *this;
    }

    /**
     * Loads up to 8 unsigned 16-bit values from memory, zero-filling trailing lanes.
     * @param buffer data
     * @param length valid bytes
     * @return this vector
     */
    Uint8x16& put(const uint16_t* buffer, const size_t length)
    {
        if (length >= 16) [[likely]]
        {
            m_block = vreinterpretq_u8_u16(vld1q_u16(buffer));
        }
        else [[unlikely]]
        {
            alignas(16) uint16_t temp[8] = {0};
            for (size_t i = 0; i < length / sizeof(uint16_t); ++i)
            {
                temp[i] = buffer[i];
            }
            m_block = vreinterpretq_u8_u16(vld1q_u16(temp));
        }
        return *this;
    }

    /**
     * Loads 16 bytes from memory without a length check.
     * @param buffer data, at least 16 valid bytes
     */
    void load(const uint8_t* buffer)
    {
        m_block = vld1q_u8(buffer);
    }

    /**
     * Stores 16 bytes to memory.
     * @param position offset into buffer
     * @param buffer destination, at least position + 16 bytes
     * @return this vector
     */
    const Uint8x16& get(const size_t position, uint8_t* buffer) const
    {
        vst1q_u8(buffer + position, m_block);
        return *this;
    }

    Uint8x16& operator|=(const Uint8x16& block)
    {
        m_block = vorrq_u8(m_block, block.m_block);
        return *this;
    }

    Uint8x16 operator|(const Uint8x16& block) const
    {
        return Uint8x16{vorrq_u8(this->m_block, block.m_block)};
    }

    Uint8x16& operator&=(const Uint8x16& block)
    {
        m_block = vandq_u8(m_block, block.m_block);
        return *this;
    }

    Uint8x16 operator&(const Uint8x16& block) const
    {
        return Uint8x16{vandq_u8(this->m_block, block.m_block)};
    }

    /**
     * Shifts lanes N positions towards lower indices, filling upper lanes with zeros.
     * @tparam N positions to shift, 0..15
     * @return shifted vector
     */
    template <int N>
    [[nodiscard]] Uint8x16 shiftLeft() const
    {
        static_assert(N >= 0 && N < 16, "Shift must be between 0 and 15");
        return Uint8x16(vextq_u8(m_block, Zeros, N));
    }

    /**
     * Shifts lanes N positions towards higher indices, filling lower lanes with zeros.
     * @tparam N positions to shift, 0..15
     * @return shifted vector
     */
    template <int N>
    [[nodiscard]] Uint8x16 shiftRight() const
    {
        static_assert(N >= 0 && N < 16, "Shift must be between 0 and 15");
        return Uint8x16(vextq_u8(Zeros, m_block, 16 - N));
    }

    Uint8x16 operator-(const Uint8x16& block) const
    {
        return Uint8x16(vsubq_u8(m_block, block.m_block));
    }

    Uint8x16& operator-=(const Uint8x16& block)
    {
        m_block = vsubq_u8(m_block, block.m_block);
        return *this;
    }

    Uint8x16 operator+(const Uint8x16& block) const
    {
        return Uint8x16(vaddq_u8(m_block, block.m_block));
    }

    Uint8x16& operator+=(const Uint8x16& block)
    {
        m_block = vaddq_u8(m_block, block.m_block);
        return *this;
    }

    Uint8x16& operator=(const value_type& vector)
    {
        m_block = vector;
        return *this;
    }

    Uint8x16& operator=(const Uint8x16& block)
    {
        if (&block != this)
        {
            m_block = block.m_block;
        }
        return *this;
    }

    /**
     * Byte-wise equality. Returns 0xFF per matching lane, 0x00 otherwise.
     */
    Uint8x16 operator==(const Uint8x16& block) const
    {
        return Uint8x16{vceqq_u8(this->m_block, block.m_block)};
    }

    /**
     * Unsigned byte-wise greater-or-equal. Returns 0xFF per lane where this >= block.
     */
    Uint8x16 operator>=(const Uint8x16& block) const
    {
        return Uint8x16{vcgeq_u8(m_block, block.m_block)};
    }

    /**
     * Unsigned byte-wise less-or-equal. Returns 0xFF per lane where this <= block.
     */
    Uint8x16 operator<=(const Uint8x16& block) const
    {
        return Uint8x16{vcleq_u8(m_block, block.m_block)};
    }

    /**
     * Horizontal sum of all 16 byte lanes.
     * @return sum of all lanes, 0..4080
     */
    [[nodiscard]] uint64_t sum() const
    {
        return vaddlvq_u8(m_block);
    }

    /**
     * @return the underlying register value
     */
    [[nodiscard]] value_type data() const
    {
        return m_block;
    }

    /**
     * Bitwise select: where this mask is set, take bits from block; elsewhere zero.
     * @param block values to pass through where the mask is set
     * @return selected vector
     */
    [[nodiscard]] Uint8x16 whenTrue(const Uint8x16& block) const
    {
        return Uint8x16{vbslq_u8(m_block, block.m_block, Zeros)};
    }

    /**
     * Byte-wise equality with out parameter.
     * @param block vector to compare with
     * @param result mask vector, 0xFF per matching lane
     * @return this vector
     */
    Uint8x16& equal(const Uint8x16& block, Uint8x16& result)
    {
        result.m_block = vceqq_u8(m_block, block.m_block);
        return *this;
    }

    /**
     * Lane-wise 16-bit equality. Returns 0xFFFF per matching 16-bit lane.
     */
    [[nodiscard]] Uint8x16 equal(const Uint8x16& block) const
    {
        return Uint8x16{vreinterpretq_u8_u16(
            vceqq_u16(vreinterpretq_u16_u8(m_block), vreinterpretq_u16_u8(block.m_block)))};
    }

    /**
     * Compresses the vector into a 64-bit mask with 4 bits per byte lane.
     * Nonzero lanes produce 0xF; zero lanes produce 0x0.
     * @return 64-bit nibble mask
     */
    [[nodiscard]] uint64_t toUint64() const
    {
        const uint8x16_t mask = vtstq_u8(m_block, m_block);
        const uint8x8_t narrowed = vshrn_n_u16(vreinterpretq_u16_u8(mask), 4);
        return vget_lane_u64(vreinterpret_u64_u8(narrowed), 0);
    }

    void print() const
    {
        print(m_block);
    }

    static void print(const value_type& block)
    {
        const auto data = reinterpret_cast<const uint8_t*>(&block);
        for (int i = 0; i < 16; ++i)
        {
            std::printf("%02x ", data[i]);
        }
        std::putchar('\n');
    }
};

/**
 * Accumulates a byte-level checksum across multiple 16-byte blocks using
 * pairwise widening, deferring the final horizontal reduction until value()
 * is called.
 */
struct ChecksumAccumulator
{
    uint16x8_t m_accum = vdupq_n_u16(0);

    /**
     * Adds the byte sums of a 16-byte block to the running accumulator.
     * @param block 16-byte vector to accumulate
     */
    void add(const Uint8x16& block)
    {
        m_accum = vaddq_u16(m_accum, vpaddlq_u8(block.m_block));
    }

    /**
     * @return the accumulated checksum reduced to a single 32-bit value
     */
    [[nodiscard]] uint32_t value() const
    {
        return vaddlvq_u16(m_accum);
    }
};

}
