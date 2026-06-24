//
// x86 SSE4.1 backend for Uint8x16 / ChecksumAccumulator.
// Included by Uint8x16.hpp — do not include directly.
//

#if !defined(__x86_64__) && !defined(_M_X64)
#error "Uint8x16_sse.inl requires x86-64 with SSE4.1"
#endif

#include <concepts>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <immintrin.h>

namespace org::limitless::fix::detail::simd {

struct Uint8x16
{
    typedef __m128i value_type;

    static uint32_t constexpr Size = 16;

    static inline const value_type Ones = _mm_set1_epi8(-1);
    static inline const value_type Zeros = _mm_setzero_si128();

    static inline const value_type AsciiZeros = _mm_set1_epi8('0');
    static inline const value_type AsciiNines = _mm_set1_epi8('9');

    value_type m_block{};

    Uint8x16() : Uint8x16{static_cast<uint8_t>(0)}
    {
    }

    explicit Uint8x16(const Uint8x16& block)
    {
        m_block = block.m_block;
    }

    explicit Uint8x16(const value_type block) : m_block(block)
    {
    }

    /**
     * Broadcasts a single byte across all 16 lanes.
     * @param filler byte
     */
    explicit Uint8x16(const uint8_t filler)
    {
        m_block = _mm_set1_epi8(static_cast<char>(filler));
    }

    /**
     * Broadcasts a 16-bit value across all 8 lanes. Constrained to exact
     * uint16_t arguments so int/char literals keep selecting the byte-broadcast
     * constructor.
     * @param filler 16-bit value
     */
    explicit Uint8x16(const std::same_as<uint16_t> auto filler)
    {
        m_block = _mm_set1_epi16(static_cast<short>(filler));
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
            m_block = _mm_loadu_si128(reinterpret_cast<const __m128i*>(buffer));
        }
        else [[unlikely]]
        {
            alignas(16) uint8_t aligned[16] = {0};
            for (size_t i = 0; i < length; ++i)
            {
                aligned[i] = buffer[i];
            }
            m_block = _mm_load_si128(reinterpret_cast<const __m128i*>(aligned));
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
            m_block = _mm_loadu_si128(reinterpret_cast<const __m128i*>(buffer));
        }
        else [[unlikely]]
        {
            alignas(16) uint16_t temp[8] = {0};
            for (size_t i = 0; i < length / sizeof(uint16_t); ++i)
            {
                temp[i] = buffer[i];
            }
            m_block = _mm_load_si128(reinterpret_cast<const __m128i*>(temp));
        }
        return *this;
    }

    /**
     * Loads 16 bytes from memory without a length check.
     * @param buffer data, at least 16 valid bytes
     */
    void load(const uint8_t* buffer)
    {
        m_block = _mm_loadu_si128(reinterpret_cast<const __m128i*>(buffer));
    }

    /**
     * Stores 16 bytes to memory.
     * @param position offset into buffer
     * @param buffer destination, at least position + 16 bytes
     * @return this vector
     */
    const Uint8x16& get(const size_t position, uint8_t* buffer) const
    {
        _mm_storeu_si128(reinterpret_cast<__m128i*>(buffer + position), m_block);
        return *this;
    }

    Uint8x16& operator|=(const Uint8x16& block)
    {
        m_block = _mm_or_si128(m_block, block.m_block);
        return *this;
    }

    Uint8x16 operator|(const Uint8x16& block) const
    {
        return Uint8x16{_mm_or_si128(this->m_block, block.m_block)};
    }

    Uint8x16& operator&=(const Uint8x16& block)
    {
        m_block = _mm_and_si128(m_block, block.m_block);
        return *this;
    }

    Uint8x16 operator&(const Uint8x16& block) const
    {
        return Uint8x16{_mm_and_si128(this->m_block, block.m_block)};
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
        return Uint8x16(_mm_bsrli_si128(m_block, N));
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
        return Uint8x16(_mm_bslli_si128(m_block, N));
    }

    Uint8x16 operator-(const Uint8x16& block) const
    {
        return Uint8x16(_mm_sub_epi8(m_block, block.m_block));
    }

    Uint8x16& operator-=(const Uint8x16& block)
    {
        m_block = _mm_sub_epi8(m_block, block.m_block);
        return *this;
    }

    Uint8x16 operator+(const Uint8x16& block) const
    {
        return Uint8x16(_mm_add_epi8(m_block, block.m_block));
    }

    Uint8x16& operator+=(const Uint8x16& block)
    {
        m_block = _mm_add_epi8(m_block, block.m_block);
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
        return Uint8x16{_mm_cmpeq_epi8(this->m_block, block.m_block)};
    }

    /**
     * Unsigned byte-wise greater-or-equal. Returns 0xFF per lane where this >= block.
     */
    Uint8x16 operator>=(const Uint8x16& block) const
    {
        return Uint8x16{_mm_cmpeq_epi8(_mm_max_epu8(m_block, block.m_block), m_block)};
    }

    /**
     * Unsigned byte-wise less-or-equal. Returns 0xFF per lane where this <= block.
     */
    Uint8x16 operator<=(const Uint8x16& block) const
    {
        return Uint8x16{_mm_cmpeq_epi8(_mm_min_epu8(m_block, block.m_block), m_block)};
    }

    /**
     * Horizontal sum of all 16 byte lanes.
     * @return sum of all lanes, 0..4080
     */
    [[nodiscard]] uint64_t sum() const
    {
        const __m128i sad = _mm_sad_epu8(m_block, Zeros);
        const __m128i total = _mm_add_epi64(sad, _mm_bsrli_si128(sad, 8));
        return static_cast<uint64_t>(_mm_cvtsi128_si64(total));
    }

    /**
     * @return the underlying register value
     */
    [[nodiscard]] value_type data() const
    {
        return m_block;
    }

    /**
     * Byte-wise select: where this mask byte is 0xFF, take from block; elsewhere zero.
     * @param block values to pass through where the mask is set
     * @return selected vector
     */
    [[nodiscard]] Uint8x16 whenTrue(const Uint8x16& block) const
    {
        return Uint8x16{_mm_blendv_epi8(Zeros, block.m_block, m_block)};
    }

    /**
     * Byte-wise equality with out parameter.
     * @param block vector to compare with
     * @param result mask vector, 0xFF per matching lane
     * @return this vector
     */
    Uint8x16& equal(const Uint8x16& block, Uint8x16& result)
    {
        result.m_block = _mm_cmpeq_epi8(m_block, block.m_block);
        return *this;
    }

    /**
     * Lane-wise 16-bit equality. Returns 0xFFFF per matching 16-bit lane.
     */
    [[nodiscard]] Uint8x16 equal(const Uint8x16& block) const
    {
        return Uint8x16{_mm_cmpeq_epi16(m_block, block.m_block)};
    }

    /**
     * Compresses the vector into a 64-bit mask with 4 bits per byte lane.
     * Nonzero lanes produce 0xF; zero lanes produce 0x0.
     * Compatible with the NEON nibble-mask format used by PayloadDecoder.
     * @return 64-bit nibble mask
     */
    [[nodiscard]] uint64_t toUint64() const
    {
        const __m128i isZero = _mm_cmpeq_epi8(m_block, Zeros);
        const __m128i nibbles = _mm_andnot_si128(isZero, _mm_set1_epi8(0x0F));
        const __m128i weights = _mm_set1_epi16(0x1001);
        const __m128i paired = _mm_maddubs_epi16(nibbles, weights);
        const __m128i packed = _mm_packus_epi16(paired, Zeros);
        return static_cast<uint64_t>(_mm_extract_epi64(packed, 0));
    }

    void print() const
    {
        alignas(16) uint8_t data[16];
        _mm_store_si128(reinterpret_cast<__m128i*>(data), m_block);
        for (int i = 0; i < 16; ++i)
        {
            std::printf("%02x ", data[i]);
        }
        std::putchar('\n');
    }

    static void print(const value_type& block)
    {
        alignas(16) uint8_t data[16];
        _mm_store_si128(reinterpret_cast<__m128i*>(data), block);
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
    __m128i m_accum = _mm_setzero_si128();

    /**
     * Adds the byte sums of a 16-byte block to the running accumulator.
     * @param block 16-byte vector to accumulate
     */
    void add(const Uint8x16& block)
    {
        const __m128i ones = _mm_set1_epi8(1);
        m_accum = _mm_add_epi16(m_accum, _mm_maddubs_epi16(block.m_block, ones));
    }

    /**
     * @return the accumulated checksum reduced to a single 32-bit value
     */
    [[nodiscard]] uint32_t value() const
    {
        const __m128i sum32 = _mm_madd_epi16(m_accum, _mm_set1_epi16(1));
        const __m128i hi64 = _mm_bsrli_si128(sum32, 8);
        const __m128i sum64 = _mm_add_epi32(sum32, hi64);
        const __m128i hi32 = _mm_bsrli_si128(sum64, 4);
        const __m128i total = _mm_add_epi32(sum64, hi32);
        return static_cast<uint32_t>(_mm_cvtsi128_si32(total));
    }
};

}
